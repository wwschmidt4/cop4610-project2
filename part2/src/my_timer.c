//Carter Venclauskas
//USE OF CHATGPT

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/timekeeping.h>
#include <linux/time64.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simple timer module.");

//stores time of prev read and tracks whether proc is being read for first time
static struct timespec64 start_time;
static bool first_read = true;

//calleed whnenver proc is read. Displays time and elapsed time.
static int timer_show(struct seq_file *m, void *v)
{
    struct timespec64 curr_time;
    struct timespec64 elapsed;

    //retrieve curr time
    ktime_get_real_ts64(&curr_time);

    print curr time
    seq_printf(m, "Current time: %lld.%09ld seconds\n", (long long)curr_time.tv_sec, curr_time.tv_nsec);

    //if not first time reading, then calc
    if (!first_read)
    {
        elapsed.tv_sec = curr_time.tv_sec - start_time.tv_sec;
        elapsed.tv_nsec = curr_time.tv_nsec - start_time.tv_nsec;

        //adjust nsec if difference is negative
        if (elapsed.tv_nsec < 0)
        {
            elapsed.tv_sec--;
            elapsed.tv_nsec += 1000000000L;
        }

        //print elapsed
        seq_printf(m, "Elapsed time: %lld.%09ld seconds\n", (long long)elapsed.tv_sec, elapsed.tv_nsec);
    }
    else
    {
        //no previous read. elapsed time cant be calc
        seq_puts(m, "Elapsed time: N/A (first read)\n");
        first_read = false;
    }

    //save curr time for next time
    start_time = curr_time;
    return 0;
}

//opens proc file and connects to timer_show func
static int timer_open(struct inode *inode, struct file *file)
{
    return single_open(file, timer_show, NULL);
}

//defines the operations supported by proc/timer
static const struct proc_ops timer_ops = 
{
    .proc_open = timer_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

//runs when kernel mod is loaded
static int __init timer_init(void)
{
    //create proc/timer entry with read only perms
    if (!proc_create("timer", 0444, NULL, &timer_ops))
    {
        pr_err("my_timer: Failed to create /proc/timer\n");
        return -ENOMEM;
    }
    //log message, successful mod loading
    pr_info("my_timer: Module loaded\n");
    return 0;
}

//runs when kernel mod is unloaded
static void __exit timer_exit(void)
{
    //remove proc/timer entry
    remove_proc_entry("timer", NULL);
    //log message that unloaded
    pr_info("my_timer: Module unloaded\n");
}

//register mod initialization and cleanup funcs
module_init(timer_init);
module_exit(timer_exit);
