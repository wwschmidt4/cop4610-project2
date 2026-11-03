# Elevator

This project extends the Linux kernel with a simulated pet elevator system. It includes system call tracing, a custom kernel timer module, and an elevator implementation using kernel threads, linked lists, mutexes, and a scheduling algorithm. The elevator transports pets between floors while enforcing capacity, weight, and movement constraints.

## Group Members
- Will Schmidt: cws22b@fsu.edu
- Aleksandar Stavreski: as21em@fsu.edu
- Carter Venclauskas: cv24i@fsu.edu

## Division of Labor

### Part 1: System Call Tracing
- **Responsibilities**: Write C programs to trace and compare system calls.
- **Assigned to**: Will Schmidt

### Part 2: Timer Kernel Module
- **Responsibilities**: Implementing my_timer kernel module. Create and remove proc entries on load and unload.
- **Assigned to**: Carter Venclauskas

### Part 3a: Adding System Calls
- **Responsibilities**: Add the elevator system calls to the kernel
- **Assigned to**: Will Schmidt

### Part 3b: Kernel Compilation
- **Responsibilities**: Compiling custom kernel
- **Assigned to**: Carter Venclauskas, Will Schmidt, Aleksandar Stavreski (All separate VMs)

### Part 3c: Threads
- **Responsibilities**: Implement elevator kthread for movement mechanics
- **Assigned to**: Will Schmidt

### Part 3d: Linked List
- **Responsibilities**: Implemented linked list for elevator
- **Assigned to**: Carter Venclauskas

### Part 3e: Mutexes
- **Responsibilities**: Implement mutex locking around all shared elevator states to prevent race conditions between the kthread and syscall handlers.
- **Assigned to**: Aleksandar Stavreski

### Part 3f: Scheduling Algorithm
- **Responsibilities**: Implement the logic that determines which floor the elevator services next, handling pickups and drop-offs 
- **Assigned to**: Aleksandar Stavreski

## File Listing
```
elevator/
├── Makefile
├── part1/
│   ├── empty.c
│   ├── empty.trace
│   ├── part1.c
│   ├── part1.trace
│   └── Makefile
├── part2/
│   ├── src/
│   └── Makefile
├── part3/
│   ├── src/
│   ├── Makefile
│   └── syscalls.c
├── Makefile
└── README.md

```
# How to Compile & Execute

### Requirements
- **Compiler**: `gcc`

## Part 1

### Compilation
```bash
make
```
This will build the empty and part1 executables in the part1/ directory.
### Execution
```bash
make run
```
This will run both executables under strace, generating empty.trace and part1.trace to record their system calls.

## Part 2

### Compilation
For a C/C++ example:
```bash
cd part2
make
```
This will build the kernel module `my_timer.ko`.
### Execution
```bash
sudo insmod my_timer.ko
cat /proc/timer
sudo rmmod my_timer
```
This will load the module, read the current time (and elapsed time since the last read), and unload the module when done.

## Part 3a System Calls (`syscalls.c` and its Makefile)

These two files are not built from this repository. They are copied into the kernel
source tree and compiled into the kernel.

1. Create a `syscalls/` directory at the top of the kernel source tree.
2. Copy `syscalls.c` and create a makefile with the one line:
   `obj-y := syscalls.o`
3. Add `syscalls/` to the kernel's top-level build (`obj-y += syscalls/` in `Kbuild`, or
   the `core-y` line in the top-level `Makefile` on older kernels).
4. Make the changes your architecture needs to register the system calls as 548, 549
   and 550.
   - x86_64: add entries to `arch/x86/entry/syscalls/syscall_64.tbl`.
   - arm64: add entries to `arch/arm64/tools/syscall_64.tbl`.
5. Rebuild and install the kernel, then reboot.

`syscalls.c` only forwards each call to the `elevator` module through exported function
pointers. The system calls return `-ENOSYS` until `elevator.ko` is loaded.

## Part 3: Elevator Kernel Module

### Build

$ cd part3
$ make


### Load

$ sudo insmod src/elevator.ko


### Run

$ ./tests/elevator-test/consumer --start
$ ./tests/elevator-test/producer [number_of_pets]
$ cat /proc/elevator
$ ./tests/elevator-test/consumer --stop


### Unload

$ sudo rmmod elevator

## Development Log
Each member records their contributions here.

### Will Schmidt

| Date       | Work Completed / Notes |
|------------|------------------------|
| 2026-10-15 | Implemented part 1     |
| 2026-10-16 | Implemented part 3a    |
| 2026-10-19 | Basic proc file code added  |
| 2026-10-27 | Implemented thread skeleton |
| 2026-10-28 | Implemented thread logic (3c complete) |


### [Member 2]

| Date       | Work Completed / Notes |
|------------|------------------------|
| 2026-10-29 | Implemented parts 3e and 3f  |


### Carter Venclauskas

| Date       | Work Completed / Notes |
|------------|------------------------|
| 2026-10-24 | Implemented timer module, compiled personal custom kernel, basic linked list  |
| 2026-10-27 | Fully refined and implemented linked list to match the project instructions.  |


## Meetings
Document in-person meetings, their purpose, and what was discussed.

| Date       | Attendees            | Topics Discussed | Outcomes / Decisions |
|------------|----------------------|------------------|-----------------------|
| 2026-10-12 | Will, Aleksandar, Carter              | Discussed division of labor and established our schedules   | Labor was divided succesfully  |
