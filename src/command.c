#include "command.h"
#include <stdlib.h>

Command *command_create(int user_id, int command_id) {
    Command *cmd = malloc(sizeof(Command));
    if (cmd != NULL) {
        cmd->user_id = user_id;
        cmd->command_id = command_id;
    }
    return cmd;
}

void command_free(Command *cmd) {
    if (cmd != NULL) {
        free(cmd);
    }
}

int command_get_user_id(Command *cmd) {
    if (cmd == NULL) return -1;
    return cmd->user_id;
}

int command_get_command_id(Command *cmd) {
    if (cmd == NULL) return -1;
    return cmd->command_id;
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
