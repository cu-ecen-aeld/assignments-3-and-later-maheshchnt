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
#include <linux/slab.h>

#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Your Name Here"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");

    struct aesd_dev *dev; /* device information */

    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev; /* for other methods */

    /**
     * TODO: handle open
     */
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");

    filp->private_data = NULL;
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry *rd_entry = NULL;
    ssize_t relative_pos = 0; //relate fpos (ignore it for now)
    ssize_t retval = 0;

    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

    rd_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer, *f_pos, &relative_pos);
    if (rd_entry == NULL) {
	return retval; //zero
    }

    //copy to user buff
    if (__copy_to_user(buf, rd_entry->buffptr, rd_entry->size) == 0) {
        retval = rd_entry->size;
    } else {
	PDEBUG("aesd_read: failed to copy to user\r\n");
        //TODO: return value?
    }

    /**
     * TODO: handle read
     */
    return retval;
}

/** check if the given string is '\n' terminated.
 *
 *  @return 1 is not terminated
 *          0 if terminated 
 **/
bool is_data_partial(char *buffptr, size_t count)
{
    int n = 0;
    int len = 0;
    bool partial = 1;

    for (n = 0; n < count; n++) {
	len++;
        if (buffptr[n] == '\n') { //if '\n' is found, set partial flag to 0
	    partial = 0;
        }
    }

    if ((partial == 0) && (len != count)) {
	PDEBUG("is_data_partial:packet terminated in the middle of the data\r\n"); //TODO: improvise the error
    }

    return partial;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry *wr_entry;
    struct aesd_buffer_entry *rm_entry = NULL; //removed entry
    char   *buffptr;
    bool   partial_data = 1;

    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    //check the write data size
    if (count > 1024) {
        PDEBUG(" Size to be written is too large\r\n");	
	//TODO: return error ?
    }

    buffptr = kmalloc(count, GFP_KERNEL);
    if (buffptr == NULL) {
        PDEBUG(" aesd_write: Failed to allocate memory for data\r\n");
        return -ENOMEM;
    }

    //copy from user
    if (__copy_from_user(buffptr, buf, count) != 0) {
	PDEBUG(" aesd_write: failed to copy user data\r\n");
	//TODO: return value?
    }
    
    //check if \n terminated
    partial_data = is_data_partial(buffptr, count);
    if (partial_data == 0) {

	//FIXME: check if this write is happening after partial write. If yes, handle it
	//combine all the partial data ptrs and copy the data to buffptr
	//free the partial data ptr
	
    	//allocate the memory for entry
    	wr_entry = kmalloc(sizeof(struct aesd_buffer_entry), GFP_KERNEL);
    	if (wr_entry == NULL) {
	    PDEBUG(" aesd_write: Failed to allocate memory for entry\r\n");
            kfree(buffptr);	
	    return -ENOMEM;
    	}

	wr_entry->buffptr = buffptr;

	rm_entry = aesd_circular_buffer_add_entry(&dev->buffer, wr_entry);
        if (rm_entry != NULL) { //overwritten entry, free it
	   kfree(rm_entry->buffptr);
	   kfree(rm_entry);
	}

	retval = count;
    }

    /**
     * TODO: handle write
     */
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

    /**
     * TODO: initialize the AESD specific portion of the device
     */

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

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
