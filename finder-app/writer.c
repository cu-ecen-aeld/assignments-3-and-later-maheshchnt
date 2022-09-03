/***********************************************************************
 * @file      writer.c
 * @version   0.1
 * @brief     writer function implementation file.
 *
 * @author    Mahesh Gudipati, magu6520@Colorado.edu
 * @date      Sep 03, 2022
 *
 * @institution University of Colorado Boulder (UCB)
 * @course      ECEN 57133-001: Advanced Embedded Software Dev
 * @instructor  Daniel Walkes
 *
 * @assignment Assignment 2 - Cross compiler setup
 * @due        
 *
 * @resources  
 *
 */
#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{

    // file and string arguments
    char *file = argv[1];
    char *string = argv[2];

    //string length
    int str_len = strlen(string);

    int fd = -1;//file descriptor
    int file_err = -1;

    ssize_t written_bytes; //number of bytes written into the file

    if (argc != 3)
    {
       printf("\n Invalid number of arguments:%d command:%s %s %s",\
		  argc, argc>=1?argv[0]:NULL, argc>=2?argv[1]:NULL, argc>=3?argv[2]:NULL);
       printf("\n Usage: ./writer [filename] [writestring]");
       printf("\n Writes the given string into the specified files");
       return 1;
    }

    openlog(NULL, 0, LOG_USER);

    // open the file. If it doesn't exist, create one. 
    fd = open(file, O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP | S_IROTH);
    file_err = errno;
    if (fd == -1) {
       syslog(LOG_ERR, "Error opening file %s. Errno:%d. Make sure that the directory is already created"\
		       , file, file_err);
    } 

    // write the tring into the given file
    written_bytes =  write (fd, string, str_len);
    file_err = errno;
    if (written_bytes != str_len) {
       syslog(LOG_ERR, "Failed to write %s to %s. Errno:%d", string, file, file_err);	    
    } else {
       syslog(LOG_DEBUG, "Writing %s to %s", string, file);
    }

    return 0;
}
