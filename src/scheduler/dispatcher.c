#include "scheduler/dispatcher.h"
#include <unistd.h> 
#include <fcntl.h>
#include "protocol.h"
#include "stdlib.h"
#include "stdio.h"

struct scheduler {
    void (*addCommand)(int user_id, double created_timestamps, int command_id, void *data); 
    int (*getNext)(void *data);
    void (*cleanup)(void *data);
    int total_commands;
    void *data;
};

Scheduler initialize_scheduler(int scheduler_algorithm){
    Scheduler s; 
    s.total_commands = 0; 
    switch(scheduler_algorithm){
        case 0: // FCFS
            // s.addCommand = addCommandFCFS; 
            // s.getNext = getNextFCFS; 
            // s.cleanup = cleanupFCFS; 
            // s.data = initializeDataFCFS;
            break; 
        case 1: // ...
            
    }
    return s;
}

void addCommand(Scheduler *s, int user_id, double created_timestamp, int command_id){
    s->addCommand(user_id, created_timestamp, command_id, s->data);
}

int getNext(Scheduler *s){
    return s->getNext(s->data);
}

void cleanup(Scheduler *s){
    s->cleanup(s);
}