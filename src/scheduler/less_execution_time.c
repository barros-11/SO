#include "scheduler/less_execution_time.h"
#include "request.h"
#include "scheduler/command.h"
#include "scheduler/dispatcher.h"
#include "scheduler/historical.h"

#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
  GHashTable *users_commands;
  GSequence *active_users;
  GHashTable *active_time_by_user;
  Historical *users_history;
  Scheduler *scheduler;
} LetData;

static long now_ms(clockid_t clock_id) {
  struct timespec ts;
  if (clock_gettime(clock_id, &ts) != 0)
    return 0;
  return (long)(ts.tv_sec * 1000L + ts.tv_nsec / 1000000L);
}

static long get_active_time_for_user(LetData *let, int user_id) {
  long *active = (long *)g_hash_table_lookup(let->active_time_by_user,
                                             GINT_TO_POINTER(user_id));
  if (active == NULL)
    return 0;
  return *active;
}

static void rebuild_active_time_by_user(LetData *let) {
  g_hash_table_remove_all(let->active_time_by_user);

  long now = now_ms(CLOCK_MONOTONIC);
  GQueue *running_commands = get_running_commands(let->scheduler);
  if (running_commands == NULL)
    return;

  for (GList *it = g_queue_peek_head_link(running_commands); it != NULL;
       it = it->next) {
    Request *running = (Request *)it->data;
    if (running == NULL)
      continue;

    int user_id = get_user_id(running);
    long started_at = get_started_at(running);
    long active_time = now - started_at;
    if (active_time < 0)
      active_time = 0;

    long *total_active = (long *)g_hash_table_lookup(let->active_time_by_user,
                                                     GINT_TO_POINTER(user_id));
    if (total_active == NULL) {
      total_active = malloc(sizeof(long));
      if (total_active == NULL)
        continue;
      *total_active = 0;
      g_hash_table_insert(let->active_time_by_user, GINT_TO_POINTER(user_id),
                          total_active);
    }

    *total_active += active_time;
  }
}

static long get_effective_user_time(LetData *let, int user_id) {
  long persisted = get_user_total_execution_time(let->users_history, user_id);
  long active = get_active_time_for_user(let, user_id);
  return persisted + active;
}

static int compare_users_by_let(LetData *let, int user_a, int user_b) {
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

  return 0;
}

static GSequenceIter *find_best_user_iter_let(LetData *let,
                                              GSequence *users_sequence) {
  rebuild_active_time_by_user(let);

  GSequenceIter *best = g_sequence_get_begin_iter(users_sequence);
  if (g_sequence_iter_is_end(best))
    return best;

  for (GSequenceIter *it = g_sequence_iter_next(best);
       !g_sequence_iter_is_end(it); it = g_sequence_iter_next(it)) {
    int candidate_user = GPOINTER_TO_INT(g_sequence_get(it));
    int best_user = GPOINTER_TO_INT(g_sequence_get(best));
    if (compare_users_by_let(let, candidate_user, best_user) < 0)
      best = it;
  }

  return best;
}

static void free_command_queue(gpointer data) {
  GQueue *queue = (GQueue *)data;
  if (queue == NULL)
    return;

  while (!g_queue_is_empty(queue)) {
    Command *cmd = (Command *)g_queue_pop_head(queue);
    command_free(cmd);
  }

  g_queue_free(queue);
}

static void add_command_let(int user_id, double created_at, int command_id,
                            void *data) {
  LetData *let = (LetData *)data;

  Command *cmd = command_create(user_id, command_id, (long)created_at);
  if (cmd == NULL)
    return;

  GQueue *queue = (GQueue *)g_hash_table_lookup(let->users_commands,
                                                GINT_TO_POINTER(user_id));

  if (queue == NULL) {
    queue = g_queue_new();
    if (queue == NULL) {
      command_free(cmd);
      return;
    }

    g_hash_table_insert(let->users_commands, GINT_TO_POINTER(user_id), queue);
    g_sequence_append(let->active_users, GINT_TO_POINTER(user_id));
  }

  g_queue_push_tail(queue, cmd);
}

static Request *get_next_let(void *data) {
  LetData *let = (LetData *)data;

  if (g_sequence_is_empty(let->active_users))
    return NULL;

  GSequenceIter *best_iter = find_best_user_iter_let(let, let->active_users);
  if (g_sequence_iter_is_end(best_iter))
    return NULL;

  int user_id = GPOINTER_TO_INT(g_sequence_get(best_iter));
  GQueue *queue = (GQueue *)g_hash_table_lookup(let->users_commands,
                                                GINT_TO_POINTER(user_id));
  if (queue == NULL)
    return NULL;

  Command *cmd_info = (Command *)g_queue_pop_head(queue);
  if (cmd_info == NULL)
    return NULL;

  if (g_queue_is_empty(queue)) {
    g_sequence_remove(best_iter);
    g_hash_table_remove(let->users_commands, GINT_TO_POINTER(user_id));
  }

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
  return result;
}

static int has_commands_let(void *data) {
  LetData *let = (LetData *)data;
  return !g_sequence_is_empty(let->active_users);
}

static void cleanup_let(void *data) {
  LetData *let = (LetData *)data;

  g_hash_table_destroy(let->users_commands);
  g_hash_table_destroy(let->active_time_by_user);
  g_sequence_free(let->active_users);
  free(let);
}

static char *list_commands_let(void *data) {
  GString *string = g_string_new("Scheduled\n");

  if (!has_commands_let(data)) {
    g_string_append(string, "No commands scheduled\n");
  } else {
    while (has_commands_let(data)) {
      Request *next = get_next_let(data);
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

void initialize_scheduler_let(Scheduler *s) {
  LetData *data = malloc(sizeof(LetData));
  if (data == NULL)
    return;

  data->users_commands = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                               NULL, free_command_queue);
  data->active_users = g_sequence_new(NULL);
  data->active_time_by_user =
      g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, free);
  data->users_history = get_users_history(s);
  data->scheduler = s;

  set_data(s, data);
  set_add_command_function(s, add_command_let);
  set_get_next_function(s, get_next_let);
  set_cleanup_function(s, cleanup_let);
  set_has_commands_function(s, has_commands_let);
  set_list_commands_function(s, list_commands_let);
}
