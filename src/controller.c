#include <unistd.h> 
#include <fcntl.h>
#include "protocol.h"
#include "stdlib.h"
#include "stdio.h"
#include "scheduler/dispatcher.h"

void controller(int max_commands, int scheduler_algorithm){ // para ja sem argumentos -> futuramente recebe o numero de comandos a executar concorrentemente, e também a politica a executar

	Scheduler* scheduler =initialize_scheduler(scheduler_algorithm); 
    
    int fd = open("tmp/controller_pipe",  O_RDONLY); 
    int acabou  = 0; 
    int current_commands = 0; // number of commands being run
	while (current_commands|| !acabou){
        // if we didnt end and (we dont have commands to execute or we cant put any more commands to execute at the same time) -> we wait for a new protocol
       if (!acabou && (current_commands == 0 || current_commands == max_commands)){ 
           Protocol *p = malloc(getPrococolSize()); 
           ssize_t size = read(fd, &p, getProtocolSize());
    
        	switch(getStatus(p)){
              case END:
                    acabou = 1;
                    break; 
               case TERMINATED: 
                    current_commands--;
                    break;
               case EXECUTE: 
					addCommand(scheduler, getUserId(p), getCreatedTimestamps(p), getCommandId(p));
                    break; 
               case LIST: 
                    int pid = fork(); 
                    if (pid == 0)
                        // list_commands(); 
                    break; 
           }
    	}

		if (current_commands < max_commands){
			int command_id = getNext(scheduler);
			current_commands++;
			//notifyRunner()
		}
	}
    close(fd);
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