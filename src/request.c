#include "request.h"
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>

struct request {
  Status status;
  int user_id;
  int command_id;
  long created_at;
  long started_at;
  long duration;
};

Status get_status(Request *r) { return r->status; }

void set_status(Status s, Request *r) { r->status = s; }

void set_command_id(int command_id, Request *r) { r->command_id = command_id; }

int get_command_id(Request *r) { return r->command_id; }

void set_user_id(int ui, Request *r) { r->user_id = ui; }

int get_user_id(Request *r) { return r->user_id; }

void set_created_at(long ts, Request *r) { r->created_at = ts; }

long get_created_at(Request *r) { return r->created_at; }

int get_request_size() { return sizeof(struct request); }

void set_duration(Request *r, long duration) { r->duration = duration; }

long get_duration(Request *r) { return r->duration; }

void set_started_at(Request *r, long started_at) { r->started_at = started_at; }

long get_started_at(Request *r) { return r->started_at; }