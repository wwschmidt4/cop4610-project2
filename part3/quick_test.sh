#!/bin/bash
# quick_test.sh - fast local sanity check for the elevator module.
# Not part of the submission: keep it in part3/tests/ (already gitignored).
#
# Usage: bash quick_test.sh [num_pets] [run_seconds]
#   Copies the project from the VirtualBox shared folder, builds elevator.ko,
#   loads it, starts the elevator, issues pets, checks invariants every 2 s,
#   stops the elevator, waits for it to drain to OFFLINE, and unloads it.




#run this command in linux to test ---> bash /media/sf_cop4610-project2/part3/tests/quick_test.sh 40 20
NUM_PETS=${1:-20}
RUN_SECS=${2:-60}
SRC=/media/sf_cop4610-project2
WORK=~/p2-test
PROC=/proc/elevator
FAILS=0

fail() { echo "  !! FAIL: $*"; FAILS=$((FAILS + 1)); }

field() { # field <label regex> -> value after ": " on the matching proc line
    awk -F': ' -v re="$1" '$0 ~ re { print $2; exit }' "$PROC"
}

snapshot() { # prints one status line and checks invariants
    local state floor load onboard_list onboard pets waiting serviced
    state=$(field '^Elevator state')
    floor=$(field '^Current floor')
    load=$(field '^Current load' | awk '{print $1}')
    onboard_list=$(field '^Elevator status')
    onboard=$(echo "$onboard_list" | wc -w)
    pets=$(field '^Number of pets:')
    waiting=$(field '^Number of pets waiting')
    serviced=$(field '^Number of pets serviced')

    printf '%-8s floor %s | load %3s lbs | onboard %s [%s] | waiting %3s | serviced %3s\n' \
        "$state" "$floor" "$load" "$onboard" "$onboard_list" "$waiting" "$serviced"

    [ "$load" -le 50 ] || fail "load $load lbs is over 50"
    [ "$onboard" -le 5 ] || fail "$onboard pets on board (max 5)"
    [ "$onboard" -eq "$pets" ] || fail "'Number of pets' ($pets) != pets listed on board ($onboard)"
    [ $((waiting + onboard + serviced)) -eq "$ACCEPTED" ] ||
        fail "waiting+onboard+serviced = $((waiting + onboard + serviced)), expected $ACCEPTED"
    LAST_STATE=$state
    LAST_ONBOARD=$onboard
    LAST_WAITING=$waiting
}

echo "== Copying project from $SRC"
[ -d "$SRC" ] || { echo "Shared folder $SRC not found (see setup steps)."; exit 1; }
rm -rf "$WORK" && cp -r "$SRC" "$WORK" || exit 1
find "$WORK" -type f \( -name '*.c' -o -name '*.h' -o -name 'Makefile' \) -exec sed -i 's/\r$//' {} +

echo "== Building elevator.ko"
cd "$WORK/part3" || exit 1
make clean >/dev/null 2>&1
make > build.log 2>&1
grep -E "warning|error" build.log && echo "(compiler output above)"
[ -f src/elevator.ko ] || { echo "BUILD FAILED - see $WORK/part3/build.log"; exit 1; }

echo "== Building producer/consumer"
PRODUCER=$(find "$WORK/part3/tests" -name producer.c | head -1)
CONSUMER=$(find "$WORK/part3/tests" -name consumer.c | head -1)
[ -n "$PRODUCER" ] && [ -n "$CONSUMER" ] || { echo "producer.c/consumer.c not found in tests/"; exit 1; }
echo "  using $PRODUCER"
gcc -o /tmp/producer "$PRODUCER" && gcc -o /tmp/consumer "$CONSUMER" || exit 1

sudo rmmod elevator 2>/dev/null

SYSCHECK_SRC=$(find "$WORK/part3/tests" -name syscheck.c | head -1)
if [ -n "$SYSCHECK_SRC" ]; then
    echo "== Checking syscalls with the starter syscheck module"
    (
        cd "$(dirname "$SYSCHECK_SRC")" || exit 1
        make > syscheck_build.log 2>&1 || { echo "syscheck build failed"; exit 1; }
        sudo rmmod syscheck 2>/dev/null
        sudo insmod syscheck.ko || exit 1
        ./test-syscalls | tee /tmp/syscheck.log
        sudo rmmod syscheck
    ) || fail "syscheck step failed"
    grep -q "does not exist" /tmp/syscheck.log && fail "syscalls missing from the kernel"
fi

echo "== Loading module"
sudo dmesg -C
sudo insmod src/elevator.ko || exit 1
[ "$(field '^Elevator state')" = "OFFLINE" ] || fail "initial state is not OFFLINE"

echo "== Starting elevator and issuing $NUM_PETS pets"
/tmp/consumer --start
echo "-- second start (should report already running):"
/tmp/consumer --start
/tmp/producer "$NUM_PETS" > /tmp/producer.log
# lines look like: "Issue (3, 6, 2) returned 1"
ACCEPTED=$(grep -c "returned 0$" /tmp/producer.log)
REJECTED=$(grep -c "returned 1$" /tmp/producer.log)
echo "  $ACCEPTED accepted, $REJECTED rejected (the official producer uses floor 6 on purpose)"
BAD=$(awk '/^Issue/ { gsub(/[(),]/, ""); s = $2; d = $3; r = $6;
                      valid = (s >= 1 && s <= 5 && d >= 1 && d <= 5);
                      if ((valid && r != 0) || (!valid && r != 1)) { print; n++ } }
           END { exit n > 0 }' /tmp/producer.log) || fail "wrong issue_request return values: $BAD"

echo "== Running for ${RUN_SECS}s"
for ((t = 0; t < RUN_SECS; t += 2)); do
    snapshot
    sleep 2
done

echo "== Stopping elevator (should drain riders, board nobody new)"
/tmp/consumer --stop
# read the waiting count right after stop returns; no pet may board from here on
WAITING_AT_STOP=$(field '^Number of pets waiting')
echo "-- second stop (should report already stopping):"
/tmp/consumer --stop
for ((t = 0; t < 120; t += 2)); do
    snapshot
    [ "$LAST_WAITING" -eq "$WAITING_AT_STOP" ] || fail "pets boarded after stop was requested"
    [ "$LAST_STATE" = "OFFLINE" ] && break
    sleep 2
done
[ "$LAST_STATE" = "OFFLINE" ] || fail "elevator never reached OFFLINE within 120 s of stop"
[ "$LAST_ONBOARD" -eq 0 ] || fail "pets still on board after OFFLINE"

echo "== Final /proc/elevator"
cat "$PROC"

echo "== Unloading module"
sudo rmmod elevator || fail "rmmod failed"
lsmod | grep -q '^elevator' && fail "module still loaded"
if sudo dmesg | grep -iE "bug|oops|warning|call trace"; then
    fail "kernel log shows problems (above)"
fi

echo
if [ "$FAILS" -eq 0 ]; then echo "ALL CHECKS PASSED"; else echo "$FAILS CHECK(S) FAILED"; fi