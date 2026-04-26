#ifndef SCHEDULER_COMMAND_H
#define SCHEDULER_COMMAND_H

typedef struct command Command;

Command *command_create(int user_id, int command_id, long created_at);

void command_free(Command *cmd);

int command_get_user_id(Command *cmd);

int command_get_command_id(Command *cmd);

long command_get_created_at(Command *cmd);

void command_set_user_id(Command *cmd, int user_id);

void command_set_command_id(Command *cmd, int command_id);

void command_set_created_at(Command *cmd, long created_at);

#endif // SCHEDULER_COMMAND_H