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
#include "aesd_ioctl.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Your Name Here"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;
int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev = NULL; /* device information */

    PDEBUG("open");

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

    //take lock before reading the data into circular buffer
    if (mutex_lock_interruptible(&dev->buf_lock))
        return -ERESTARTSYS;

    rd_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer, *f_pos, &relative_pos);

    if (rd_entry == NULL) {
	mutex_unlock(&dev->buf_lock);
	PDEBUG("aesd_read: No data to send\r\n");
	return retval; //zero
    }

    //copy to user buff
    if (__copy_to_user(buf, (rd_entry->buffptr+relative_pos), rd_entry->size-relative_pos) == 0) {
        retval = rd_entry->size - relative_pos; 
	*f_pos = *f_pos+retval;
    } else {
	PDEBUG("aesd_read: failed to copy to user\r\n");
        //TODO: return value?
    }

    mutex_unlock(&dev->buf_lock);

    /**
     * TODO: handle read
     */
    PDEBUG("aesd_read: string %s read and its size id %d\r\n", rd_entry->buffptr, retval);
    return retval;
}


/** Function to handle the partial data memory.
 *  
 *  Accepts the old partial date buffer ptr and it's size and creates a new partial 
 *  buffer with new size and return the new partial data buffer ptr.
 *
 *  @return char ptr which is new partial data buff ptr.
 *           
 **/
char *handle_partial_data(char *partial_data_buf, int partial_mem_size, char *new_data_ptr, int new_size)
{
    void *buf;

    if (partial_data_buf == NULL) {
       return new_data_ptr;	
    } 
    
    buf = kmalloc(new_size, GFP_KERNEL);
    if (buf == NULL) { //error condition
	    //TODO: assert?
        return NULL;
    }

    //copy old  and new contents into newly allocated memory and free the old memory
    memcpy(buf, partial_data_buf, partial_mem_size);
    memcpy((buf+partial_mem_size), new_data_ptr, (new_size-partial_mem_size));

    kfree(partial_data_buf);

    return buf;
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
	PDEBUG("is_data_partial:partial pkt length error\r\n"); //TODO: improvise the error
    }

    return partial;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry wr_entry;
    char  *rm_buffptr = NULL; //removed entry
    char   *buffptr;
    bool   partial_data = 1;
    int    total_buf_size = 0;

    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    //check the write data size(is this check needed?)
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
    
    //check if data is \n terminated
    partial_data = is_data_partial(buffptr, count);
    if (partial_data == 0) {

	//take lock before writting the data into circular buffer/accessing the part_mem_ptr
	if (mutex_lock_interruptible(&dev->buf_lock)) {
	    kfree(buffptr);
            return 0;//-ERESTARTSYS;
	}

	// if there is data in partial buffer, handle that data
	if (dev->part_mem_ptr != NULL) {
            buffptr = handle_partial_data(dev->part_mem_ptr, 
			    dev->part_buf_size, buffptr, (dev->part_buf_size+count));
	    total_buf_size = dev->part_buf_size+count;
	    //reset partial buffer info
	    dev->part_mem_ptr = NULL;
	    dev->part_buf_size = 0;
	}

	PDEBUG(" aesd_write: FINAL BUFF contains %s and count is %d\r\n", buffptr, count);
	
	wr_entry.buffptr = buffptr;
	wr_entry.size = (total_buf_size != 0) ? total_buf_size:count;//handle partial data if any
	
	rm_buffptr = aesd_circular_buffer_add_entry(&dev->buffer, &wr_entry);
	mutex_unlock(&dev->buf_lock);

        if (rm_buffptr != NULL) { //overwritten entry, free it
	   kfree(rm_buffptr);
	}

	*f_pos = *f_pos + ((total_buf_size != 0) ? total_buf_size:count);//TODO:Come back and check this
	retval = count;
    } else { //partial data

    	//take lock before accessing the part_mem_ptr
	if (mutex_lock_interruptible(&dev->buf_lock)) {
    	   return 0;//-ERESTARTSYS;
	}

	dev->part_mem_ptr = handle_partial_data(dev->part_mem_ptr,           
                 dev->part_buf_size, buffptr, (dev->part_buf_size+count));
	PDEBUG(" aesd_write: CURR BUFF contains %s\r\n", dev->part_mem_ptr);
	dev->part_buf_size += count;
	mutex_unlock(&dev->buf_lock);

	retval = count;
    }

    /**
     * TODO: handle write
     */
    return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t offset, int whence)
{
   struct aesd_dev *dev = filp->private_data;
   loff_t cb_size = 0;

   switch (whence) {
       case SEEK_SET: 
       case SEEK_CUR: 
       case SEEK_END:

	  if (mutex_lock_interruptible(&dev->buf_lock)) {
             return -ERESTARTSYS;
	  }

          cb_size = dev->buffer.cb_size;

	  mutex_unlock(&dev->buf_lock);

	  PDEBUG(" aesd_llseek: CUR FPOS:%d CB_SIZE:%d SEEKING_OFFSET:%d\r\n", filp->f_pos, cb_size, offset);
          return fixed_size_llseek(filp, offset, whence, cb_size);
       break;

       default:
          return -EINVAL;
   }

   return -EINVAL;
}

static long int aesd_ioctl(struct file *filp, unsigned int cmd, long unsigned int arg)
{
    struct aesd_dev *dev = filp->private_data;
    struct aesd_seekto seekto;
    int retval = -1;
    
    switch (cmd) {
       case AESDCHAR_IOCSEEKTO:
    	 if (copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto)) != 0) {
             PDEBUG(" aesd_ioctl: failed to copy data from user memory\r\n");
	     retval = EFAULT;
         } else {
             if (mutex_lock_interruptible(&dev->buf_lock)) {
                return -ERESTARTSYS;
             }
             retval = aesd_adjust_file_offset(&dev->buffer, filp, seekto.write_cmd, seekto.write_cmd_offset);
	     mutex_unlock(&dev->buf_lock);
         }

	 break;
    }
    return retval; 
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .llseek =   aesd_llseek,
    .release =  aesd_release,
    .unlocked_ioctl = aesd_ioctl,
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

    mutex_init(&aesd_device.buf_lock);
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

    // take the lock before cleaning the buffers(maybe not needed here)
    if (mutex_lock_interruptible(&aesd_device.buf_lock)) {
	printk(KERN_ERR "Failed to acquire lock");
    }

    //cleanup the buffers (free the buffers)
    cleanup_circulat_buffers(&aesd_device.buffer);

    mutex_unlock(&aesd_device.buf_lock);
    mutex_destroy(&aesd_device.buf_lock);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
