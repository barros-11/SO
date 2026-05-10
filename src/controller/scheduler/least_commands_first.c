#include "scheduler/least_commands_first.h"
#include "request.h"
#include "scheduler/command.h"
#include "scheduler/dispatcher.h"
#include "scheduler/historical.h"

#include <glib.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  int user_id;
  int dynamic_command_count;
  GQueue *pending_commands;
  GSequenceIter *seq_iter;
} UserEntry;

typedef struct {
  GHashTable *user_map;
  GSequence *sorted_users;
  Historical *users_history;
} LcfData;

gint compare_lcf_nodes(gconstpointer a, gconstpointer b, gpointer user_data) {
  (void)user_data;
  UserEntry *user_a = (UserEntry *)a;
  UserEntry *user_b = (UserEntry *)b;

  if (user_a->dynamic_command_count < user_b->dynamic_command_count)
    return -1;
  if (user_a->dynamic_command_count > user_b->dynamic_command_count)
    return 1;
  return 0;
}

void add_command_lcf(int user_id, double created_at, int command_id,
                     void *data) {
  (void)created_at;
  LcfData *lcf = (LcfData *)data;

  UserEntry *user =
      g_hash_table_lookup(lcf->user_map, GINT_TO_POINTER(user_id));

  if (user == NULL) {
    user = malloc(sizeof(UserEntry));
    user->user_id = user_id;
    user->dynamic_command_count =
        get_user_number_command(lcf->users_history, user_id);
    user->pending_commands = g_queue_new();

    g_hash_table_insert(lcf->user_map, GINT_TO_POINTER(user_id), user);
    user->seq_iter = g_sequence_insert_sorted(lcf->sorted_users, user,
                                              compare_lcf_nodes, NULL);
  }

  Command *cmd = command_create(user_id, command_id, (long)created_at);
  g_queue_push_tail(user->pending_commands, cmd);

  user->dynamic_command_count++;
  g_sequence_sort_changed(user->seq_iter, compare_lcf_nodes, NULL);
}

static Request *get_next_lcf(void *data) {
  LcfData *lcf = (LcfData *)data;

  if (g_sequence_is_empty(lcf->sorted_users)) {
    return NULL;
  }

  GSequenceIter *first_iter = g_sequence_get_begin_iter(lcf->sorted_users);
  UserEntry *user = (UserEntry *)g_sequence_get(first_iter);

  Command *cmd_info = g_queue_pop_head(user->pending_commands);

  Request *result = malloc(get_request_size());
  if (result != NULL) {
    set_user_id(command_get_user_id(cmd_info), result);
    set_command_id(command_get_command_id(cmd_info), result);
    set_created_at(command_get_created_at(cmd_info), result);
    set_duration(result, 0);
    set_started_at(result, 0);
    set_status(EXECUTE, result);
  }
  command_free(cmd_info);

  if (g_queue_is_empty(user->pending_commands)) {
    g_sequence_remove(user->seq_iter);
    g_hash_table_remove(lcf->user_map, GINT_TO_POINTER(user->user_id));
    g_queue_free(user->pending_commands);
    free(user);
  }

  return result;
}

static int has_commands_lcf(void *data) {
  LcfData *lcf = (LcfData *)data;
  return !g_sequence_is_empty(lcf->sorted_users);
}

static void cleanup_lcf(void *data) {
  LcfData *lcf = (LcfData *)data;

  GSequenceIter *iter = g_sequence_get_begin_iter(lcf->sorted_users);
  while (!g_sequence_iter_is_end(iter)) {
    UserEntry *user = (UserEntry *)g_sequence_get(iter);

    while (!g_queue_is_empty(user->pending_commands)) {
      Command *cmd = g_queue_pop_head(user->pending_commands);
      command_free(cmd);
    }

    g_queue_free(user->pending_commands);
    free(user);
    iter = g_sequence_iter_next(iter);
  }

  g_sequence_free(lcf->sorted_users);
  g_hash_table_destroy(lcf->user_map);
  free(lcf);
}

static char *list_commands_lcf(void *data) {
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

  data->sorted_users = g_sequence_new(NULL);
  data->user_map = g_hash_table_new(g_direct_hash, g_direct_equal);
  data->users_history = get_users_history(s);

  set_data(s, data);
  set_add_command_function(s, add_command_lcf);
  set_get_next_function(s, get_next_lcf);
  set_cleanup_function(s, cleanup_lcf);
  set_has_commands_function(s, has_commands_lcf);
  set_list_commands_function(s, list_commands_lcf);
}