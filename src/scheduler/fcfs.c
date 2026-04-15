#include "scheduler/fcfs.h"
#include "scheduler/command.h"
#include "scheduler/dispatcher.h"

#include <glib.h>
#include <stdlib.h>

static void *initialize_data_fcfs() { return ((void *)g_queue_new()); }

static void add_command_fcfs(int user_id, double created_at, int command_id,
                             void *data) {
  (void)created_at;
  GQueue *q = (GQueue *)data;
  Command *cmd = command_create(user_id, command_id);
  if (cmd != NULL)
    g_queue_push_tail(q, cmd);
}

static Request *get_next_fcfs(void *data) {
  GQueue *q = (GQueue *)data;
  Command *cmd_info = (Command *)g_queue_pop_head(q);
  if (cmd_info == NULL)
    return NULL;

  Request *result = malloc(get_request_size());
  if (result != NULL) {
    set_user_id(command_get_user_id(cmd_info), result);
    set_command_id(command_get_command_id(cmd_info), result);
    set_created_timestamps(0, result);
    set_duration(result, 0);
    set_status(EXECUTE, result);
  }

  command_free(cmd_info);
  return result;
}

static void cleanup_fcfs(void *data) {
  g_queue_free_full((GQueue *)data, (GDestroyNotify)command_free);
}

static void append_to_string_fcfs(gpointer data, gpointer user_data) {
  GString *string = (GString *)user_data;
  Command *cmd = (Command *)data;
  g_string_append_printf(string, "user-id %d - command-id %d\n",
                         command_get_user_id(cmd), command_get_command_id(cmd));
}

static char *list_commands_fcfs(void *data) {
  GQueue *q = (GQueue *)data;
  GString *string = g_string_new(NULL);
  g_string_append(string, "Scheduled\n");
  if (q->length == 0)
    g_string_append(string, "No commands scheduled\n");
  else
    g_queue_foreach(q, append_to_string_fcfs, string);
  char *final_string = strdup(string->str);
  g_string_free(string, TRUE);
  return final_string;
}

static int has_commands_fcfs(void *data) {
  GQueue *q = (GQueue *)data;
  return q->length != 0;
}

void initialize_scheduler_fcfs(Scheduler *s) {
  set_add_command_function(s, add_command_fcfs);
  set_get_next_function(s, get_next_fcfs);
  set_cleanup_function(s, cleanup_fcfs);
  set_has_commands_function(s, has_commands_fcfs);
  set_list_commands_function(s, list_commands_fcfs);
  set_data(s, initialize_data_fcfs());
}
