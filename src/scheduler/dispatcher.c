#include "scheduler/dispatcher.h"
#include <unistd.h> 
#include <fcntl.h>
#include "protocol.h"
#include "stdlib.h"
#include "stdio.h"
#include "scheduler/fcfs.h"

struct scheduler {
    void (*addCommand)(int user_id, double created_timestamps, int command_id, void *data);  // returns the command_id 
    int (*getNext)(void *data);
    void (*cleanup)(void *data);
    int (*has_commands)(void *data);
    int total_commands;
    void *data;
};

Scheduler* initialize_scheduler(int scheduler_algorithm){
    Scheduler *s = malloc(sizeof(Scheduler));
    s->total_commands = 0; 
    switch(scheduler_algorithm){
        case 0: 
            initialize_scheduler_fcfs(s);
            break; 
        case 1: // ...
            
    }
    return s;
}

int addCommand(Scheduler *s, int user_id, double created_timestamp){
    s->addCommand(user_id, created_timestamp, s->total_commands, s->data);
    return s->total_commands++;
}

int getNext(Scheduler *s){
    return s->getNext(s->data);
}

int has_commands(Scheduler*s){
    return s->has_commands(s->data);
}

void set_add_command_function(Scheduler *s, void (*addCommand)(int user_id, double created_timestamps, int command_id, void *data)){
    s->addCommand = addCommand;
}

void set_get_next_function(Scheduler *s, int (*getNext)(void *data)){
    s->getNext = getNext; 
}

void set_cleanup_function(Scheduler *s, void (*cleanup)(void *data)){
    s->cleanup = cleanup;
}

void set_data(Scheduler *s, void *data){
    s->data = data;
}

void set_has_commands(Scheduler*s, int (*has_commands)(void *data)){
    s->has_commands =has_commands;
}

void cleanup(Scheduler *s){
    s->cleanup(s->data);
    free(s);
}