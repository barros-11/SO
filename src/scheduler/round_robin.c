#include "scheduler/round_robin.h"
#include "scheduler/command.h"
#include "scheduler/dispatcher.h"
#include <string.h>

struct round_robin {
  GSList *current_user;
  GSList *active_users;
  GHashTable *users_jobs;
  int last_served_user;
};

struct job {
  int user_id;
  int command_id;
  double created_ts;
};

static GSList *insert_user_before_current(GSList *active_users,
                                          GSList *current_user_node,
                                          int user_id) {
  gpointer user_ptr = GINT_TO_POINTER(user_id);

  if (active_users == NULL)
    return g_slist_append(NULL, user_ptr);

  if (g_slist_find(active_users, user_ptr) != NULL)
    return active_users;

  if (current_user_node == NULL)
    return g_slist_append(active_users, user_ptr);

  return g_slist_insert_before(active_users, current_user_node, user_ptr);
}

static void free_job_queue(gpointer value) {
  g_queue_free_full((GQueue *)value, g_free);
}

static void add_job(GQueue *jobs_queue, int user_id, int command_id,
                    double created_at) {

  if (jobs_queue == NULL)
    return;

  Job *job = g_malloc(sizeof(Job));
  job->user_id = user_id;
  job->command_id = command_id;
  job->created_ts = created_at;

  g_queue_push_tail(jobs_queue, job);
}

void initialize_scheduler_rr(Scheduler *s) {
  set_add_command_function(s, add_command_rr);
  set_get_next_function(s, get_next_rr);
  set_cleanup_function(s, cleanup_rr);
  set_has_commands_function(s, has_commands_rr);
  set_list_commands_function(s, list_commands_rr);
  set_data(s, initialize_data_rr());
}

RoundRobin *initialize_data_rr() {
  RoundRobin *round_robin = g_malloc(sizeof(struct round_robin));
  round_robin->current_user = NULL;
  round_robin->active_users = NULL;
  round_robin->last_served_user = -1;
  round_robin->users_jobs = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                                  NULL, free_job_queue);

  return round_robin;
}

static void free_users_jobs(RoundRobin *round_robin) {
  g_slist_free(round_robin->active_users);
  g_hash_table_destroy(round_robin->users_jobs);
}

void add_command_rr(int user_id, double created_at, int command_id,
                    void *data) {
  RoundRobin *round_robin = (RoundRobin *)data;

  if (round_robin == NULL)
    return;

  // linked list
  round_robin->active_users = insert_user_before_current(
      round_robin->active_users, round_robin->current_user, user_id);

  // hash table
  GQueue *user_jobs_queue =
      g_hash_table_lookup(round_robin->users_jobs, GINT_TO_POINTER(user_id));
  if (!user_jobs_queue) {

    Job *job = g_malloc(sizeof(Job));
    job->user_id = user_id;
    job->command_id = command_id;
    job->created_ts = created_at;

    GQueue *jobs_queue = g_queue_new();
    g_queue_push_head(jobs_queue, job);

    g_hash_table_insert(round_robin->users_jobs, GINT_TO_POINTER(user_id),
                        jobs_queue);

  } else
    add_job(user_jobs_queue, user_id, command_id, created_at);
}

Request *get_next_rr(void *data) {
  RoundRobin *round_robin = (RoundRobin *)data;

  if (round_robin == NULL || round_robin->active_users == NULL)
    return NULL;

  if (round_robin->current_user == NULL)
    round_robin->current_user = round_robin->active_users;

  if (round_robin->active_users->next != NULL &&
      GPOINTER_TO_INT(round_robin->current_user->data) ==
          round_robin->last_served_user) {
    round_robin->current_user = round_robin->current_user->next
                                    ? round_robin->current_user->next
                                    : round_robin->active_users;
  }

  GSList *served_user_node = round_robin->current_user;
  int user_id = GPOINTER_TO_INT(served_user_node->data);
  GQueue *jobs_queue =
      g_hash_table_lookup(round_robin->users_jobs, GINT_TO_POINTER(user_id));

  if (jobs_queue == NULL || g_queue_is_empty(jobs_queue))
    return NULL;

  Job *job = g_queue_pop_head(jobs_queue);
  round_robin->last_served_user = user_id;
  int command_id = job->command_id;
  Command *next_command = command_create(user_id, command_id);
  Request *result = NULL;

  if (next_command != NULL) {
    result = malloc(get_request_size());
    if (result != NULL) {
      set_user_id(command_get_user_id(next_command), result);
      set_command_id(command_get_command_id(next_command), result);
      set_created_timestamps((long)job->created_ts, result);
      set_duration(result, 0);
      set_status(EXECUTE, result);
    }
    command_free(next_command);
  }
  g_free(job);

  GSList *next_user_node = served_user_node->next ? served_user_node->next
                                                  : round_robin->active_users;

  if (g_queue_is_empty(jobs_queue)) {
    round_robin->active_users =
        g_slist_remove(round_robin->active_users, GINT_TO_POINTER(user_id));
    g_hash_table_remove(round_robin->users_jobs, GINT_TO_POINTER(user_id));

    if (round_robin->active_users == NULL)
      round_robin->current_user = NULL;
    else
      round_robin->current_user =
          g_slist_find(round_robin->active_users,
                       next_user_node ? next_user_node->data
                                      : round_robin->active_users->data);
  } else {
    round_robin->current_user = next_user_node;
  }

  return result;
}

int has_commands_rr(void *data) {
  RoundRobin *round_robin = (RoundRobin *)data;

  return round_robin != NULL && round_robin->active_users != NULL;
}

void cleanup_rr(void *data) {
  RoundRobin *round_robin = (RoundRobin *)data;

  if (round_robin == NULL)
    return;

  free_users_jobs(round_robin);
  g_free(round_robin);
}

char *list_commands_rr(void *data) {
  RoundRobin *round_robin = (RoundRobin *)data;
  GString *string = g_string_new(NULL);
  g_string_append(string, "Scheduled\n");

  if (!round_robin->active_users) {
    g_string_append(string, "No commands scheduled\n");
  } else {
    while (has_commands_rr(data)) {
      Request *next = get_next_rr(data);
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