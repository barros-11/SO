#include "scheduler/dispatcher.h"
#include "request.h"
#include "scheduler/command.h"
#include "scheduler/fcfs.h"
#include "scheduler/round_robin.h"
#include "stdio.h"
#include "stdlib.h"
#include <fcntl.h>
#include <glib.h>
#include <unistd.h>

struct scheduler {
  void (*addCommand)(int user_id, double created_timestamps, int command_id,
                     void *data); // returns the command_id
  Request *(*getNext)(void *data);
  void (*cleanup)(void *data);
  int (*has_commands)(void *data);
  char *(*list_commands)(void *data);
  int total_commands;
  void *data;
  // bellow are the data structers to keep track of the commands currenlty being
  // executed
  GHashTable
      *table;   // maps a command in to the pointer in the linked list bellow
  GQueue *list; // linked list with the commands that are currently running
};

Scheduler *initialize_scheduler(int scheduler_algorithm) {
  Scheduler *s = malloc(sizeof(struct scheduler));
  s->table = g_hash_table_new(g_direct_hash, g_direct_equal);
  s->list = g_queue_new();
  s->total_commands = 0;
  switch (scheduler_algorithm) {
  case 0:
    initialize_scheduler_fcfs(s);
    printf("[Scheduler] First Come First Served selected!");
    break;
  case 1: // ...
    initialize_scheduler_rr(s);
    printf("[Scheduler] Round Robin selected!");
    break;
  }
  return s;
}

int add_command(Scheduler *s, int user_id, double created_timestamp,
                int command_id) {
  s->addCommand(user_id, created_timestamp, command_id, s->data);
  return s->total_commands++;
}

Request *get_next(Scheduler *s) { return s->getNext(s->data); }

int has_commands(Scheduler *s) { return s->has_commands(s->data); }

void set_add_command_function(Scheduler *s,
                              void (*addCommand)(int user_id,
                                                 double created_timestamps,
                                                 int command_id, void *data)) {
  s->addCommand = addCommand;
}

void set_get_next_function(Scheduler *s, Request *(*getNext)(void *data)) {
  s->getNext = getNext;
}

void set_cleanup_function(Scheduler *s, void (*cleanup)(void *data)) {
  s->cleanup = cleanup;
}

void set_data(Scheduler *s, void *data) { s->data = data; }

void set_has_commands_function(Scheduler *s, int (*has_commands)(void *data)) {
  s->has_commands = has_commands;
}

void set_list_commands_function(Scheduler *s,
                                char *(*list_commands)(void *data)) {
  s->list_commands = list_commands;
}

void cleanup(Scheduler *s) {
  g_hash_table_destroy(s->table);
  g_queue_free_full(s->list, free);
  s->cleanup(s->data);
  free(s);
}

void append_to_string(gpointer data, gpointer user_data) {
  GString *string = (GString *)user_data;
  Request *cmd = (Request *)data;
  g_string_append_printf(string, "user-id %d - command-id %d\n",
                         get_user_id(cmd), get_command_id(cmd));
}

char *list_commands(Scheduler *s) {
  GString *result = g_string_new("");
  g_string_append(result, "\n-- \nExecuting\n");
  if (s->list->length == 0)
    g_string_append(result, "No commands executing\n");
  else
    g_queue_foreach(s->list, append_to_string, result);
  g_string_append(result, "---\n");
  char *result_to_be_executed =
      s->list_commands(s->data); // the string that comes from the spefic
                                 // algorithm with the order commands
  g_string_append(result, result_to_be_executed);
  free(result_to_be_executed);
  char *final_result = strdup(result->str);
  g_string_free(result, 1); // 1 <=> True,  for freeing the string
  return final_result;
}

void remove_command(Scheduler *s, int command_id) {
  gpointer element = g_hash_table_lookup(s->table, GINT_TO_POINTER(command_id));
  if (element != NULL) {
    GList *elem = (GList *)element;
    free(elem->data);
    g_queue_delete_link(s->list, elem);
    g_hash_table_remove(s->table, GINT_TO_POINTER(command_id));
  }
}

void insert_command(Scheduler *s, Request *cmd_info) {
  Request *cmd = malloc(get_request_size());
  if (cmd == NULL)
    return;

  set_user_id(get_user_id(cmd_info), cmd);
  set_command_id(get_command_id(cmd_info), cmd);
  set_created_timestamps(get_created_timestamps(cmd_info), cmd);
  set_duration(cmd, get_duration(cmd_info));
  set_status(get_status(cmd_info), cmd);

  g_queue_push_tail(s->list, cmd);
  g_hash_table_insert(s->table, GINT_TO_POINTER(get_command_id(cmd_info)),
                      s->list->tail);
}