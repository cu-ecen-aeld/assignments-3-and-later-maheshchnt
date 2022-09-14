#include "systemcalls.h"
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{
    int rc = 0; //return code

    rc = system(cmd);
    if (rc < 0 || rc == 127) {
       return false;
    }
  
    return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    int      child_status = -1;
    pid_t   child_pid = -1;
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    va_end(args);

    //create child process
    child_pid = fork();
    if (child_pid == -1) {
	return false; //child process creation failed
    }
    if (child_pid == 0) {
	execv(command[0], command);
	exit(-1);
    } else {
	int rc =  waitpid(child_pid, &child_status, 0);
	if (rc != -1 && child_status == 0) {
		return true;
	}
    }

    return false;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+2];
    int i;

    pid_t   child_pid = -1;
    int     child_status = -1;
 
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    va_end(args);

    int fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
    if (fd < 0) { 
	perror("open"); 
	return -1; 
    }

    //create child process
    child_pid = fork();
    if (child_pid == -1) {
	return false; //child process creation failed
    }
    if (child_pid == 0) {
	if (dup2(fd, 1) < 0) 
	{ 
	    perror("dup2"); 
	    exit(-1);
	}

        close(fd);

	execv(command[0], command);
	exit(-1);
    } else {
	int rc =  waitpid(child_pid, &child_status, 0);
	 close(fd);
	if (rc != -1 && child_status == 0) {
		return true;
	}
    }

    return false;

}
