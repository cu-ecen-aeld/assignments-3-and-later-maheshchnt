/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#include <linux/slab.h>
#else
#include <string.h>
#include <stdio.h>
#endif

#include "aesd-circular-buffer.h"
#include "aesdchar.h"

/*
 * @brief Private function used only by this .c file.
 *
 * Given the pointer of the queue, this function calculates next pointer value.
 *
 * @param ptr (in) - ptr of the messsage queue
 *
 * @return uint32_t (out) - next ptr value
 */
static uint32_t nextPtr(uint32_t ptr) {

  if ((ptr + 1) >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) {
    return 0;
  }

  return (ptr+1);
} // nextPtr()


/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    struct aesd_buffer_entry *entry = NULL;
    int    char_pos = char_offset;
    int    next_out_index = 0;

    if (buffer != NULL) {
	// read and write pointers are same but full flag is not set (empty condition)
	if ((buffer->in_offs == buffer->out_offs) && (buffer->full == 0)) {
	    PDEBUG("I shouldn't be coming here. IN:%d OUT:%d FULL:%d\n\r", buffer->in_offs, buffer->out_offs, buffer->full);
            return NULL;
	}

	entry = &buffer->entry[buffer->out_offs];
	next_out_index = nextPtr(buffer->out_offs);
	
	// iterate over entries until char pos is found
	while (entry) {
	    if (entry->size > char_pos) {
		*entry_offset_byte_rtn = char_pos;
		return entry;
	    } else {
		char_pos -= entry->size;
		//read and write pointers are same-end of buffers
                if (next_out_index == buffer->in_offs) {
		    return NULL;
		}
		entry = &buffer->entry[next_out_index];
                next_out_index = nextPtr(next_out_index);
	    }

	}

    }
    /**
    * TODO: implement per description
    */
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
char *aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    char *remove_buffptr = NULL;
    int next_in_index;

    if (buffer != NULL) {
	//get the next write entry index
	next_in_index = nextPtr(buffer->in_offs);

	PDEBUG("writing %s in %d offsetn\n\r", add_entry->buffptr, buffer->in_offs);

    	if (buffer->full) {//if full, overwrite the buffer and return the ptr of entry that is overwritten 
	     remove_buffptr = (char *)buffer->entry[buffer->out_offs].buffptr;
	     buffer->out_offs = next_in_index;
    	} else if (next_in_index == buffer->out_offs) {
	     buffer->full = 1;
    	}

	//write the entry
        buffer->entry[buffer->in_offs] = *add_entry;

	PDEBUG("CUR_WR_PTR:%d NEXT_WR_PTR=%d CUR_RD_PTR=%d FULL=%d\n\r", buffer->in_offs, next_in_index, buffer->out_offs, buffer->full);

	buffer->in_offs = next_in_index;
    } 

    return remove_buffptr;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}


/*
 * Function to cleanup the circular buffers. Calling function has to take care of
 * locks.
 *
 */
void cleanup_circulat_buffers(struct aesd_circular_buffer *buffer)
{

   struct aesd_buffer_entry *entry = NULL;
   int    next_out_index = 0;

   if (buffer != NULL) {
	// read and write pointers are same but full flag is not set (empty condition)
	if ((buffer->in_offs == buffer->out_offs) && (buffer->full == 0)) {
		//empty condition:do nothing
	} else {

	   entry = &buffer->entry[buffer->out_offs];
	   next_out_index = nextPtr(buffer->out_offs);
	
	   // iterate over entries until char pos is found
	   while (entry) {
	       kfree(entry->buffptr);
		   //read and write pointers are same-end of buffers
                   if (next_out_index == buffer->in_offs) {
		       break;
		   }
		   entry = &buffer->entry[next_out_index];
                   next_out_index = nextPtr(next_out_index);
	   }
       }
   }
 
}
