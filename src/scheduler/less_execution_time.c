#include "scheduler/less_execution_time.h"
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
  Scheduler *scheduler;
} LetData;

static long get_effective_user_time(LetData *let, int user_id) {
  long persisted = get_user_total_execution_time(let->users_history, user_id);
  long active = get_user_active_execution_time(let->scheduler, user_id);
  return persisted + active;
}

static int compare_commands(LetData *let, Command *cmd_a, Command *cmd_b) {
  int user_a = command_get_user_id(cmd_a);
  int user_b = command_get_user_id(cmd_b);

  long total_a = get_effective_user_time(let, user_a);
  long total_b = get_effective_user_time(let, user_b);

  if (total_a < total_b)
    return -1;
  if (total_a > total_b)
    return 1;

  // fallback case just considers the user id.
  if (user_a < user_b)
    return -1;
  if (user_a > user_b)
    return 1;

  if (command_get_command_id(cmd_a) < command_get_command_id(cmd_b))
    return -1;
  if (command_get_command_id(cmd_a) > command_get_command_id(cmd_b))
    return 1;
  return 0;
}

static GSequenceIter *find_best_iter_let(LetData *let, GSequence *sequence) {
  GSequenceIter *best = g_sequence_get_begin_iter(sequence);
  if (g_sequence_iter_is_end(best))
    return best;

  for (GSequenceIter *it = g_sequence_iter_next(best);
       !g_sequence_iter_is_end(it); it = g_sequence_iter_next(it)) {
    Command *candidate = (Command *)g_sequence_get(it);
    Command *current_best = (Command *)g_sequence_get(best);
    if (compare_commands(let, candidate, current_best) < 0)
      best = it;
  }

  return best;
}

void add_command_let(int user_id, double created_at, int command_id,
                     void *data) {
  (void)created_at;
  LetData *let = (LetData *)data;

  Command *cmd = command_create(user_id, command_id);
  if (cmd == NULL)
    return;

  g_sequence_append(let->sequence, cmd);
}

Request *get_next_let(void *data) {
  LetData *let = (LetData *)data;

  if (g_sequence_is_empty(let->sequence))
    return NULL;

  GSequenceIter *best_iter = find_best_iter_let(let, let->sequence);
  if (g_sequence_iter_is_end(best_iter))
    return NULL;

  Command *cmd_info = (Command *)g_sequence_get(best_iter);

  g_sequence_remove(best_iter);

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

int has_commands_let(void *data) {
  LetData *let = (LetData *)data;
  return !g_sequence_is_empty(let->sequence);
}

void cleanup_let(void *data) {
  LetData *let = (LetData *)data;

  GSequenceIter *iter = g_sequence_get_begin_iter(let->sequence);
  while (!g_sequence_iter_is_end(iter)) {
    Command *cmd = (Command *)g_sequence_get(iter);
    command_free(cmd);
    iter = g_sequence_iter_next(iter);
  }

  g_sequence_free(let->sequence);
  free(let);
}

char *list_commands_let(void *data) {
  LetData *let = (LetData *)data;
  GString *string = g_string_new("Scheduled\n");

  if (!has_commands_let(data)) {
    g_string_append(string, "No commands scheduled\n");
  } else {
    GSequence *list = g_sequence_new(NULL);
    for (GSequenceIter *iter = g_sequence_get_begin_iter(let->sequence);
         !g_sequence_iter_is_end(iter); iter = g_sequence_iter_next(iter)) {
      Command *cmd = (Command *)g_sequence_get(iter);
      g_sequence_append(list, cmd);
    }

    while (!g_sequence_is_empty(list)) {
      GSequenceIter *best = find_best_iter_let(let, list);
      Command *cmd = (Command *)g_sequence_get(best);
      if (cmd != NULL) {
        g_string_append_printf(string, "user-id %d - command-id %d\n",
                               command_get_user_id(cmd),
                               command_get_command_id(cmd));
      }
      g_sequence_remove(best);
    }

    g_sequence_free(list);
  }

  char *final_string = strdup(string->str);
  g_string_free(string, TRUE);
  return final_string;
}

void initialize_scheduler_let(Scheduler *s) {
  LetData *data = malloc(sizeof(LetData));
  if (data == NULL)
    return;

  data->sequence = g_sequence_new(NULL);
  data->users_history = get_users_history(s);
  data->scheduler = s;

  set_data(s, data);
  set_add_command_function(s, add_command_let);
  set_get_next_function(s, get_next_let);
  set_cleanup_function(s, cleanup_let);
  set_has_commands_function(s, has_commands_let);
  set_list_commands_function(s, list_commands_let);
}
