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

#define MAX_BUF_SIZE         (1024)
#define PORT                 (9000)
#define SA struct sockaddr   

#if 0
int getaddrinfo(const char *restrict node,
                       const char *restrict service,
                       const struct addrinfo *restrict hints,
                       struct addrinfo **restrict res);

       void freeaddrinfo(struct addrinfo *res);
#endif	   
	   
int fd = -1;//file descriptor
char *buff; //packet buffer
char write_back_pkt[MAX_BUF_SIZE];

/*
 *  1. Write the contents into the file
 *  2. Send all the file contents back to client
 */
int process_packet(int connfd, int len)
{
   int file_err = -1;
   int write_bytes = 0;
   int read_bytes = 0;

   //append the packet to the pkt file contents
   lseek (fd, 0, SEEK_END);
   // write the string into the given file

   write_bytes =  write (fd, buff, len);
   file_err = errno;
   if (write_bytes != len) {
      syslog(LOG_ERR, "Failed to write %s to %s. Errno:%d", buff, "/var/tmp/aesdsocketdata", file_err);
   } else {
      //syslog(LOG_DEBUG, "Writing %s to %s len:%d fd:%d", buff, "/var/tmp/aesdsocketdata", len, fd);
   }

   //send data in 1024 chunks
   lseek (fd, 0, SEEK_SET);
 
   while ((read_bytes = read(fd, write_back_pkt, MAX_BUF_SIZE))) {
      //syslog(LOG_ERR, "send the %s back to client. len:%d", write_back_pkt, read_bytes);
      if (write(connfd, write_back_pkt, read_bytes) != read_bytes) {
	  syslog(LOG_ERR, "Failed to send pkts back to client");
	  return -1;
      } 
   }
 
   return 0;
}

// Function designed for chat between client and server.
void process_client_connection(int connfd)
{
    int  n = 0, len = 0;

    buff = malloc(MAX_BUF_SIZE);
    bzero(buff, MAX_BUF_SIZE);
    // infinite loop for chat
   
    // read the message from client and copy it in buffer
    while (read(connfd, &buff[len], MAX_BUF_SIZE) != 0) {
      
	// copy server message into buffer
        for (n = 0; n < MAX_BUF_SIZE; n++) {
            if (buff[len++] == '\n') {
		if (process_packet(connfd, len) != 0) {
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

    // open the file. If it doesn't exist, create one. 
    fd = open("/var/tmp/aesdsocketdata", O_RDWR | O_CREAT, S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP | S_IROTH);
    if (fd == -1) {
       syslog(LOG_ERR, "Error opening file %s. Errno:%d. Make sure that the directory is already created"\
                       ,"/var/tmp/aesdsocketdata", errno);
       return -1;
    }

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
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
        syslog(LOG_ERR, "socket bind failed...\n");
        return -1;
    }
    else {
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

       // Now server is ready to listen and verification
       if ((listen(sockfd, 5)) != 0) {
           syslog(LOG_ERR, "Listen failed...\n");
	   goto prog_cleanup;
       } else {
           syslog(LOG_INFO, "Server listening..\n");
       }

       len = sizeof(struct sockaddr_in);
   
       while (run_connections) {
	  // Accept the data packet from client and verification
	  connfd = accept(sockfd, (SA*)&cli, (socklen_t *restrict) &len);
	  file_err = errno;
	  if ((connfd < 0) && (file_err != EAGAIN)) {
	      syslog(LOG_ERR, "server accept failed...\n");
	      break;
	  } else if (!(connfd < 0)) {
	      syslog(LOG_INFO, "server accept the client...\n");
   
	      process_client_connection(connfd);
	  }
       }

prog_cleanup:
       //remove the file
       if (remove("/var/tmp/aesdsocketdata") != 0) {
	   syslog(LOG_ERR, "Failed to delete the /var/tmp/aesdsocketdata file");
       }
    } // (pid == 0)


    //close socked and /var/tmp/aesd file fds
    close(fd);
    close(sockfd);
}
