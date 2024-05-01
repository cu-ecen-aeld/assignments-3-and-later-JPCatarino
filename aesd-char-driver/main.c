/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Jorge Catarino");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    
    struct aesd_dev *dev;

    dev = containerof(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    struct aesd_buffer_entry *cmd_entry;
    size_t cmd_offset, chars_read;
    
    struct aesd_dev *dev = filp->private_data;;

    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    
    mutex_lock_interruptible(&dev->lock); 

    cmd_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->cb, *f_pos, &cmd_offset);

    if (!start_entry) {
        PDEBUG("No data to read");
        retval = 0; 
        goto unlock_out;
    }

    chars_read = min(start_entry->size - cmd_offset, count);

    if (chars_read <= 0) {
        retval = 0; 
        goto unlock_out;
    }

    if (copy_to_user(buf, &(start_entry->buffptr[start_entry_off]), chars_read)) {
        retval = -EFAULT; 
        goto unlock_out;
    }

    *f_pos += chars_read;
    retval = chars_read; 

unlock_out:
    mutex_unlock(&dev->lock);  
    PDEBUG("read retval %ld", retval);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    struct aesd_buffer_entry new;
    size_t char_to_write;
    
    struct aesd_dev *dev = filp->private_data;


    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    mutex_lock_interruptible(&dev->lock);

    if(!dev->buf){
        dev->buf = kmalloc(count, GFP_KERNEL);

        if (!dev->buf) {
            retval = -ENOMEM;
            goto unlock_out;
        }

        char_to_write = copy_from_user(dev->buf, buf, count)

        retval = count - char_to_write;
        dev->buf_size = retval;
    }
    else{
        char extend_buf = krealloc(dev->buf, dev->buf_size + count, GFP_KERNEL);

        if (!extend_buf) {
            retval = -ENOMEM;
            goto unlock_out;
        }

        dev->buf = extend_buf;

        memset(&dev->buf[dev->buf_size], 0, count);

        char_to_write = copy_from_user(dev->buf + dev->buf_size, buf, char_to_write);

        retval = count - char_to_write;
        dev->buf_size += retval;
    }

    char *newline = memchr(dev->buf, '\n', dev->buf_size);

    if(newline){

        if (dev->cb.full) {
            uint8_t curr_offset = dev->cb.in_offs;
            if ((cb.entry[curr_offset].size > 0) && (dev->cb.entry[curr_offset].buffptr != NULL)) {
                kfree(dev->cb.entry[curr_offset].buffptr);
                dev->cb.entry[curr_offset].size = 0;
            }
        }

        new.buffptr = dev->buf;
        new.size = dev->buf_size;
        
        aesd_circular_buffer_add_entry(&dev->cb, &new);

        dev->buf = NULL;
        dev->buf_size = 0;
    }

unlock_out:
    mutex_unlock(&dev->lock);
    PDEBUG("write retval %ld", retval);
    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    aesd_circular_buffer_init(&aesd_device.cb);
    aesd_device.buf = NULL;
    aesd_device.buf_size = 0;

    mutex_init(&aesd_device.lock);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    AESD_CIRCULAR_BUFFER_FOREACH(entry, buffer, index) {
        if ((entry->size > 0) && (entry->buffptr != NULL)) {
            kfree(entry->buffptr);
            entry->size = 0;
        }
    }
    mutex_destroy(&aesd_device.lock);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
