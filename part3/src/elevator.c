#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");

extern int (*STUB_start_elevator)(void);
extern int (*STUB_issue_request)(int, int, int);
extern int (*STUB_stop_elevator)(void);

static int start_elevator(void)
{
	return 0;
}

static int issue_request(int start, int dest, int type)
{
	return 0;
}

static int stop_elevator(void)
{
	return 0;
}

static int __init elevator_init(void)
{
	STUB_start_elevator = start_elevator;
	STUB_issue_request = issue_request;
	STUB_stop_elevator = stop_elevator;
	return 0;
}

static void __exit elevator_exit(void)
{
	STUB_start_elevator = NULL;
	STUB_issue_request = NULL;
	STUB_stop_elevator = NULL;
}

module_init(elevator_init);
module_exit(elevator_exit);