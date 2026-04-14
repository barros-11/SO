#include "request.h"
#include "scheduler/dispatcher.h"
#include <fcntl.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

void notify_runner(int command_id, char *msg) {
  char path[64];
  snprintf(path, 64, "tmp/runner_%d", command_id);
  int fd = open(path, O_WRONLY);
  if (fd == -1) {
    perror("Error opening runner pipe");
    return;
  }
  if (write(fd, msg, strlen(msg) + 1) == -1)
    perror("Error writing to the pipe");
  close(fd);
}

void wait_for_children(GArray *pids) {
  for (guint i = 0; i < pids->len; i++) {
    int pid = g_array_index(pids, int, i);
    int status;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status)) {
      perror("Error exiting a child process");
    }
  }
}

/**
 * Creates and opens the main named pipe for the controller.
 * Returns the file descriptor, or -1 on failure.
 */
int 
setup_server_pipe() {
  unlink("tmp/controller_pipe"); 
  if (mkfifo("tmp/controller_pipe", 0666) == -1) {
    perror("Error creating the named pipe\n");
    return -1;
  }
  int fd = open("tmp/controller_pipe", O_RDONLY);
  if (fd == -1) {
    unlink("tmp/controller_pipe");
    perror("Error opening the controller named pipe");
    return -1;
  }
  return fd;
}

/**
 * Evaluates whether the main loop should block and wait for a pipe message.
 * returns 1 if it should block 
 * returns 0 if it should continue the loop
 */
int check_wait_condition(int current_commands, int max_commands, int has_cmds, int acabou) {
  if (current_commands == max_commands) {
    return 1; 
  } 
  else if (!has_cmds) {
    if (!acabou) {
      return 1; 
    } else if (current_commands > 0) {
      return 1; 
    }
  }
  return 0;
}

/**
 * Reads from the pipe and handles EOF / reopening logic.
 * Returns: 1 on success, 0 on EOF (pipe reopened), -1 on fatal error.
 */
int handle_pipe_read(int *fd, Request *r) {
  ssize_t size = read(*fd, r, get_request_size());
  
  if (size == get_request_size()) {
    return 1; 
  }
  
  close(*fd);
  unlink("tmp/controller_pipe");
  
  if (size == 0) { 
    mkfifo("tmp/controller_pipe", 0666);
    *fd = open("tmp/controller_pipe", O_RDONLY);
    if (*fd == -1) {
      perror("Error reopening controller pipe");
      return -1;
    }
    return 0; 
  }
  
  perror("Error reading from pipe or incomplete request message\n");
  return -1;
}

/**
 * Processes a single Request structure and updates system states accordingly.
 */
void process_request(Request *r, Scheduler *scheduler, int *acabou, 
                     int *terminate_command_id, int *current_commands, GArray *pids) {
  switch (get_status(r)) {
  case END:
    *acabou = 1;
    *terminate_command_id = get_command_id(r);
    break;
  case TERMINATED:
    remove_command(scheduler, get_command_id(r), r);
    (*current_commands)--;
    break;
  case EXECUTE:
    if (!(*acabou)) {
      add_command(scheduler, get_user_id(r), get_created_timestamps(r), get_command_id(r));
    } else {
      notify_runner(get_command_id(r), "DONT EXECUTE");
    }
    break;
  case LIST: {
    int pid = fork();
    if (pid == 0) {
      char *result = list_commands(scheduler);
      notify_runner(get_command_id(r), result);
      free(result);
      _exit(0);
    } else if (pid > 0) {
      g_array_append_val(pids, pid);
    } else {
      perror("fork failed");
    }
    break;
  }
  }
}

/**
 * Dispatches pending commands from the scheduler up to the max parallel limit.
 */
void dispatch_pending_commands(Scheduler *scheduler, int *current_commands, int max_commands) {
  if (*current_commands < max_commands && has_commands(scheduler)) {
    Request *cmd = get_next(scheduler);
    if (cmd != NULL) {
      int command_id = get_command_id(cmd);
      (*current_commands)++;
      notify_runner(command_id, "GO");
      insert_command(scheduler, cmd); 
      free(cmd);
    }
  }
}



void controller(int max_commands, int scheduler_algorithm) {
  int terminate_command_id = -1;
  int acabou = 0;
  int current_commands = 0; 

  Scheduler *scheduler = initialize_scheduler(scheduler_algorithm);
  if (scheduler == NULL){
    printf("Error creating the scheduler\n"); 
    return;
  }

  GArray *pids = g_array_new(FALSE, FALSE, sizeof(int));
  
  int fd = setup_server_pipe();
  if (fd == -1) {
    cleanup(scheduler);
    g_array_free(pids, TRUE);
    return;
  }

  while (!acabou || current_commands > 0 || has_commands(scheduler)) {
    
    int wait_for_pipe = check_wait_condition(current_commands, max_commands, has_commands(scheduler), acabou);

    if (wait_for_pipe) {
      Request *r = malloc(get_request_size());
      int read_status = handle_pipe_read(&fd, r);
      
      if (read_status == -1) {
        // Fatal read error -> break loop and trigger cleanup at the end
        free(r);
        break; 
      } else if (read_status == 0) {
        // EOF handled successfully -> skip the rest of the loop
        free(r);
        continue;
      }

      // Success -> process the message
      process_request(r, scheduler, &acabou, &terminate_command_id, &current_commands, pids);
      free(r);
    }

    dispatch_pending_commands(scheduler, &current_commands, max_commands);
  }
  
  // Clean up routines
  wait_for_children(pids);
  g_array_free(pids, TRUE);
  cleanup(scheduler);
  if (fd != -1) close(fd);
  unlink("tmp/controller_pipe");
  
  if (terminate_command_id != -1) {
    notify_runner(terminate_command_id, "ENDED"); 
  }
}

int main(int n, char *args[]) {
  if (n == 3)
    controller(atoi(args[1]), atoi(args[2])); 
  else {
    printf("Incorrect number of arguments \n");
    return 1;
  }
  return 0;
}