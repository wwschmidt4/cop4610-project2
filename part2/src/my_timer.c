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

static struct timespec64 start_time;
static bool first_read = true;

static int timer_show(struct seq_file *m, void *v)
{
    struct timespec64 curr_time;
    struct timespec64 elapsed;

    ktime_get_real_ts64(&curr_time);

    seq_printf(m, "Current time: %lld.%09ld seconds\n", (long long)curr_time.tv_sec, curr_time.tv_nsec);

    if (!first_read)
    {
        elapsed.tv_sec = curr_time.tv_sec - start_time.tv_sec;
        elapsed.tv_nsec = curr_time.tv_nsec - start_time.tv_nsec;

        if (elapsed.tv_nsec < 0)
        {
            elapsed.tv_sec--;
            elapsed.tv_nsec += 1000000000L;
        }

        seq_printf(m, "Elapsed time: %lld.%09ld seconds\n", (long long)elapsed.tv_sec, elapsed.tv_nsec);
    }
    else
    {
        seq_puts(m, "Elapsed time: N/A (first read)\n");
        first_read = false;
    }

    start_time = curr_time;
    return 0;
}

static int timer_open(struct inode *inode, struct file *file)
{
    return single_open(file, timer_show, NULL);
}

static const struct proc_ops timer_ops = 
{
    .proc_open = timer_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init timer_init(void)
{
    if (!proc_create("timer", 0444, NULL, &timer_ops))
    {
        pr_err("my_timer: Failed to create /proc/timer\n");
        return -ENOMEM;
    }

    pr_info("my_timer: Module loaded\n");
    return 0;
}

static void __exit timer_exit(void)
{
    remove_proc_entry("timer", NULL);
    pr_info("my_timer: Module unloaded\n");
}

module_init(timer_init);
module_exit(timer_exit);