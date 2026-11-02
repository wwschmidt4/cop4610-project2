/*
 * AI Use Disclosure:
 * Portions of this file (Part 3c: kthread lifecycle management, elevator
 * state machine, floor-to-floor movement timing, and pet boarding/unloading
 * logic; Part 3e: mutex locking around shared data; Part 3f: capacity
 * limits and the LOOK-style scheduling algorithm) were developed with the
 * assistance of Claude (Anthropic), an AI coding assistant, through an
 * interactive, step-by-step process involving design discussion, code
 * generation, and iterative testing. All AI-suggested code was reviewed,
 * tested, and verified by the author before inclusion.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Pet elevator kernel module");

#define NUM_FLOORS      5
#define MAX_PETS        5       // max pets on board at once
#define MAX_WEIGHT      50      // max lbs on board at once
#define MOVE_DELAY_MS   2000    // time to travel one floor
#define LOAD_DELAY_MS   1000    // time to load/unload at a floor
#define IDLE_POLL_MS    100     // how often an idle/offline elevator rechecks for work

// states for proc file
enum elevator_state { OFFLINE, IDLE, LOADING, UP, DOWN };
static const char * const state_names[] = {
    "OFFLINE", "IDLE", "LOADING", "UP", "DOWN"
};

// weight (lbs) and single-letter code per pet_type (0=Chihuahua,1=Pug,2=Pughuahua,3=Dachshund)
static const int pet_weights[] = {3, 14, 10, 16};
static const char pet_letters[] = {'C', 'P', 'H', 'D'};

// pointers to syscalls
extern int (*STUB_start_elevator)(void);
extern int (*STUB_issue_request)(int, int, int);
extern int (*STUB_stop_elevator)(void);

// structure to represent an elevator request (one pet)
struct elevator_request {
    int start_floor;
    int dest_floor;
    int pet_type;
    struct list_head list;
};

/*
 * Shared data. Everything below is touched by the elevator kthread, the three
 * syscalls, and the /proc reader, so it may only be read or written while
 * holding elevator_mutex. The mutex is never held across msleep(), so the
 * syscalls and /proc reads never block for the 1-2 second movement delays.
 */
static DEFINE_MUTEX(elevator_mutex);

static enum elevator_state state = OFFLINE;
static int current_floor = 1;
static int direction = 1;       // last direction traveled: +1 = up, -1 = down
static bool stopping;           // true once stop_elevator() is called and we're draining

//init one waiting queue per floor and one list for pets currently on elevator
static struct list_head floor_queues[NUM_FLOORS];
static LIST_HEAD(elevator_queue);

static int elevator_load;       // lbs currently on board
static int elevator_count;      // pets currently on board
static int pets_waiting;        // total pets across all floor queues
static int pets_serviced;       // total pets that have disembarked

// the elevator kthread; created on the first start_elevator(), stopped on rmmod
static struct task_struct *elevator_thread;

/* ---------- helpers: caller must hold elevator_mutex ---------- */

// true if a pet of this type can board without exceeding pet count or weight limits
static bool pet_fits(int pet_type)
{
    return elevator_count < MAX_PETS &&
           elevator_load + pet_weights[pet_type] <= MAX_WEIGHT;
}

// true if any pet on board wants to get off at this floor
static bool has_dropoff(int floor)
{
    struct elevator_request *req;

    //iterate through every pet currently on elevator to check destination
    list_for_each_entry(req, &elevator_queue, list) {
        if (req->dest_floor == floor)
            return true;
    }
    return false;
}

// true if the pet at the front of this floor's queue could board right now.
// while stopping, waiting pets are ignored so the elevator only drains.
static bool has_pickup(int floor)
{
    struct elevator_request *front;

    if (stopping || list_empty(&floor_queues[floor - 1]))
        return false;

    front = list_first_entry(&floor_queues[floor - 1], struct elevator_request, list);
    return pet_fits(front->pet_type);
}

// true if any floor strictly past current_floor in direction dir needs a stop
static bool work_in_direction(int dir)
{
    int floor;

    for (floor = current_floor + dir; floor >= 1 && floor <= NUM_FLOORS; floor += dir) {
        if (has_dropoff(floor) || has_pickup(floor))
            return true;
    }
    return false;
}

/*
 * LOOK scheduling: keep going in the current direction as long as there is
 * work ahead; otherwise turn around if there is work behind; otherwise idle.
 * Returns +1 (up), -1 (down), or 0 (nothing to do).
 */
static int pick_direction(void)
{
    if (work_in_direction(direction))
        return direction;
    if (work_in_direction(-direction))
        return -direction;
    return 0;
}

// unloads any pets whose destination is the current floor
static void unload_pets(void)
{
    struct elevator_request *req, *next;

    //use safe iterator bc current node might be removed/freed
    list_for_each_entry_safe(req, next, &elevator_queue, list) {
        if (req->dest_floor == current_floor) {
            list_del(&req->list);
            elevator_load -= pet_weights[req->pet_type];
            elevator_count--;
            pets_serviced++;
            kfree(req);
        }
    }
}

// loads waiting pets from the current floor onto the elevator, FIFO order
static void load_pets(void)
{
    struct elevator_request *req, *next;

    if (stopping)
        return;     // no new boardings once a stop has been requested

    list_for_each_entry_safe(req, next, &floor_queues[current_floor - 1], list) {
        if (!pet_fits(req->pet_type))
            break;  // FIFO: stop at the first pet that doesn't fit, don't skip ahead

        list_move_tail(&req->list, &elevator_queue);
        elevator_load += pet_weights[req->pet_type];
        elevator_count++;
        pets_waiting--;
    }
}

/* ---------- elevator kthread ---------- */

// main loop for the elevator kthread; runs until the module is removed
static int elevator_run(void *data)
{
    int dir;

    while (!kthread_should_stop()) {
        mutex_lock(&elevator_mutex);

        // not started (or fully stopped): wait for start_elevator()
        if (state == OFFLINE) {
            mutex_unlock(&elevator_mutex);
            msleep(IDLE_POLL_MS);
            continue;
        }

        // someone to drop off or pick up here: stop and load/unload
        if (has_dropoff(current_floor) || has_pickup(current_floor)) {
            state = LOADING;
            mutex_unlock(&elevator_mutex);
            msleep(LOAD_DELAY_MS);

            mutex_lock(&elevator_mutex);
            unload_pets();
            load_pets();
            mutex_unlock(&elevator_mutex);
            continue;
        }

        dir = pick_direction();

        // nothing to do anywhere; if stopping, the elevator is now empty
        if (dir == 0) {
            if (stopping) {
                state = OFFLINE;
                stopping = false;
            } else {
                state = IDLE;
            }
            mutex_unlock(&elevator_mutex);
            msleep(IDLE_POLL_MS);
            continue;
        }

        // move one floor (never wraps between 1 and 5)
        direction = dir;
        state = (dir > 0) ? UP : DOWN;
        mutex_unlock(&elevator_mutex);
        msleep(MOVE_DELAY_MS);

        mutex_lock(&elevator_mutex);
        current_floor += dir;
        mutex_unlock(&elevator_mutex);
    }

    return 0;
}

/* ---------- syscall implementations ---------- */

static int start_elevator(void)
{
    struct task_struct *thread;

    mutex_lock(&elevator_mutex);

    // already running (or still draining from a stop)
    if (state != OFFLINE) {
        mutex_unlock(&elevator_mutex);
        return 1;
    }

    // launch the thread the first time; it stays alive until rmmod
    if (!elevator_thread) {
        thread = kthread_run(elevator_run, NULL, "elevator_thread");
        if (IS_ERR(thread)) {
            mutex_unlock(&elevator_mutex);
            return PTR_ERR(thread);
        }
        elevator_thread = thread;
    }

    stopping = false;
    state = IDLE;
    mutex_unlock(&elevator_mutex);
    return 0;
}

static int issue_request(int start, int dest, int type)
{
    struct elevator_request *req;

    // reject out-of-range floors, same-floor trips, and unknown pet types
    if (start < 1 || start > NUM_FLOORS || dest < 1 || dest > NUM_FLOORS)
        return 1;
    if (start == dest)
        return 1;
    if (type < 0 || type >= ARRAY_SIZE(pet_weights))
        return 1;

    // allocate outside the lock; kmalloc(GFP_KERNEL) may sleep
    req = kmalloc(sizeof(*req), GFP_KERNEL);
    if (!req)
        return -ENOMEM;

    
    req->start_floor = start;
    req->dest_floor = dest;
    req->pet_type = type;
    //init nodes linked list pointers b4 inserting
    INIT_LIST_HEAD(&req->list);

    mutex_lock(&elevator_mutex);
    list_add_tail(&req->list, &floor_queues[start - 1]);
    pets_waiting++;
    mutex_unlock(&elevator_mutex);

    return 0;
}

static int stop_elevator(void)
{
    int ret = 0;

    mutex_lock(&elevator_mutex);
    if (state == OFFLINE || stopping)
        ret = 1;            // not running, or already deactivating
    else
        stopping = true;    // thread drains remaining pets, then goes OFFLINE
    mutex_unlock(&elevator_mutex);

    return ret;
}

/* ---------- /proc/elevator ---------- */

// fills the output each time proc file is read
static int elevator_show(struct seq_file *m, void *v)
{
    struct elevator_request *req;
    int floor;

    mutex_lock(&elevator_mutex);

    seq_printf(m, "Elevator state: %s\n", state_names[state]);
    seq_printf(m, "Current floor: %d\n", current_floor);
    seq_printf(m, "Current load: %d lbs\n", elevator_load);

    // elevator status: 2-char code (letter + dest floor) per pet on board
    seq_puts(m, "Elevator status: ");
    list_for_each_entry(req, &elevator_queue, list)
        seq_printf(m, "%c%d ", pet_letters[req->pet_type], req->dest_floor);
    seq_puts(m, "\n");

    // per-floor breakdown, printed top floor to bottom floor
    for (floor = NUM_FLOORS; floor >= 1; floor--) {
        int count = 0;
        char marker = (floor == current_floor) ? '*' : ' ';

        //count # of pets waiting on this floor
        list_for_each_entry(req, &floor_queues[floor - 1], list)
            count++;

        seq_printf(m, "[%c] Floor %d: %d ", marker, floor, count);
        list_for_each_entry(req, &floor_queues[floor - 1], list)
            seq_printf(m, "%c%d ", pet_letters[req->pet_type], req->dest_floor);
        seq_puts(m, "\n");
    }

    seq_printf(m, "Number of pets: %d\n", elevator_count);
    seq_printf(m, "Number of pets waiting: %d\n", pets_waiting);
    seq_printf(m, "Number of pets serviced: %d\n", pets_serviced);

    mutex_unlock(&elevator_mutex);
    return 0;
}

// runs when /proc/elevator is opened; sets elevator_show as its output function
static int elevator_open(struct inode *inode, struct file *file)
{
    return single_open(file, elevator_show, NULL);
}

// maps proc file operations to their handlers
static const struct proc_ops elevator_proc_ops = {
    .proc_open = elevator_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

/* ---------- module load / unload ---------- */

// frees every request in a list (used on module removal)
static void clear_pet_list(struct list_head *head)
{
    struct elevator_request *req, *next;
    
    list_for_each_entry_safe(req, next, head, list) {
        list_del(&req->list);
        kfree(req);
    }
}

// runs on elevator module load
static int __init elevator_init(void)
{
    int i;

    //init linked list head for each floors waiting queue
    for (i = 0; i < NUM_FLOORS; i++)
        INIT_LIST_HEAD(&floor_queues[i]);

    if (!proc_create("elevator", 0444, NULL, &elevator_proc_ops))
        return -ENOMEM;

    STUB_start_elevator = start_elevator;
    STUB_issue_request = issue_request;
    STUB_stop_elevator = stop_elevator;

    return 0;
}

// runs on elevator module removal
static void __exit elevator_exit(void)
{
    int i;

    // stop accepting syscalls and proc reads first
    STUB_start_elevator = NULL;
    STUB_issue_request = NULL;
    STUB_stop_elevator = NULL;
    remove_proc_entry("elevator", NULL);

    // stop the thread; must not hold the mutex here or the thread can't finish
    if (elevator_thread) {
        kthread_stop(elevator_thread);
        elevator_thread = NULL;
    }

    mutex_lock(&elevator_mutex);
    for (i = 0; i < NUM_FLOORS; i++)
        clear_pet_list(&floor_queues[i]);
    clear_pet_list(&elevator_queue);
    mutex_unlock(&elevator_mutex);
}

module_init(elevator_init);
module_exit(elevator_exit);
