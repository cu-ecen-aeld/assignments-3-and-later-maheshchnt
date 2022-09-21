#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    struct thread_data *prams = (struct thread_data *) thread_param;
    prams->thread_complete_success = true;


    // sleep before acquiring lock
    if (usleep(prams->wait_time_for_lock * 1000) != 0) {
	ERROR_LOG("\r\n Failed to sleep before acquiring locks");
	prams->thread_complete_success = false;
	return thread_param;
    }

    //acquire lock 
    if (pthread_mutex_lock(prams->lock) != 0) {
    	ERROR_LOG("\r\n Failed to acquire lock");
	prams->thread_complete_success = false;
        return thread_param;

    }

    //sleep before releasing lock
    if (usleep(prams->wait_time_to_release_lock * 1000) != 0) {
	ERROR_LOG("\r\n Failed to sleep after acquiring locks");
	pthread_mutex_unlock(prams->lock);
	prams->thread_complete_success = false;
        return thread_param;
    }


    //release lock
    if (pthread_mutex_unlock(prams->lock) != 0) {
    	ERROR_LOG("\r\n Failed to release lock");
	prams->thread_complete_success = false;
        return thread_param;

    }

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{

    int rc = -1; //return code
    struct thread_data *thread_parms; //thread parameters

    //allocate the memory for the thread params
    thread_parms = malloc(sizeof(struct thread_data));
    if (thread_parms == NULL) {
	return false;
    }
    
    // pass mutex, lock wait period, lock release period parameters to newly created thread.
    thread_parms->lock 			     = mutex;
    thread_parms->wait_time_for_lock        = wait_to_obtain_ms;
    thread_parms->wait_time_to_release_lock = wait_to_release_ms;

    // Create a thread that will function threadFunc()
    rc = pthread_create(thread, NULL, &threadfunc, (void *) thread_parms);
    if (rc != 0) {
        free(thread_parms); // free the allocated memory and return false
	return false;
    }

    return true;
}

