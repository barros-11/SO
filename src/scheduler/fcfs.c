#include "scheduler/fcfs.h"
#include "scheduler/dispatcher.h"

#include <glib.h>

void *initialize_data_fcfs(){
    return ((void *)g_queue_new());
}

void add_command_fcfs(int user_id, double created_at, int command_id, void *data){
    (void)user_id;
    (void)created_at; 
    GQueue *q = (GQueue *)data; 
    g_queue_push_tail(q, GINT_TO_POINTER(command_id));
}

int get_next_fcfs(void *data){
    GQueue *q = (GQueue *)data; 
    gpointer next = g_queue_pop_head(q);
    return GPOINTER_TO_INT(next);
}

void cleanup_fcfs(void *data){
    g_queue_free((GQueue*)data);
}

void initialize_scheduler_fcfs(Scheduler * s){
    set_add_command_function(s, add_command_fcfs);
    set_get_next_function(s, get_next_fcfs);
    set_cleanup_function(s, cleanup_fcfs);
    set_has_commands(s, has_commands_fcfs);
    set_data(s, initialize_data_fcfs());
}

int has_commands_fcfs(void *data){
    GQueue*q = (GQueue *)data; 
    return q->length != 0; 
}