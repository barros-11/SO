#include "protocol.h"
#include <stdlib.h>
#include <sys/types.h>


struct protocol {
    Type status; 
    int user_id;  
    pid_t command_id; 
    int pid;  
    long created_timestamps;  
};

Type getType(Protocol *p){
    return p->status; 
}

void setType(Type t, Protocol *p){
    p->status = t; 
}

void setPid(int pid, Protocol *p){
    p->pid = pid; 
}

int getPid(Protocol *p){
    return p->pid; 
}

void setCommandId(int ci, Protocol *p){
    p->command_id = ci; 
}

int getCommandId(Protocol *p){
    return p->command_id; 
}

void setUserId(int ui, Protocol *p){
    p->user_id = ui; 
}

int getUserId(Protocol *p){
    return p->user_id; 
}

void setCreatedTimestamps(long ts, Protocol *p){
    p->created_timestamps = ts; 
}

long getCreatedTimestamps(Protocol *p){
    return p->created_timestamps; 
}

int getProtocolSize(){
    return sizeof(struct protocol);
}