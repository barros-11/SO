#include "scheduler/least_commands_first.h"
#include "request.h"
#include "scheduler/command.h"
#include "scheduler/dispatcher.h"
#include "scheduler/historical.h"

#include <glib.h>
#include <stdlib.h>
#include <string.h>

// Represents a user
typedef struct {
  int user_id;
  int dynamic_command_count; // Tracks history + currently queued commands
  GQueue *pending_commands;  // Queue of Command* waiting to execute
  GSequenceIter
      *seq_iter; // Pointer to this user's position in the sorted sequence
} UserEntry;

typedef struct {
  GHashTable *user_map;    // Maps user_id -> UserEntry*
  GSequence *sorted_users; // Sorted sequence of UserEntry* for O(log N) sorting
  Historical *users_history; // The historical database
} LcfData;

// Sort users by their dynamic command count (Lowest goes first)
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

  // Lookup the user in the Hash Table
  UserEntry *user =
      g_hash_table_lookup(lcf->user_map, GINT_TO_POINTER(user_id));

  // Create the user if he is not is the scheduler
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

  //  Add the new command to the user's personal queue
  Command *cmd = command_create(user_id, command_id);
  g_queue_push_tail(user->pending_commands, cmd);

  // Update their score and let GLib automatically shift them to the right spot!
  user->dynamic_command_count++;
  g_sequence_sort_changed(user->seq_iter, compare_lcf_nodes, NULL);
}

static Request *get_next_lcf(void *data) {
  LcfData *lcf = (LcfData *)data;

  if (g_sequence_is_empty(lcf->sorted_users)) {
    return NULL;
  }

  // Get the user with the lowest command count (first in the sequence)
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

  // If this user has no more then he is removed
  if (g_queue_is_empty(user->pending_commands)) {
    g_sequence_remove(user->seq_iter); // Removes from the sorted tree
    g_hash_table_remove(
        lcf->user_map,
        GINT_TO_POINTER(user->user_id)); // Removes from Hash Table
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

    // Free all remaining commands in this user's queue
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