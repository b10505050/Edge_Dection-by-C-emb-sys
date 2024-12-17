#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include "edge_detection_ioctl.h"
#define DEVICE_NAME "edge_detection"
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/perf_event.h>
#include <asm/arch_timer.h> 
#define IMAGE_WIDTH  640
#define IMAGE_HEIGHT 480
#define IMAGE_SIZE   (IMAGE_WIDTH * IMAGE_HEIGHT)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple edge detection driver module for Raspberry Pi Camera");
MODULE_VERSION("0.1");
//PMU
static inline uint64_t read_cycle_counter(void) {
    uint64_t cycle_count;
    asm volatile("MRS %0, pmccntr_el0" : "=r"(cycle_count));
    return cycle_count;
}


static unsigned char *input_image;
static unsigned char *output_image;
static int major_number;
static struct class* edge_detection_class = NULL;
static struct device* edge_detection_device = NULL;
static int edge_detection_enabled = 1;
static DEFINE_MUTEX(edge_lock);

// 算法
static void edge_detection(const unsigned char *input, unsigned char *output, int width, int height) {
    int gx, gy;
//PMU start
    uint64_t start_cycles, end_cycles;

    start_cycles = read_cycle_counter(); 


    memset(output, 0, width * height);
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            gx = (-1 * input[(y-1)*width + (x-1)]) + (1 * input[(y-1)*width + (x+1)])
               + (-2 * input[(y  )*width + (x-1)]) + (2 * input[(y  )*width + (x+1)])
               + (-1 * input[(y+1)*width + (x-1)]) + (1 * input[(y+1)*width + (x+1)]);
            gy = (-1 * input[(y-1)*width + (x-1)]) + (-2 * input[(y-1)*width + (x  )]) + (-1 * input[(y-1)*width + (x+1)])
               + ( 1 * input[(y+1)*width + (x-1)]) + ( 2 * input[(y+1)*width + (x  )]) + ( 1 * input[(y+1)*width + (x+1)]);

            int magnitude = abs(gx) + abs(gy);
            if (magnitude > 255) {
                magnitude = 255;
            }
            output[y*width + x] = (magnitude > 255) ? 255 : magnitude;
        }
    }
//PMU end
    end_cycles = read_cycle_counter();  // 结束计数

    /*printk(KERN_INFO "Edge Detection PMU Profiling:\n");
    printk(KERN_INFO "Start Cycles: %llu\n", start_cycles);
    printk(KERN_INFO "End Cycles: %llu\n", end_cycles);
    */
    printk(KERN_INFO " name Total Cycles: %llu\n", end_cycles - start_cycles);
    printk(KERN_INFO "Edge Detection: Running algorithm on input buffer\n");


    /*for (int i = 0; i < 10; i++) {
    printk(KERN_INFO "Input Pixel [%d]: %d\n", i, input[i]);
}


    for (int i = 0; i < 10; i++) {
    printk(KERN_INFO "Output Pixel [%d]: %d\n", i, output[i]);
}
*/
}

static ssize_t edge_write(struct file *file, const char __user *buffer, size_t len, loff_t *offset) {
    if (!mutex_trylock(&edge_lock)) {
        printk(KERN_ALERT "Edge Detection: Device is busy\n");
        return -EBUSY;
    }

    *offset = 0; // 忽略偏移
    if (len > IMAGE_SIZE) {
        mutex_unlock(&edge_lock);
        return -EINVAL;
    }

    if (copy_from_user(input_image, buffer, len)) {
        mutex_unlock(&edge_lock);
        return -EFAULT;
    }

    if (edge_detection_enabled) {
        edge_detection(input_image, output_image, IMAGE_WIDTH, IMAGE_HEIGHT);
    } else {
        memcpy(output_image, input_image, IMAGE_SIZE);
    }printk(KERN_INFO "Edge Detection: Writing %zu bytes to input buffer\n", len);

    /*
    for (int i = 0; i < 10; i++) {
    printk(KERN_INFO "Input Buffer [%d]: %d\n", i, input_image[i]);
    }
    */

    mutex_unlock(&edge_lock);
    return len;
}

static ssize_t edge_read(struct file *file, char __user *buffer, size_t len, loff_t *offset) {
    size_t bytes_to_read;

    if (!mutex_trylock(&edge_lock)) {
        printk(KERN_ALERT "Edge Detection: Device is busy\n");
        return -EBUSY;
    }

 
    if (*offset >= IMAGE_SIZE) {
        mutex_unlock(&edge_lock);
        return 0;
    }

 
    bytes_to_read = min(len, (size_t)(IMAGE_SIZE - *offset));

    if (copy_to_user(buffer, output_image + *offset, bytes_to_read)) {
        mutex_unlock(&edge_lock);
        return -EFAULT;
    }
    printk(KERN_INFO "Edge Detection: Reading %zu bytes from output buffer\n", len);
/*

    for (int i = 0; i < 10; i++) {
    printk(KERN_INFO "Output Buffer [%d]: %d\n", i, output_image[i]);
}
*/ 
    *offset += bytes_to_read;

    mutex_unlock(&edge_lock);
    return bytes_to_read; 
}


static long edge_detection_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
//    printk(KERN_INFO "edge_detection_ioctl: cmd=0x%08x expected=0x%08lx\n", cmd, IOCTL_ENABLE_EDGE_DETECTION);

    switch (cmd) {
        case IOCTL_ENABLE_EDGE_DETECTION:
            if (copy_from_user(&edge_detection_enabled, (int __user *)arg, sizeof(int))) {
                return -EFAULT;
            }
            printk(KERN_INFO "Edge Detection: %s\n", edge_detection_enabled ? "Enabled" : "Disabled");
            break;
        default:
            return -ENOTTY;
    }
    return 0;
}


static struct file_operations fops = {
    .write = edge_write,
    .read = edge_read,
    .unlocked_ioctl = edge_detection_ioctl,
};

/************************************************************
 * 
 * 
 ************************************************************/

static int __init edge_detection_init(void) {
    printk(KERN_INFO "Edge Detection Module Init: Starting Initialization\n");
    printk(KERN_INFO "IOCTL_ENABLE_EDGE_DETECTION = 0x%08lx\n", (unsigned long)IOCTL_ENABLE_EDGE_DETECTION);


    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ALERT "Edge Detection failed to register a major number\n");
        return major_number;
    }
    printk(KERN_INFO "Edge Detection: Registered correctly with major number %d\n", major_number);


    edge_detection_class = class_create(DEVICE_NAME);
    if (IS_ERR(edge_detection_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "Failed to register device class\n");
        return PTR_ERR(edge_detection_class);
    }
    printk(KERN_INFO "Edge Detection: Device class registered successfully\n");

 
    edge_detection_device = device_create(edge_detection_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(edge_detection_device)) {
        class_destroy(edge_detection_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "Failed to create the device\n");
        return PTR_ERR(edge_detection_device);
    }
    printk(KERN_INFO "Edge Detection: Device created successfully\n");

 
    input_image = vmalloc(IMAGE_SIZE);
    output_image = vmalloc(IMAGE_SIZE);

    if (!input_image || !output_image) {
        printk(KERN_ALERT "Edge Detection: Failed to allocate memory for buffers\n");
        device_destroy(edge_detection_class, MKDEV(major_number, 0));
        class_unregister(edge_detection_class);
        class_destroy(edge_detection_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return -ENOMEM;
    }

  
    memset(input_image, 0, IMAGE_SIZE);
    memset(output_image, 0, IMAGE_SIZE);

    printk(KERN_INFO "Edge Detection: Memory buffers initialized\n");

    return 0;
}

static void __exit edge_detection_exit(void) {
  printk(KERN_INFO "Edge Detection Module Exit: Starting Cleanup\n");

 
    if (input_image) {
        vfree(input_image);
    }
    if (output_image) {
        vfree(output_image);
    }

   
    device_destroy(edge_detection_class, MKDEV(major_number, 0));
    class_unregister(edge_detection_class);
    class_destroy(edge_detection_class);
    unregister_chrdev(major_number, DEVICE_NAME);

    printk(KERN_INFO "Edge Detection Module Unloaded\n");
}



module_init(edge_detection_init);
module_exit(edge_detection_exit);