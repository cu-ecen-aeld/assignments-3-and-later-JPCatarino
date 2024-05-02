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
#include <linux/slab.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
#include "aesd_ioctl.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Jorge Catarino");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev;
    
    PDEBUG("open");

    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
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
    
    struct aesd_dev *dev = filp->private_data;

    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

    if (mutex_lock_interruptible(&dev->lock)) {
        return -ERESTARTSYS;
    }

    cmd_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->cb, *f_pos, &cmd_offset);

    if (!cmd_entry) {
        PDEBUG("No data to read");
        retval = 0; 
        goto unlock_out;
    }

    chars_read = min(cmd_entry->size - cmd_offset, count);

    if (chars_read <= 0) {
        retval = 0; 
        goto unlock_out;
    }

    if (copy_to_user(buf, &(cmd_entry->buffptr[cmd_offset]), chars_read)) {
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
    struct aesd_buffer_entry new;
    char *newline;
    size_t char_to_write = 0;
    ssize_t retval = -ENOMEM;
    
    struct aesd_dev *dev = filp->private_data;


    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    if (mutex_lock_interruptible(&dev->lock)) {
        return -ERESTARTSYS;
    }

    if(!dev->buf){
        dev->buf = kmalloc(count, GFP_KERNEL);

        if (!dev->buf) {
            retval = -ENOMEM;
            goto unlock_out;
        }

        memset(dev->buf, 0, count);

        char_to_write = copy_from_user(dev->buf, buf, count);

        retval = count - char_to_write;
        dev->buf_size = retval;
    }
    else{
        char* extend_buf = (char*) krealloc(dev->buf, dev->buf_size + count, GFP_KERNEL);

        if (!extend_buf) {
            retval = -ENOMEM;
            goto unlock_out;
        }

        dev->buf = extend_buf;

        memset(&dev->buf[dev->buf_size], 0, count);

        char_to_write = copy_from_user(dev->buf + dev->buf_size, buf, count);

        retval = count - char_to_write;
        dev->buf_size += retval;
    }

    newline = memchr(dev->buf, '\n', dev->buf_size);

    if(newline){

        if (dev->cb.full) {
            uint8_t curr_offset = dev->cb.in_offs;
            if (dev->cb.entry[curr_offset].buffptr != NULL) {
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
    else {
        PDEBUG("partial write %s with %zu bytes", dev->buf, dev->buf_size);
    }

    dev->cb_size += retval;

unlock_out:
    mutex_unlock(&dev->lock);
    PDEBUG("write retval %ld", retval);
    return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t off, int whence){
    struct aesd_dev *dev = filp->private_data;
    loff_t retval;

    if (mutex_lock_interruptible(&dev->lock)) {
        return -ERESTARTSYS;
    }

    retval = fixed_size_llseek(filp, off, whence, dev->cb_size);

    if (retval < 0 || retval > dev->cb_size) {
        retval = -EINVAL;
        goto unlock_out;
    }

    PDEBUG("curr f_pos %lld new f_pos %lld", filp->f_pos, retval);

    filp->f_pos = retval;

unlock_out:
    mutex_unlock(&dev->lock);  
    PDEBUG("seek retval %lld", retval);
    return retval;
}

long aesd_adjust_file_offset(struct file *filp, unsigned int write_cmd, unsigned int write_cmd_offset){
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry *tmp;
    
    int i;
    loff_t off = 0;
    long retval = 0;

    PDEBUG("write_cmd %u write_cmd_offset %u", write_cmd, write_cmd_offset);

    if (write_cmd < 0 || write_cmd > AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED){
        return -EINVAL;
    }

    if (mutex_lock_interruptible(&dev->lock)) {
        return -ERESTARTSYS;
    }

    for (i = 0; i < write_cmd; i++) {
        tmp = &dev->cb.entry[i];
        if (tmp->buffptr != NULL && tmp->size > 0) {
            off += tmp->size;
        } 
        else{
            retval = -EINVAL;
            goto unlock_out;
        }
    }

    PDEBUG("After looping off %lld", off);

    tmp = &dev->cb.entry[write_cmd];
    if ((tmp->buffptr != NULL && tmp->size > 0) && (write_cmd_offset <= tmp->size)) {
        off += write_cmd_offset;
        PDEBUG("if valid off %lld", off);
    } 
    else {
        retval = -EINVAL;
        goto unlock_out;
    }

    filp->f_pos = off;
    
unlock_out:
    mutex_unlock(&dev->lock);  
    PDEBUG("adjust retval %ld", retval);
    return retval;
}

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
    struct aesd_seekto seekto;
    long retval;

    switch(cmd){
        case AESDCHAR_IOCSEEKTO:
        {
            if(copy_from_user(&seekto, (const void __user *) arg, sizeof(seekto)) != 0){
                retval = -EFAULT;
            } 
            else{
                retval = aesd_adjust_file_offset(filp, seekto.write_cmd, seekto.write_cmd_offset);
            }
            break;
        }
        default:
            retval = -EINVAL;
    }

    return retval;
}

struct file_operations aesd_fops = {
    .owner =            THIS_MODULE,
    .read =             aesd_read,
    .write =            aesd_write,
    .llseek =           aesd_llseek,
    .unlocked_ioctl =   aesd_ioctl,
    .open =             aesd_open,
    .release =          aesd_release,
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
    aesd_device.cb_size = 0;

    mutex_init(&aesd_device.lock);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    struct aesd_buffer_entry *tmp;
    struct aesd_circular_buffer *buffer = &aesd_device.cb;
    uint8_t index;
    
    dev_t devno = MKDEV(aesd_major, aesd_minor);
    

    cdev_del(&aesd_device.cdev);

    AESD_CIRCULAR_BUFFER_FOREACH(tmp, buffer, index) {
        if ((tmp->size > 0) && (tmp->buffptr != NULL)) {
            kfree(tmp->buffptr);
            tmp->size = 0;
        }
    }
    mutex_destroy(&aesd_device.lock);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
