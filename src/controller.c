#include <unistd.h> 
#include <fcntl.h>
#include "protocol.h"
#include "stdlib.h"
#include "stdio.h"
#include "scheduler/dispatcher.h"
#include "command.h"
#include <string.h>
#include <sys/stat.h>
#include <glib.h>
#include <sys/wait.h>

void notify_runner(int command_id, char *msg){
    char path[64]; 
    snprintf(path, 64, "tmp/runner_%d", command_id);
    printf("path: %s\n", path);
    int fd =open(path, O_WRONLY); 
    if (fd == -1){
        perror("Error opening runner pipe");
        return;
    }
    if (write(fd, msg, strlen(msg)) == -1)
        perror("Error writing to the pipe");
    close(fd);
}

void wait_for_children(GArray *pids){
    for (guint i = 0; i < pids->len; i++) {
        int pid = g_array_index(pids, int, i);
        int status; 
        waitpid(pid, &status, 0);
        if(!WIFEXITED(status)){
            perror("Error exiting a child process"); 
        }
    }
}


void controller(int max_commands, int scheduler_algorithm){ 
    int terminate_command_id;

    unlink("tmp/controller_pipe"); // clean from other executions

    GArray*pids = g_array_new(FALSE, FALSE, sizeof(int)); 

	Scheduler* scheduler = initialize_scheduler(scheduler_algorithm); 
    int v = mkfifo("tmp/controller_pipe", 0666); 
    if (v == -1){
        cleanup(scheduler);
        g_array_free(pids, TRUE);
        perror("Error creating the named pipe\n");
        return;
    }
    int fd = open("tmp/controller_pipe",  O_RDONLY); 
    if (fd == -1) {
        cleanup(scheduler);
        g_array_free(pids, TRUE);
        unlink("tmp/controller_pipe");
        perror("Error opening the controller named pipe");
        return;
    }
    int acabou  = 0; 
    int current_commands = 0; // number of commands being run
	while (current_commands || !acabou){
        // if we didnt end and (we dont have commands to execute or we cant put any more commands to execute at the same time) -> we wait for a new protocol
       if (!acabou && (!has_commands(scheduler) || current_commands == max_commands)){ 
           Protocol *p = malloc(getProtocolSize()); 
           ssize_t size = read(fd, p, getProtocolSize());
           printf("Protocol size is: %d\n", getProtocolSize());
           printf("size is: %ld\n", size);
           if (size != getProtocolSize()){
               free(p);
               close(fd);
               unlink("tmp/controller_pipe");
               if (size == 0){ // so it wait for new runner to be created
                    mkfifo("tmp/controller_pipe", 0666);
                    fd = open("tmp/controller_pipe", O_RDONLY);
                    if (fd == -1) {
                        wait_for_children(pids); 
                        g_array_free(pids, TRUE);
                        cleanup(scheduler);
                        perror("Error reopening controller pipe");
                        return;
                    }
                    continue;  
               }
               perror("Error reading from pipe or incomplete protocol message\n");
               wait_for_children(pids); 
                g_array_free(pids, TRUE);
               return; 
           }
    
        	switch(getType(p)){
              case END:
                    acabou = 1;
                    terminate_command_id = getPid(p);
                    break; 
               case TERMINATED: 
                    remove_command(scheduler, getPid(p));
                    current_commands--;
                    break;
               case EXECUTE: 
					add_command(scheduler, getUserId(p), getCreatedTimestamps(p), getPid(p));
                    break; 
               case LIST: 
                    int pid = fork(); 
                    if (pid == 0) {
                        char * result =  list_commands(scheduler); 
                        notify_runner(getPid(p), result);
                        free(result);
                        _exit(0);
                    }
                    else if (pid > 0)
                        g_array_append_val(pids, pid);
                    else
                        perror("fork failed");
                    break; 
           }
           free(p);
    	}

		if (current_commands < max_commands &&  has_commands(scheduler)){
			Command *cmd = get_next(scheduler);
			if (cmd != NULL) {
				int command_id = command_get_command_id(cmd);
				current_commands++;
				notify_runner(command_id, "GO");
                insert_command(scheduler, cmd); // insert in the list of commands that are currently being executed
				command_free(cmd);
			}
		}
	}
    wait_for_children(pids);

    g_array_free(pids, TRUE);
    cleanup(scheduler);
    close(fd);
    unlink("tmp/controller_pipe");
    notify_runner(terminate_command_id, "ENDED"); // notifies the runner, that sended the finish message, that it will exit 
}

int main(int n, char* args[]) { 
    if (n== 3)
        controller(atoi(args[1]), atoi(args[2]));  // WARNING: if we need to do validation atoi doesnt do that
    else {
        printf("Incorrect number of arguments \n");
        return 1; 
    }
    return 0;
}