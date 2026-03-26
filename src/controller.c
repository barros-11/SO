#include <unistd.h> 
#include <fcntl.h>
#include "protocol.h"
#include "stdlib.h"
#include "stdio.h"
#include "scheduler/dispatcher.h"

void controller(int max_commands, int scheduler_algorithm){ // para ja sem argumentos -> futuramente recebe o numero de comandos a executar concorrentemente, e também a politica a executar

	Scheduler* scheduler = initialize_scheduler(scheduler_algorithm); 
    
    int fd = open("tmp/controller_pipe",  O_RDONLY); 
    int acabou  = 0; 
    int current_commands = 0; // number of commands being run
	while (current_commands|| !acabou){
        // if we didnt end and (we dont have commands to execute or we cant put any more commands to execute at the same time) -> we wait for a new protocol
       if (!acabou && (!has_commands(scheduler) || current_commands == max_commands)){ 
           Protocol *p = malloc(getProtocolSize()); 
           ssize_t size = read(fd, p, getProtocolSize());
           if (size <= 0 || size != getProtocolSize()){
               free(p);
               close(fd);
               printf("Error reading from pipe or incomplete protocol message\n");
               return; 
           }
    
        	switch(getType(p)){
              case END:
                    acabou = 1;
                    break; 
               case TERMINATED: 
                    current_commands--;
                    break;
               case EXECUTE: 
					int  command_id = addCommand(scheduler, getUserId(p), getCreatedTimestamps(p));
                    // notifyRunner(command_id, flag de ser submetida);
                    break; 
               case LIST: 
                    int pid = fork(); 
                    if (pid == 0)
                        // list_commands(); 
                    break; 
           }
           free(p);
    	}

		if (current_commands < max_commands &&  has_commands(scheduler)){
			int command_id = getNext(scheduler);
			current_commands++;
			//notifyRunner(command_id, flag para comerçar a rodar)
		}
	}
    cleanup(scheduler);
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