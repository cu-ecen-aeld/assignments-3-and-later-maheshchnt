#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include <stdbool.h>
#include <pthread.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <time.h>
#include "queue.h"

//When set, char driver is used
#define USE_AESD_CHAR_DEVICE (1)

#define MAX_BUF_SIZE         (1024)
#define PORT                 (9000)

typedef struct sockaddr  sockaddr_t;

// client connection queue lock
pthread_mutex_t queue_lock;

#ifndef USE_AESD_CHAR_DEVICE
// lock to protect pkt file
pthread_mutex_t file_lock;
#endif

//client connection queue
typedef struct client_queue {
	int        connfd;           //client connection fd
	int        task_finished;    //flag to indicate client thread is done
	pthread_t  threadId;
        int 	   fd; //file descriptor
	TAILQ_ENTRY(client_queue) ent;
} client_queue_t;

typedef TAILQ_HEAD(head_s, client_queue) head_t;

//head of the queue
head_t head;

//thread function to process the client's connection
void *process_client_connection(void *e);

// Takes a string and puts in in the queue on character at a time.
static int create_client_thread(int connfd)
{
    int rc = -1; //return code

    struct client_queue *e = malloc(sizeof(struct client_queue));
    if (e == NULL)
    {
        syslog(LOG_ERR, "Failed to allocate memory");
	return -1;
    }

    e->connfd = connfd;
    e->task_finished = 0;

#ifdef USE_AESD_CHAR_DEVICE
    // open the file. If it doesn't exist, create one. 
    e->fd = open("/dev/aesdchar", O_RDWR | O_CREAT, S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP | S_IROTH);
    if (e->fd == -1) {
       syslog(LOG_ERR, "Error opening file %s. Errno:%d. Make sure that the directory is already created"\
                       ,"/dev/aesdchar", errno);
       return -1;
    }
#endif
 
    // Create a thread that will function threadFunc()
    rc = pthread_create(&e->threadId, NULL, &process_client_connection, (void *)e);
    if (rc != 0) {
        free(e); // free the allocated memory and return error
        return -1;
    }

    //acquire lock
    if (pthread_mutex_lock(&queue_lock) != 0) {
        syslog(LOG_ERR, "\r\n Failed to acquire lock"); //assert would be better
	return -1;
    }
    TAILQ_INSERT_TAIL(&head, e, ent);

    //unlock
    if (pthread_mutex_unlock(&queue_lock) != 0) {
        syslog(LOG_ERR, "Failed to unlock");
        return -1;
    }

    return 0;
}

int check_for_finished_client_threads()
{
    int    outstanding_cons = 0; //num active client connections
    struct client_queue *e = NULL;
    struct client_queue *next = NULL;

     //acquire lock
    if (pthread_mutex_lock(&queue_lock) != 0) {
        syslog(LOG_ERR, "\r\n Failed to acquire lock"); //assert would be better
	return -1;
    }
    //iterate over all connection elements
    TAILQ_FOREACH_SAFE(e, &head, ent, next)
    {
        if (e->task_finished == true) {
	   pthread_join(e->threadId, NULL);
	   close(e->connfd);
	   close(e->fd);
	   TAILQ_REMOVE(&head, e, ent);
           free(e);
	} else {
           outstanding_cons++;
	}
    }

    //unlock
    if (pthread_mutex_unlock(&queue_lock) != 0) {
        syslog(LOG_ERR, "Failed to unlock");
        return -1;
    }

    return outstanding_cons;
}

#ifndef USE_AESD_CHAR_DEVICE
int fd = -1;//file descriptor
#endif
char write_back_pkt[MAX_BUF_SIZE];

/*
 *  1. Write the contents into the file
 *  2. Send all the file contents back to client
 */
int process_packet(int connfd, int len, char *buff, int fd)
{
   int file_err = -1;
   int write_bytes = 0;
   int read_bytes = 0;

#ifndef USE_AESD_CHAR_DEVICE
   //acquire lock
   if (pthread_mutex_lock(&file_lock) != 0) {
       syslog(LOG_ERR, "\r\n Failed to acquire lock"); //assert would be better
   }

   //append the packet to the pkt file contents
   lseek (fd, 0, SEEK_END);
   // write the string into the given file
#endif

   write_bytes =  write (fd, buff, len);
   file_err = errno;
   if (write_bytes != len) {
      syslog(LOG_ERR, "Failed to write %s to %s. Errno:%d", buff, "/var/tmp/aesdsocketdata", file_err);
   } else {
      syslog(LOG_DEBUG, "Writing %s to %s len:%d fd:%d", buff, "/var/tmp/aesdsocketdata", len, fd);
   }

//#ifndef USE_AESD_CHAR_DEVICE
   //send data in 1024 chunks
   lseek (fd, 0, SEEK_SET);
//#endif
 
   while ((read_bytes = read(fd, write_back_pkt, MAX_BUF_SIZE))) {
      //syslog(LOG_ERR, "send the %s back to client. len:%d", write_back_pkt, read_bytes);
      if (write(connfd, write_back_pkt, read_bytes) != read_bytes) {
	  syslog(LOG_ERR, "Failed to send pkts back to client");
	  return -1;
      } 
   }

#ifndef USE_AESD_CHAR_DEVICE       
   //acquire lock
   if (pthread_mutex_unlock(&file_lock) != 0) {
       syslog(LOG_ERR, "\r\n Failed to unlock"); //assert would be better
    }
#endif

   return 0;
}

// Function designed for chat between client and server.
void *process_client_connection(void *e)
{

    syslog(LOG_INFO, "created client thread");
    char *buff; //packet buffer
    struct client_queue *thread_params;
    int connfd;
    int  n = 0, len = 0;

    //retrieve the data from the client queue
    thread_params = (struct client_queue *) e;
    connfd = thread_params->connfd;

    buff = malloc(MAX_BUF_SIZE);
    if (buff == NULL) {
       syslog(LOG_ERR, "Failed to allocate memory");
       goto cleanup;
    }
    bzero(buff, MAX_BUF_SIZE);
    // infinite loop for chat
   
    // read the message from client and copy it in buffer
    while (read(connfd, &buff[len], MAX_BUF_SIZE) != 0) {
      
	// copy server message into buffer
        for (n = 0; n < MAX_BUF_SIZE; n++) {
            if (buff[len++] == '\n') {
		if (process_packet(connfd, len, buff, thread_params->fd) != 0) {
		   syslog(LOG_ERR, "Failed to process packet that is of %d len", len);
		   goto cleanup;
		}
	    }
	}

        //long pkt, re-allocate more memory	
	buff = realloc(buff, (len+MAX_BUF_SIZE));
    }	

cleanup:

    free(buff);
    //acquire lock
    if (pthread_mutex_lock(&queue_lock) != 0) {
        syslog(LOG_ERR, "\r\n Failed to acquire lock"); //assert would be better
    }
    thread_params->task_finished = 1;
    //acquire lock
    if (pthread_mutex_unlock(&queue_lock) != 0) {
        syslog(LOG_ERR, "\r\n Failed to acquire lock"); //assert would be better
    }
    pthread_exit( NULL );
}

//flag to enable/disable
int run_connections = 1;

void sig_handler(int signo)
{
  if ((signo == SIGTERM) || (signo == SIGINT)) {
     run_connections = 0;
     syslog(LOG_INFO,"Caught signal, exitin\n");
  }
}

#ifndef USE_AESD_CHAR_DEVICE
void timer_thread(union sigval arg)
{
  //source: geeksfrom geeks
  int rc = 0; //return code
  time_t tm;
  struct tm *local_time;
  char time_stamp[1024];
  int len = 0, write_bytes = 0;
  int file_err = 0;

  //get time in seconds since epoch
  (void)time(&tm);

  //convert the seconds to local time
  local_time = localtime(&tm); 

  //retrieve time in required format
  strftime(time_stamp, sizeof(time_stamp), "timestamp:%y/%m/%d - %T\n", local_time);
  len = strlen(time_stamp);

  rc = pthread_mutex_lock(&file_lock);
  if (rc != 0) {
      syslog(LOG_ERR, "Failed to acquire lock"); //maybe assert
  }

  write_bytes =  write (fd, time_stamp, len);
  file_err = errno;

  rc = pthread_mutex_unlock(&file_lock);
  if (rc != 0) {
     syslog(LOG_ERR, "Failed to unlock"); //maybe assert
  }

  if (write_bytes != len) {
      syslog(LOG_ERR, "Failed to write %s to %s. Errno:%d", time_stamp, "/var/tmp/aesdsocketdata", file_err);
   } else {
      syslog(LOG_DEBUG, "Writing %s to %s len:%d fd:%d", time_stamp, "/var/tmp/aesdsocketdata", len, fd);
   }


}

int setup_posix_timer()
{
   timer_t timer_id;
   struct  itimerspec sleep_time;
   struct  sigevent sev;

   memset(&sev, 0, sizeof(struct sigevent));

  /* setup a call to timer_thread passing in the td structure as the segev_value*/
  sev.sigev_notify = SIGEV_THREAD;
  sev.sigev_value.sival_ptr = &timer_id;
  sev.sigev_notify_function = timer_thread;
  sev.sigev_notify_attributes = NULL;

  if (timer_create(CLOCK_REALTIME, &sev, &timer_id) < 0) {
      syslog(LOG_ERR, "Failed to create timer");
      return -1;
  }

  /* set sleep time to 10.001s to ensure the last ms aligned timer event is fired*/
  sleep_time.it_value.tv_sec = 10;
  sleep_time.it_value.tv_nsec = 1000000;
  sleep_time.it_interval.tv_sec = 10;
  sleep_time.it_interval.tv_nsec = 1000000;

  if (timer_settime(timer_id, 0, &sleep_time, 0) < 0) {
      syslog(LOG_ERR, "Failed to start timer");
      return -1;
  }

  return 0;
}
#endif

// Driver function
int main(int argc, char *argv[])
{

    int file_err = -1;

    pid_t pid = 0;
    int sockfd, connfd, len;
    int option_value = 1;
    int dont_fork = 1;   //dont fork the process unless -d option is specified
    struct sockaddr_in servaddr, cli;

    if (argc == 2) {
	dont_fork = strcmp(argv[1], "-d");
    } else if (argc > 2) {
	printf("\n Invalid number of arguments:%d", argc);
    }

#ifndef USE_AESD_CHAR_DEVICE
    // open the file. If it doesn't exist, create one. 
    fd = open("/var/tmp/aesdsocketdata", O_RDWR | O_CREAT, S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP | S_IROTH);
    if (fd == -1) {
       syslog(LOG_ERR, "Error opening file %s. Errno:%d. Make sure that the directory is already created"\
                       ,"/var/tmp/aesdsocketdata", errno);
       return -1;
    }
#endif

    // socket create and verification 
    sockfd = socket(PF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0); 
    if (sockfd == -1) {
        syslog(LOG_ERR, "socket creation failed...\n");
        return -1;;
    }
    else {
        syslog(LOG_INFO, "Socket successfully created..\n");
    }
    
    bzero(&servaddr, sizeof(servaddr));
   
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (int *)&option_value, sizeof(option_value)) < 0)
    syslog(LOG_ERR, "setsockopt(SO_REUSEADDR) failed...\n");
	
    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);  
    servaddr.sin_port = htons(PORT);
   
    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (sockaddr_t*)&servaddr, sizeof(servaddr))) != 0) {
        syslog(LOG_ERR, "socket bind failed...\n");
        return -1;
    } else {
        syslog(LOG_INFO, "Socket successfully binded..\n");
    }

    if (dont_fork == 0) { //fork if dont_fork is not true
       pid = fork();
    }

    if (pid == 0) {

       if (signal(SIGTERM, sig_handler) == SIG_ERR) {
           printf("\n failed to register for signal");
	   goto prog_cleanup;
       }

       if (signal(SIGINT, sig_handler) == SIG_ERR) {
           printf("\n failed to register for signal");
	   goto prog_cleanup;
       }

       // create two seperate locks: one for queue and other file file sync
       if (pthread_mutex_init(&queue_lock, NULL) != 0) {
           printf("\n mutex init failed\n");
           goto prog_cleanup;
       }

#ifndef USE_AESD_CHAR_DEVICE
       if (pthread_mutex_init(&file_lock, NULL) != 0) {
           printf("\n mutex init failed\n");
           goto prog_cleanup;
       }
#endif

       // Now server is ready to listen and verification
       if ((listen(sockfd, 5)) != 0) {
           syslog(LOG_ERR, "Listen failed...\n");
	   goto prog_cleanup;
       } else {
           syslog(LOG_INFO, "Server listening..\n");
       }

#ifndef USE_AESD_CHAR_DEVICE
       //setup and start timer
       (void)setup_posix_timer();
#endif

       len = sizeof(struct sockaddr_in);

       //initialize client queue head
       TAILQ_INIT(&head);

       while (run_connections) {
	  // Accept the data packet from client and verification
	  connfd = accept(sockfd, (sockaddr_t*)&cli, (socklen_t *restrict) &len);
	  file_err = errno;
	  if ((connfd < 0) && (file_err != EAGAIN)) {
	      syslog(LOG_ERR, "server accept failed...\n");
	      break;
	  } else if (!(connfd < 0)) {
	      syslog(LOG_INFO, "server accept the client...\n");

	      if (create_client_thread(connfd) == 0) {
		  syslog(LOG_INFO, "thread successfully for %d connection", connfd);
	      }  
	  }

	  //check if client thread is joined
	  (void) check_for_finished_client_threads();
       }

       //control comes when signal is detected and run_connections == 0
       while (check_for_finished_client_threads() != 0) {
	       //wait for all outstanding connections to be finished;
       }

    }//(pid == 0)

prog_cleanup:
#ifndef USE_AESD_CHAR_DEVICE
       //remove the file
       if (remove("/var/tmp/aesdsocketdata") != 0) {
	   syslog(LOG_ERR, "Failed to delete the /var/tmp/aesdsocketdata file");
       }
#endif

    //close socked and /var/tmp/aesd file fds
#ifndef  USE_AESD_CHAR_DEVICE
    close(fd);
#endif
    close(sockfd);
}
