#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Pet elevator kernel module");

// states for proc file
enum elevator_state { OFFLINE, IDLE, LOADING, UP, DOWN };
static const char * const state_names[] = {
    "OFFLINE", "IDLE", "LOADING", "UP", "DOWN"
};
static enum elevator_state state = OFFLINE;
static int current_floor = 1;
static bool stopping = false;  // true once stop_elevator() has been called and we're draining

// weight (lbs) and single-letter code per pet_type (0=Chihuahua,1=Pug,2=Pughuahua,3=Dachshund)
static const int pet_weights[] = {3, 14, 10, 16};
static const char pet_letters[] = {'C', 'P', 'H', 'D'};

// running totals for the proc file
static int pets_waiting = 0;   // total pets across all floor queues
static int pets_serviced = 0;  // total pets that have disembarked

// pointers to syscalls
extern int (*STUB_start_elevator)(void);
extern int (*STUB_issue_request)(int, int, int);
extern int (*STUB_stop_elevator)(void);

// stores the running thread so stop_elevator can signal it later
static struct task_struct *elevator_thread;

//structure to represent elevator req
struct elevator_request {
    int start_floor;
    int dest_floor;
    int pet_type;
    struct list_head list;
};

//floor lists for animals and elevator list for animals in elevator
#define NUM_FLOORS 5
static struct list_head floor_queues[NUM_FLOORS];
static LIST_HEAD(elevator_queue);

// main loop for the elevator thread
static int elevator_run(void *data)
{
   while (!kthread_should_stop()) 
   {
        if (stopping && list_empty(&elevator_queue)) 
		{
            state = OFFLINE;
            break;   // thread exits on its own
        }

        msleep(100);
    }
    elevator_thread = NULL;
    stopping = false;
    return 0;
}


static int start_elevator(void)
{
	// already running
	if (elevator_thread)
        return 1;

    state = IDLE;

	// launch the thread
    elevator_thread = kthread_run(elevator_run, NULL, "elevator_thread");
    if (IS_ERR(elevator_thread)) {
        elevator_thread = NULL;
        state = OFFLINE;
        return PTR_ERR(elevator_thread);
    }
	return 0;

}

static int issue_request(int start, int dest, int type)
{

	if (!elevator_thread || stopping) return 1;

	struct elevator_request *req;
	
	//if start out of bounds
	if (start < 1 || start > NUM_FLOORS) return 1;
	//if dest out of bounds
	if (dest < 1 || dest > NUM_FLOORS) return 1;
	//if same floor
	if (start == dest) return 1;
	//if not valid animal
	if (type < 0 || type > 3) return 1;

	req = kmalloc(sizeof(*req), GFP_KERNEL);
	if (!req) return -ENOMEM;
	//assign request params
	req->start_floor = start;
	req->dest_floor = dest;
	req->pet_type = type;
	INIT_LIST_HEAD(&req->list);
	
	list_add_tail(&req->list, &floor_queues[start - 1]);
	pets_waiting++;
	return 0;
}

static int stop_elevator(void)
{
	 if (!elevator_thread || stopping)
        return 1;   

    stopping = true;   // signal the thread to finish up and self-terminate
    return 0;
}

// fills the output each time proc file is read
static int elevator_show(struct seq_file *m, void *v)
{
    struct elevator_request *req;
    int current_load = 0;
    int floor;

    // compute current load by summing weights of pets in the elevator
    list_for_each_entry(req, &elevator_queue, list) 
	{
        current_load += pet_weights[req->pet_type];
    }

    seq_printf(m, "Elevator state: %s\n", state_names[state]);
    seq_printf(m, "Current floor: %d\n", current_floor);
    seq_printf(m, "Current load: %d lbs\n", current_load);

    // elevator status: 2-char code (letter + dest floor) per pet onboard
    seq_printf(m, "Elevator status: ");
    list_for_each_entry(req, &elevator_queue, list) 
	{
        seq_printf(m, "%c%d ", pet_letters[req->pet_type], req->dest_floor);
    }
    seq_printf(m, "\n");

    // per-floor breakdown, printed top floor to bottom floor
    for (floor = NUM_FLOORS; floor >= 1; floor--) 
	{
        int count = 0;
        char marker = (floor == current_floor) ? '*' : ' ';

        list_for_each_entry(req, &floor_queues[floor - 1], list) 
		{
            count++;
        }

        seq_printf(m, "[%c] Floor %d: %d ", marker, floor, count);
        list_for_each_entry(req, &floor_queues[floor - 1], list) 
		{
            seq_printf(m, "%c%d ", pet_letters[req->pet_type], req->dest_floor);
        }
        seq_printf(m, "\n");
    }

    seq_printf(m, "Number of pets: %d\n", pets_waiting);
    seq_printf(m, "Number of pets waiting: %d\n", pets_waiting);
    seq_printf(m, "Number of pets serviced: %d\n", pets_serviced);

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

// runs on elevator module load
static int __init elevator_init(void)
{
	for (int i = 0; i < NUM_FLOORS; i++) 
	{
		INIT_LIST_HEAD(&floor_queues[i]);
	}

	INIT_LIST_HEAD(&elevator_queue);

	if (!proc_create("elevator", 0444, NULL, &elevator_proc_ops)) return -ENOMEM;
	STUB_start_elevator = start_elevator;
	STUB_issue_request = issue_request;
	STUB_stop_elevator = stop_elevator;

	return 0;
}

//helper 4 exit
static void clear_pet_list(struct list_head *head)
{
	struct elevator_request *request, *next;

	list_for_each_entry_safe(request, next, head, list) 
	{
		list_del(&request->list);
		kfree(request);
	}
}

// runs on elevator module removal
static void __exit elevator_exit(void)
{
	STUB_start_elevator = NULL;
	STUB_issue_request = NULL;
	STUB_stop_elevator = NULL;

	// if the thread is still running when the module is removed
	if (elevator_thread) 
	{
		kthread_stop(elevator_thread);
		elevator_thread = NULL;
	}

	//call floor clear on every floor
	for (int i = 0; i < NUM_FLOORS; i++) clear_pet_list(&floor_queues[i]);

	clear_pet_list(&elevator_queue);
	remove_proc_entry("elevator", NULL);
}

static struct elevator_request *get_next_request(int floor)
{
    struct elevator_request *request;

	//if floor invalid
	if (floor < 1 || floor > NUM_FLOORS) return NULL;
	if (list_empty(&floor_queues[floor - 1])) return NULL;

    request = list_first_entry(&floor_queues[floor - 1], struct elevator_request, list);
    
	list_del(&request->list);

    return request;
}

module_init(elevator_init);
module_exit(elevator_exit);