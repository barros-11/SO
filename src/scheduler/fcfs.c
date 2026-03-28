#include "scheduler/fcfs.h"
#include "scheduler/dispatcher.h"
#include "command.h"

#include <glib.h>
#include <stdlib.h>

typedef struct {
    int user_id;
    int command_id;
} CommandInfo;

void *initialize_data_fcfs(){
    return ((void *)g_queue_new());
}

void add_command_fcfs(int user_id, double created_at, int command_id, void *data){
    (void)created_at; 
    GQueue *q = (GQueue *)data; 
    CommandInfo *cmd_info = malloc(sizeof(CommandInfo));
    cmd_info->user_id = user_id;
    cmd_info->command_id = command_id;
    g_queue_push_tail(q, cmd_info);
}

Command *get_next_fcfs(void *data){
    GQueue *q = (GQueue *)data; 
    CommandInfo *cmd_info = (CommandInfo *)g_queue_pop_head(q);
    if (cmd_info == NULL) return NULL;
    
    // Devolve um Command com ambos os campos
    Command *result = command_create(cmd_info->user_id, cmd_info->command_id);
    free(cmd_info);
    return result;
}

void cleanup_fcfs(void *data){
    g_queue_free_full((GQueue*)data, free);
}

void append_to_string_fcfs (
  gpointer data,
  gpointer user_data
) { 
    GString * string = (GString *)user_data; 
    CommandInfo * cmd = (CommandInfo *)data; 
    g_string_append_printf(string, "user-id %d - command-id %d\n", cmd->user_id, cmd->command_id);
}

char *list_commands_fcfs(void*data){
    GQueue *q = (GQueue *)data; 
    GString *string = g_string_new(NULL);
    g_string_append(string, "Scheduled\n");
    if (q->length == 0)
        g_string_append(string, "No commands scheduled\n"); 
    else 
        g_queue_foreach(q, append_to_string_fcfs, string);
    char * final_string= strdup(string->str);
    g_string_free(string, TRUE);
    return final_string; 
}

void initialize_scheduler_fcfs(Scheduler * s){
    set_add_command_function(s, add_command_fcfs);
    set_get_next_function(s, get_next_fcfs);
    set_cleanup_function(s, cleanup_fcfs);
    set_has_commands_function(s, has_commands_fcfs);
    set_list_commands_function(s, list_commands_fcfs); 
    set_data(s, initialize_data_fcfs());
}

int has_commands_fcfs(void *data){
    GQueue*q = (GQueue *)data; 
    return q->length != 0; 
}
