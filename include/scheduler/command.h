#ifndef SCHEDULER_COMMAND_H
#define SCHEDULER_COMMAND_H

typedef struct command Command;

Command *command_create(int user_id, int command_id);

void command_free(Command *cmd);

int command_get_user_id(Command *cmd);

int command_get_command_id(Command *cmd);

void command_set_user_id(Command *cmd, int user_id);

void command_set_command_id(Command *cmd, int command_id);

#endif // SCHEDULER_COMMAND_H