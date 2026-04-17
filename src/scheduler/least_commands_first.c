#include "scheduler/least_commands_first.h"
#include "request.h"
#include "scheduler/command.h"
#include "scheduler/dispatcher.h"
#include "scheduler/historical.h"

#include <glib.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  GSequence *sequence;
  Historical *users_history;
} LcfData;

typedef struct node_data {
  Command *command;
  int command_counter; // number of commands ran once the controller receives
                       // the command
} NodeData;

// Lowest command counter goes first
gint compare_lcf_nodes(gconstpointer a, gconstpointer b, gpointer user_data) {
  (void)user_data;
  NodeData *node_a = (NodeData *)a;
  NodeData *node_b = (NodeData *)b;

  if (node_a->command_counter < node_b->command_counter)
    return -1;
  if (node_a->command_counter > node_b->command_counter)
    return 1;
  return 0;
}

void add_command_lcf(int user_id, double created_at, int command_id,
                     void *data) {
  (void)created_at;
  LcfData *lcf = (LcfData *)data;

  int past_commands = get_user_number_command(lcf->users_history, user_id);

  NodeData *node = malloc(sizeof(NodeData));

  node->command = command_create(user_id, command_id);
  node->command_counter = past_commands;

  // O(log n)
  g_sequence_insert_sorted(lcf->sequence, node, compare_lcf_nodes, NULL);
}

Request *get_next_lcf(void *data) {
  LcfData *lcf = (LcfData *)data;

  if (g_sequence_is_empty(lcf->sequence)) {
    return NULL;
  }

  // Get the first element (the one with the least commands )
  GSequenceIter *first_iter = g_sequence_get_begin_iter(lcf->sequence);
  NodeData *node = (NodeData *)g_sequence_get(first_iter);
  Command *cmd_info = node->command;

  g_sequence_remove(first_iter);
  free(node);

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

int has_commands_lcf(void *data) {
  LcfData *lcf = (LcfData *)data;
  return !g_sequence_is_empty(lcf->sequence);
}

void cleanup_lcf(void *data) {
  LcfData *lcf = (LcfData *)data;

  GSequenceIter *iter = g_sequence_get_begin_iter(lcf->sequence);
  while (!g_sequence_iter_is_end(iter)) {
    NodeData *node = (NodeData *)g_sequence_get(iter);
    command_free(node->command);
    free(node);
    iter = g_sequence_iter_next(iter);
  }

  g_sequence_free(lcf->sequence);
  free(lcf);
}

char *list_commands_lcf(void *data) {
  GString *string = g_string_new("Scheduled\n");

  if (!has_commands_lcf(data)) {
    g_string_append(string, "No commands scheduled\n");
  } else {
    while (has_commands_lcf(data)) {
      Request *next = get_next_lcf(data);
      if (next == NULL)
        break;

      g_string_append_printf(string, "user-id %d - command-id %d\n",
                             get_user_id(next), get_command_id(next));
      free(next);
    }
  }

  char *final_string = strdup(string->str);
  g_string_free(string, TRUE);
  return final_string;
}

void initialize_scheduler_lcf(Scheduler *s) {
  LcfData *data = malloc(sizeof(LcfData));

  // We initialize without a GDestroyNotify (NULL) so g_sequence_remove
  // doesn't automatically free the data inside get_next_lcf
  data->sequence = g_sequence_new(NULL);
  data->users_history = get_users_history(s);
  set_data(s, data);
  set_add_command_function(s, add_command_lcf);
  set_get_next_function(s, get_next_lcf);
  set_cleanup_function(s, cleanup_lcf);
  set_has_commands_function(s, has_commands_lcf);
  set_list_commands_function(s, list_commands_lcf);
}