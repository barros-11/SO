#ifndef ROUND_ROBIN_H
#define ROUND_ROBIN_H

#include <glib.h>


typedef struct round_robin RoundRobin;
typedef struct job Job;

RoundRobin *initialize_data_rr();

void add_command_rr(int user_id, double created_at, int command_id, void *data);

int get_next_rr(void *data);

//Todo: remove these (testing)
int rr_test_active_users_count(void *data);

int rr_test_is_user_active(int user_id, void *data);

int rr_test_jobs_count(int user_id, void *data);

int rr_test_current_user(void *data);

void cleanup_rr(void *data);

#endif // ROUND_ROBIN_H