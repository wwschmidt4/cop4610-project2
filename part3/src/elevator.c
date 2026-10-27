#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/list.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Pet elevator kernel module");

extern int (*STUB_start_elevator)(void);
extern int (*STUB_issue_request)(int, int, int);
extern int (*STUB_stop_elevator)(void);

//structur to represent elevator req
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


static int start_elevator(void)
{
	return 0;
}

static int issue_request(int start, int dest, int type)
{
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
	return 0;
}

static int stop_elevator(void)
{
	return 0;
}

// states for proc file
enum elevator_state { OFFLINE, IDLE, LOADING, UP, DOWN };
static const char * const state_names[] = {
    "OFFLINE", "IDLE", "LOADING", "UP", "DOWN"
};


static enum elevator_state state = OFFLINE;
static int current_floor = 1;

// fills the output each time proc file is read
static int elevator_show(struct seq_file *m, void *v)
{
    seq_printf(m, "Elevator state: %s\n", state_names[state]);
    seq_printf(m, "Current floor: %d\n", current_floor);
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