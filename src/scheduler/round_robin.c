#include "scheduler/round_robin.h"
#include "scheduler/dispatcher.h"

struct round_robin {
  GSList *current_user;
  GSList *active_users;
  GHashTable *users_jobs;
};

struct job {
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

static void free_job_list(gpointer value) {
  g_slist_free_full((GSList *)value, g_free);
}

static void add_job(GHashTable *users_jobs, int user_id, int command_id,
                    double created_at) {
  GSList *jobs_list = g_hash_table_lookup(users_jobs, GINT_TO_POINTER(user_id));

  if (jobs_list == NULL)
    return;

  Job *job = g_malloc(sizeof(Job));
  job->command_id = command_id;
  job->created_ts = created_at;

  jobs_list = g_slist_append(jobs_list, job);
}

RoundRobin *initialize_data_rr() {
  RoundRobin *round_robin = g_malloc(sizeof(struct round_robin));
  round_robin->current_user = NULL;
  round_robin->active_users = NULL;
  round_robin->users_jobs =
      g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, free_job_list);

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
  GSList *user_jobs =
      g_hash_table_lookup(round_robin->users_jobs, GINT_TO_POINTER(user_id));
  if (!user_jobs) {

    Job *job = g_malloc(sizeof(Job));
    job->command_id = command_id;
    job->created_ts = created_at;

    GSList *list = g_slist_append(NULL, job);
    g_hash_table_insert(round_robin->users_jobs, GINT_TO_POINTER(user_id),
                        list);
  } else
    add_job(round_robin->users_jobs, user_id, command_id, created_at);
}

int get_next_rr(void *data) {
  RoundRobin *round_robin = (RoundRobin *)data;

  if (round_robin == NULL || round_robin->active_users == NULL)
    return -1;

  if (round_robin->current_user == NULL) {
    round_robin->current_user = round_robin->active_users;
    return GPOINTER_TO_INT(round_robin->current_user->data);
  }

  if (round_robin->current_user->next == NULL)
    round_robin->current_user = round_robin->active_users;
  else
    round_robin->current_user = round_robin->current_user->next;

  return GPOINTER_TO_INT(round_robin->current_user->data);
}

int rr_test_active_users_count(void *data) {
  RoundRobin *round_robin = (RoundRobin *)data;

  if (round_robin == NULL)
    return 0;

  return g_slist_length(round_robin->active_users);
}

int rr_test_is_user_active(int user_id, void *data) {
  RoundRobin *round_robin = (RoundRobin *)data;

  if (round_robin == NULL)
    return 0;

  return g_slist_find(round_robin->active_users, GINT_TO_POINTER(user_id)) !=
         NULL;
}

int rr_test_jobs_count(int user_id, void *data) {
  RoundRobin *round_robin = (RoundRobin *)data;

  if (round_robin == NULL)
    return 0;

  GSList *user_jobs =
      g_hash_table_lookup(round_robin->users_jobs, GINT_TO_POINTER(user_id));
  return g_slist_length(user_jobs);
}

int rr_test_current_user(void *data) {
  RoundRobin *round_robin = (RoundRobin *)data;

  if (round_robin == NULL || round_robin->current_user == NULL)
    return -1;

  return GPOINTER_TO_INT(round_robin->current_user->data);
}

void cleanup_rr(void *data) {
  RoundRobin *round_robin = (RoundRobin *)data;

  if (round_robin == NULL)
    return;

  free_users_jobs(round_robin);
  g_free(round_robin);
}