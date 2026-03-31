#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include "request.h"

int send_to_controller(Request *r);

int create_response_pipe(int pid);

int wait_for_go(int pid);

void cleanup_pipe(int pid);

void read_list_response(int pid);

#endif