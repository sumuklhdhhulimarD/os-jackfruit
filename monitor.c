#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/sched/signal.h>
#include <linux/mm.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#include "monitor_ioctl.h"

#define DEVICE_NAME "container_monitor"

// Limits in KB
#define DEFAULT_SOFT 40000   // 40 MB
#define DEFAULT_HARD 64000   // 64 MB

static int major;
static struct class *cls;
static struct cdev my_cdev;

// =======================
// PID STRUCT
// =======================
struct pid_entry {
    pid_t pid;
    unsigned long soft_limit;
    unsigned long hard_limit;
    int warned;
    struct list_head list;
};

static LIST_HEAD(pid_list);

// =======================
// IOCTL
// =======================
static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    if (cmd == REGISTER_PID) {
        pid_t pid;
        struct pid_entry *entry;

        if (copy_from_user(&pid, (pid_t *)arg, sizeof(pid_t)))
            return -EFAULT;

        entry = kmalloc(sizeof(*entry), GFP_KERNEL);

        entry->pid = pid;
        entry->soft_limit = DEFAULT_SOFT;
        entry->hard_limit = DEFAULT_HARD;
        entry->warned = 0;

        list_add(&entry->list, &pid_list);

        printk(KERN_INFO "[monitor] Registered PID: %d\n", pid);
    }

    return 0;
}

// =======================
// FILE OPS
// =======================
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = device_ioctl,
};

// =======================
// MONITOR THREAD
// =======================
static struct task_struct *monitor_thread;

static int monitor_fn(void *data)
{
    while (!kthread_should_stop()) {

        struct pid_entry *entry;

        list_for_each_entry(entry, &pid_list, list) {

            struct task_struct *task;
            struct mm_struct *mm;

            task = pid_task(find_vpid(entry->pid), PIDTYPE_PID);
            if (!task)
                continue;

            // safer direct access
            mm = task->mm;
            if (!mm)
                continue;

            // =======================
            // MEMORY CALCULATION
            // =======================
            unsigned long rss = get_mm_rss(mm);
            unsigned long total_vm = mm->total_vm;

            rss = rss * (PAGE_SIZE / 1024);
            total_vm = total_vm * (PAGE_SIZE / 1024);

            printk(KERN_INFO "[monitor] PID %d RSS: %lu KB | VM: %lu KB\n",
                   entry->pid, rss, total_vm);

            // =======================
            // SOFT LIMIT
            // =======================
            if (total_vm > entry->soft_limit && !entry->warned) {
                printk(KERN_WARNING "[monitor] PID %d exceeded soft limit (%lu KB)\n",
                       entry->pid, total_vm);
                entry->warned = 1;
            }

            // =======================
            // HARD LIMIT
            // =======================
            if (total_vm > entry->hard_limit) {
                printk(KERN_ALERT "[monitor] Killing PID %d (memory %lu KB)\n",
                       entry->pid, total_vm);

                send_sig(SIGKILL, task, 0);
            }
        }

        ssleep(2);
    }

    return 0;
}

// =======================
// INIT
// =======================
static int __init monitor_init(void)
{
    dev_t dev;

    alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
    major = MAJOR(dev);

    cdev_init(&my_cdev, &fops);
    cdev_add(&my_cdev, dev, 1);

    cls = class_create("monitor_class");
    device_create(cls, NULL, dev, NULL, DEVICE_NAME);

    monitor_thread = kthread_run(monitor_fn, NULL, "monitor_thread");

    printk(KERN_INFO "[monitor] Module loaded\n");
    return 0;
}

// =======================
// EXIT
// =======================
static void __exit monitor_exit(void)
{
    struct pid_entry *entry, *tmp;

    kthread_stop(monitor_thread);

    list_for_each_entry_safe(entry, tmp, &pid_list, list) {
        list_del(&entry->list);
        kfree(entry);
    }

    device_destroy(cls, MKDEV(major, 0));
    class_destroy(cls);
    unregister_chrdev_region(MKDEV(major, 0), 1);

    printk(KERN_INFO "[monitor] Module unloaded\n");
}

module_init(monitor_init);
module_exit(monitor_exit);

MODULE_LICENSE("GPL");
