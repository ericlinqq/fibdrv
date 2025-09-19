#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/version.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 92

static dev_t fib_dev = 0;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);
static int major = 0, minor = 0;
static ktime_t kt;

typedef long long (*fib_ft)(long long);

/*
 * Timing function for fib_sequence* function
 */
static long long fib_time_proxy(fib_ft fib, long long k)
{
    kt = ktime_get();
    long long res = fib(k);
    kt = ktime_sub(ktime_get(), kt);

    return res;
}


/**
 * fib_sequence() - Calculate the k-th Fibonacci number
 * @k:     Index of the Fibonacci number to calculate
 *
 * Return: The k-th Fibonacci number on success, -ENOMEM on memory allocation
 * failure.
 */
static long long fib_sequence(long long k)
{
    long long *f = kmalloc(sizeof(*f) * (k + 2), GFP_KERNEL);
    if (!f)
        return -ENOMEM;

    f[0] = 0;
    f[1] = 1;

    for (int i = 2; i <= k; i++) {
        f[i] = f[i - 1] + f[i - 2];
    }

    long long ret = f[k];

    kfree(f);

    return ret;
}

static long long fib_sequence_fdoubling(long long n)
{
    if (n <= 2)
        return !!n;

    long long a = 0;  // F(0)
    long long b = 1;  // F(1)

    /* get highest set bit */
    unsigned long long h = n >> 32 | n;
    h |= h >> 16;
    h |= h >> 8;
    h |= h >> 4;
    h |= h >> 2;
    h |= h >> 1;
    h ^= (h >> 1);

    for (; h; h >>= 1) {
        long long c = a * (2 * b - a);  // F(2k) = F(k) * [2 * F(k+1) - F(k)]
        long long d = a * a + b * b;  // F(2k+1) = F(k) * F(k) + F(k+1) * F(k+1)

        if (h & n) {
            a = d;      // F(n) = F(2k+1)
            b = c + d;  // F(n+1) = F(n-1) + F(n) = F(2k) + F(2k+1)
        } else {
            a = c;  // F(n) = F(2k)
            b = d;  // F(n+1) = F(2k+1)
        }
    }
    return a;
}

/* Calculate Fibonacci number using Fast Doubling */
static long long fib_sequence_fdoubling_clz(long long n)
{
    if (n <= 2)
        return !!n;

    long long a = 0;  // F(0)
    long long b = 1;  // F(1)

    for (unsigned int h = 1 << (31 - __builtin_clz(n)); h; h >>= 1) {
        long long c = a * (2 * b - a);  // F(2k) = F(k) * [2 * F(k+1) - F(k)]
        long long d = a * a + b * b;  // F(2k+1) = F(k) * F(k) + F(k+1) * F(k+1)

        if (h & n) {
            a = d;      // F(n) = F(2k+1)
            b = c + d;  // F(n+1) = F(n-1) + F(n) = F(2k) + F(2k+1)
        } else {
            a = c;  // F(n) = F(2k)
            b = d;  // F(n+1) = F(2k+1)
        }
    }
    return a;
}


static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use\n");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    return (ssize_t) fib_sequence_fdoubling(*offset);
}

/*
 * Use write operation for returning the time spent on
 * calculating the fibonacci number for given offset
 */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    switch (size) {
    case 0:
        fib_time_proxy(fib_sequence, *offset);
        break;
    case 1:
        fib_time_proxy(fib_sequence_fdoubling, *offset);
        break;
    case 2:
        fib_time_proxy(fib_sequence_fdoubling_clz, *offset);
        break;
    default:
        return 1;
    }
    return (ssize_t) ktime_to_ns(kt);
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;
    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = major = register_chrdev(major, DEV_FIBONACCI_NAME, &fib_fops);
    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev\n");
        rc = -2;
        goto failed_cdev;
    }
    fib_dev = MKDEV(major, minor);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    fib_class = class_create(DEV_FIBONACCI_NAME);
#else
    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);
#endif
    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class\n");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device\n");
        rc = -4;
        goto failed_device_create;
    }

    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
failed_cdev:
    unregister_chrdev(major, DEV_FIBONACCI_NAME);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    unregister_chrdev(major, DEV_FIBONACCI_NAME);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
