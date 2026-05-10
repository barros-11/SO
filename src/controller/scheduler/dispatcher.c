#include "scheduler/dispatcher.h"
#include "scheduler/fcfs.h"
#include "scheduler/least_commands_first.h"
#include "scheduler/less_execution_time.h"
#include "scheduler/round_robin.h"
#include "stdio.h"
#include "stdlib.h"
#include <fcntl.h>
#include <glib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

struct scheduler {
  void (*addCommand)(int user_id, double created_at, int command_id,
                     void *data); // returns the command_id
  Request *(*getNext)(void *data);
  void (*cleanup)(void *data);
  int (*has_commands)(void *data);
  char *(*list_commands)(void *data);
  int total_commands;
  void *data;

  int fd;
  int stats_fd;
  int scheduler_algorithm;
  int max_parallelism;
  long run_id;
  const char *history_mode;

  Historical *users_history;

  GHashTable *table;
  GQueue *list;
};

static long now_ms(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000L + tv.tv_usec / 1000L;
}

static const char *policy_name(int scheduler_algorithm) {
  switch (scheduler_algorithm) {
  case 0:
    return "FCFS";
  case 1:
    return "RR";
  case 2:
    return "LCF";
  case 3:
    return "LET";
  default:
    return "UNKNOWN";
  }
}

static void write_stats_header(int fd) {
  const char *header =
      "run_id,history_mode,policy,parallelism,user_id,command_id,submit_time_"
      "ms,start_time_ms,end_time_ms,duration_ms,turnaround_ms,waiting_ms\n";
  write(fd, header, strlen(header));
}

static void write_stats_row(Scheduler *s, Request *stored_cmd, int command_id,
                            Request *event) {
  if (s == NULL || event == NULL || s->stats_fd < 0) {
    return;
  }

  long submit_time =
      stored_cmd != NULL ? get_created_at(stored_cmd) : get_created_at(event);
  long start_time =
      stored_cmd != NULL ? get_started_at(stored_cmd) : submit_time;
  long duration = get_duration(event);
  long turnaround = get_created_at(event);
  long waiting = start_time - submit_time;
  long end_time = submit_time + turnaround;

  if (waiting < 0) {
    waiting = 0;
  }

  char buffer[256];
  int len = snprintf(
      buffer, sizeof(buffer), "%ld,%s,%s,%d,%d,%d,%ld,%ld,%ld,%ld,%ld,%ld\n",
      s->run_id, s->history_mode, policy_name(s->scheduler_algorithm),
      s->max_parallelism, get_user_id(event), command_id, submit_time,
      start_time, end_time, duration, turnaround, waiting);

  if (len > 0) {
    write(s->stats_fd, buffer, len);
  }
}

Scheduler *initialize_scheduler(int scheduler_algorithm) {
  const char *stats_path = getenv("STATS_CSV");
  const char *parallelism_env = getenv("PARALLELISM");
  const char *history_mode_env = getenv("HISTORY_MODE");
  struct stat st;

  Scheduler *s = malloc(sizeof(struct scheduler));
  int fd = open("logs.txt", O_CREAT | O_APPEND | O_WRONLY, 0666);
  if (fd == -1) {
    perror("Error opening the file for the logs");
    free(s);
    return NULL;
  }

  int stats_fd = open(stats_path != NULL ? stats_path : "tmp/stats.csv",
                      O_CREAT | O_APPEND | O_WRONLY, 0666);
  if (stats_fd == -1) {
    perror("Error opening stats csv");
    close(fd);
    free(s);
    return NULL;
  }

  if (fstat(stats_fd, &st) == 0 && st.st_size == 0) {
    write_stats_header(stats_fd);
  }

  s->fd = fd;
  s->stats_fd = stats_fd;
  s->scheduler_algorithm = scheduler_algorithm;
  s->max_parallelism = parallelism_env != NULL ? atoi(parallelism_env) : 0;
  s->run_id = now_ms();
  s->history_mode = history_mode_env != NULL ? history_mode_env : "unknown";
  s->table = g_hash_table_new(g_direct_hash, g_direct_equal);
  s->list = g_queue_new();
  s->total_commands = 0;
  s->users_history = load_history();
  char buffer[128];
  switch (scheduler_algorithm) {
  case 0:
    initialize_scheduler_fcfs(s);
    snprintf(buffer, sizeof(buffer),
             "[Scheduler] First Come First Served selected!\n");
    break;
  case 1:
    initialize_scheduler_rr(s);
    snprintf(buffer, sizeof(buffer), "[Scheduler] Round Robin selected!\n");
    break;
  case 2:
    initialize_scheduler_lcf(s);
    snprintf(buffer, sizeof(buffer),
             "[Scheduler] Least Commands First selected!\n");
    break;
  case 3:
    initialize_scheduler_let(s);
    snprintf(buffer, sizeof(buffer),
             "[Scheduler] Less Execution Time selected!\n");
    break;
  default:
    initialize_scheduler_fcfs(s);
    snprintf(buffer, sizeof(buffer),
             "[Scheduler] Unknown algorithm, defaulting to FCFS.\n");
    break;
  }
  write(STDOUT_FILENO, buffer, strlen(buffer));
  return s;
}

int add_command(Scheduler *s, int user_id, double created_timestamp,
                int command_id) {
  s->addCommand(user_id, created_timestamp, command_id, s->data);
  increment_user_command_count(s->users_history, user_id);
  return s->total_commands++;
}

Request *get_next(Scheduler *s) { return s->getNext(s->data); }

int has_commands(Scheduler *s) { return s->has_commands(s->data); }

void set_add_command_function(Scheduler *s,
                              void (*addCommand)(int user_id, double created_at,
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
  save_history(s->users_history);
  g_hash_table_destroy(s->table);
  g_queue_free_full(s->list, free);
  s->cleanup(s->data);
  close(s->fd);
  close(s->stats_fd);
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
  g_string_append(result, "\n---\nExecuting\n");
  if (s->list->length == 0)
    g_string_append(result, "No commands executing\n");
  else
    g_queue_foreach(s->list, append_to_string, result);
  g_string_append(result, "---\n");
  char *result_to_be_executed = s->list_commands(s->data);
  if (result_to_be_executed != NULL) {
    g_string_append(result, result_to_be_executed);
    free(result_to_be_executed);
  }
  char *final_result = strdup(result->str);
  g_string_free(result, 1);
  return final_result;
}

void save_log(Request *r, int fd) {
  if (r == NULL || fd < 0) {
    return;
  }
  char buffer[256];

  int user_id = get_user_id(r);
  int command_id = get_command_id(r);
  long duration = get_duration(r);
  long time_since_creation = get_created_at(r);

  int len = snprintf(buffer, sizeof(buffer),
                     "user-id: %d, command-id: %d, time_since_creation: %ld "
                     "ms, duration: %ld ms\n",
                     user_id, command_id, time_since_creation, duration);

  if (len > 0) {
    write(fd, buffer, len);
  }
}

int remove_command(Scheduler *s, int command_id, Request *r) {
  gpointer element = g_hash_table_lookup(s->table, GINT_TO_POINTER(command_id));
  if (element != NULL) {
    GList *elem = (GList *)element;
    Request *stored_cmd = elem != NULL ? (Request *)elem->data : NULL;
    write_stats_row(s, stored_cmd, command_id, r);
    save_log(r, s->fd);
    increment_user_duration(s->users_history, get_user_id(r), get_duration(r));
    free(elem->data);
    g_queue_delete_link(s->list, elem);
    g_hash_table_remove(s->table, GINT_TO_POINTER(command_id));
    return 0;
  }
  return -1;
}

void insert_command(Scheduler *s, Request *cmd_info) {
  Request *cmd = malloc(get_request_size());
  if (cmd == NULL)
    return;

  set_user_id(get_user_id(cmd_info), cmd);
  set_command_id(get_command_id(cmd_info), cmd);
  set_created_at(get_created_at(cmd_info), cmd);
  set_duration(cmd, get_duration(cmd_info));
  set_started_at(cmd, get_started_at(cmd_info));
  set_status(get_status(cmd_info), cmd);

  g_queue_push_tail(s->list, cmd);
  g_hash_table_insert(s->table, GINT_TO_POINTER(get_command_id(cmd_info)),
                      s->list->tail);
}

Historical *get_users_history(Scheduler *s) { return s->users_history; }

GQueue *get_running_commands(Scheduler *s) { return s->list; }
