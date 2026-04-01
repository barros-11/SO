#include "scheduler/historical.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define FILE_PATH "historical.bin"

typedef struct {
  int user_id;
  int commands_run;
  long total_execution_time;
} UserStats;

struct historical {
  GHashTable * historical; 
};

Historical *load_history() {
  Historical* h = malloc(sizeof(Historical)); 
  GHashTable *history =
      g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, free);
  h->historical = history;
  int fd = open(FILE_PATH, O_RDONLY);
  if (fd == -1) {
    // It is completely normal for the file to not exist on the very first run.
    return h;
  }

  UserStats us; 
  while((read(fd, &us, sizeof(UserStats)))  == sizeof(UserStats)){
    UserStats *user_stats = malloc(sizeof(UserStats));
    if (user_stats){
      *user_stats = us; 
      g_hash_table_insert(history, GINT_TO_POINTER(user_stats->user_id), user_stats); 
    }
  }
  close(fd);
  return h;
}

void save_history(Historical *h) {
  GHashTable * history = h->historical;
  if (history == NULL) {
    return;
  }

  // Open with O_TRUNC to overwrite the old file with the new data
  int fd = open(FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd == -1) {
    perror("[Historical] Error saving history state to binary file");
    return;
  }

  GHashTableIter iter;
  gpointer key, value;
  g_hash_table_iter_init(&iter, history);

  while (g_hash_table_iter_next(&iter, &key, &value)) {
    UserStats *stats = (UserStats *)value;
    write(fd, stats, sizeof(UserStats));
  }
  free_history(h); 
  close(fd);
}

void update_user_stats(Historical *h, int user_id, long duration) {
  GHashTable *history = h->historical;
  if (history == NULL) {
    return;
  }

  UserStats *stats = g_hash_table_lookup(history, GINT_TO_POINTER(user_id));

  if (stats == NULL) {
    stats = malloc(sizeof(UserStats));
    if (stats != NULL) {
      stats->user_id = user_id;
      stats->commands_run = 0;
      stats->total_execution_time = 0;
      g_hash_table_insert(history, GINT_TO_POINTER(user_id), stats);
    }
  }

  // Update the statistics
  if (stats != NULL) {
    stats->commands_run++;
    stats->total_execution_time += duration;
  }
}

void free_history(Historical *h) {
  GHashTable *history = h->historical; 
  if (history != NULL) {
    // Because we used g_hash_table_new_full, this safely destroys the table 
    // AND calls free() on every single UserStats struct allocated.
    g_hash_table_destroy(history);
  }
  free(h);
}

int get_user_number_command(Historical *h, int  user_id){
  GHashTable * table = h->historical; 
  gpointer p = g_hash_table_lookup(table, GINT_TO_POINTER(user_id)); 
  if (p == NULL) return 0;
  UserStats * us = (UserStats *)p; 
  return us->commands_run; 
}

long get_user_total_execution_time(Historical *h, int  user_id){
  GHashTable * table = h->historical; 
  gpointer p = g_hash_table_lookup(table, GINT_TO_POINTER(user_id)); 
  if (p==NULL) return 0;
  UserStats * us = (UserStats *)p; 
  return us->total_execution_time; 
}