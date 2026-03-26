#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include "protocol.h"

int send_to_controller(Protocol *p);

int create_response_pipe(int pid);

int wait_for_go(int pid);

void cleanup_pipe(int pid);

void read_list_response(int pid);

#endif