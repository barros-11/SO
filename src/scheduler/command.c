#include "scheduler/command.h"
#include <stdlib.h>

struct command {
  int user_id;
  int command_id;
  long created_at;
};

Command *command_create(int user_id, int command_id, long created_at) {
  Command *cmd = malloc(sizeof(Command));
  if (cmd != NULL) {
    cmd->user_id = user_id;
    cmd->command_id = command_id;
    cmd->created_at = created_at;
  }
  return cmd;
}

void command_free(Command *cmd) {
  if (cmd != NULL) {
    free(cmd);
  }
}

int command_get_user_id(Command *cmd) {
  if (cmd == NULL)
    return -1;
  return cmd->user_id;
}

int command_get_command_id(Command *cmd) {
  if (cmd == NULL)
    return -1;
  return cmd->command_id;
}

long command_get_created_at(Command *cmd) {
  if (cmd == NULL)
    return 0;
  return cmd->created_at;
}

void command_set_user_id(Command *cmd, int user_id) {
  if (cmd != NULL) {
    cmd->user_id = user_id;
  }
}

void command_set_command_id(Command *cmd, int command_id) {
  if (cmd != NULL) {
    cmd->command_id = command_id;
  }
}

void command_set_created_at(Command *cmd, long created_at) {
  if (cmd != NULL) {
    cmd->created_at = created_at;
  }
}