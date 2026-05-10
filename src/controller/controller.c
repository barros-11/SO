#include "request.h"
#include "scheduler/dispatcher.h"
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static long now_ms(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000L + tv.tv_usec / 1000L;
}

static void controller_feedback(const char *fmt, ...) {
  char buf[128];
  va_list args;

  va_start(args, fmt);
  int len = vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  if (len > 0)
    write(STDOUT_FILENO, buf, (size_t)len);
}

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

/**
 * Creates and opens the main named pipe for the controller.
 * Returns the file descriptor, or -1 on failure.
 */
int setup_server_pipe(int *fd_read, int *fd_write) {
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
  *fd_read = fd;
  fd = open("tmp/controller_pipe",
            O_WRONLY); // Opens for write so it doesnt receives the EOF of any
                       // runner doesnt have the descriptor open
  if (fd == -1) {
    unlink("tmp/controller_pipe");
    perror("Error opening the controller named pipe");
    return -1;
  }
  *fd_write = fd;
  return 1;
}

/**
 * Evaluates whether the main loop should block and wait for a pipe message.
 * returns 1 if it should block
 * returns 0 if it should continue the loop
 */
int check_wait_condition(int current_commands, int max_commands, int has_cmds,
                         int is_pipe_empty) {
  if (is_pipe_empty)
    return 0; // if there is nothing to read we cant read

  if (current_commands ==
      max_commands) { // if the max number of commands are running we need to
                      // read to see if we get the terminated from any runner
    return 1;
  } else if (!has_cmds) { // if there aren't any commands in the scheduler we
                          // need to read to wait for new execute commands
    return 1;
  }
  return 0; // if we have open slots and commands in the queue, skip reading to
            // dispatch!
}

/**
 * Reads from the pipe and handles EOF / reopening logic.
 * Returns: 1 on success, 0 on EOF (pipe reopened), -1 on fatal error.
 */
int handle_pipe_read(int *fd, Request *r, int *is_pipe_empty) {
  ssize_t size = read(*fd, r, get_request_size());

  if (size == get_request_size()) { // it awalys has to read a request to be
                                    // correct (it can never be 0 bytes because
                                    // it has the fd_write open)
    return 1;
  }

  if (size == 0) {
    *is_pipe_empty = 1;
    return 0;
  }
  close(*fd);
  unlink("tmp/controller_pipe");

  perror("Error reading from pipe or incomplete request message\n");
  return -1;
}

/**
 * Processes a single Request structure and updates system states accordingly.
 */
void process_request(Request *r, Scheduler *scheduler, int *acabou,
                     int *terminate_command_id, int *current_commands,
                     int fd_write) {
  switch (get_status(r)) {
  case END: {
    *acabou = 1;
    unlink(
        "tmp/controller_pipe"); // We unlink so we cant send any other commands
    // we dont close the fd_write because the processes from the list command
    // might need them
    *terminate_command_id = get_command_id(r);
    controller_feedback("[Controller] Shutdown requested\n");
    Request *req = malloc(get_request_size());
    set_status(FLUSH, req);
    write(fd_write, req, get_request_size());
    free(req);
    break;
  }
  case TERMINATED:
    int result_remove = remove_command(scheduler, get_command_id(r), r);
    if (result_remove != -1)
      (*current_commands)--;

    controller_feedback("[Controller] Command %d from user %d terminated!\n",
                        get_command_id(r), get_user_id(r));

    break;
  case EXECUTE:
    add_command(scheduler, get_user_id(r), get_created_at(r),
                get_command_id(r));
    controller_feedback("[Controller] Command %d from user %d received\n",
                        get_command_id(r), get_user_id(r));
    break;
  case LIST: {
    controller_feedback("[Controller] List requested\n");
    int pid = fork();
    if (pid == 0) {
      char *result = list_commands(scheduler);
      notify_runner(get_command_id(r), result);
      free(result);
      Request *req = malloc(get_request_size());
      set_command_id(getpid(),
                     req); // put in the command id the pid to wait for
      set_status(WAIT, req);
      write(fd_write, req,
            get_request_size()); // send to the controller pipe a message to
                                 // wait for this process (it wont have to wait
                                 // for it to finish because it is sent in the
                                 // end of his execution)
      free(req);
      _exit(0);
    } else if (pid < 0) {
      perror("fork failed");
    }
    break;
  }
  case FLUSH:
    close(fd_write);
    break;
  case WAIT: {
    int status;
    waitpid(get_command_id(r), &status, 0);
    if (!WIFEXITED(status))
      perror("Erro a esperar pelo processo");
    break;
  }
  }
}

/**
 * Dispatches pending commands from the scheduler up to the max parallel limit.
 */
void dispatch_pending_commands(Scheduler *scheduler, int *current_commands,
                               int max_commands) {
  if (*current_commands < max_commands && has_commands(scheduler)) {
    Request *cmd = get_next(scheduler);
    if (cmd != NULL) {
      int command_id = get_command_id(cmd);
      long started_at = now_ms();
      (*current_commands)++;
      set_started_at(cmd, started_at);
      notify_runner(command_id, "GO");

      controller_feedback(
          "[Controller] Command %d from user %d started executing!\n",
          command_id, get_user_id(cmd));

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
  if (scheduler == NULL) {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "Error creating the scheduler\n");
    write(STDERR_FILENO, buffer, strlen(buffer));
    return;
  }

  int fd_read;
  int fd_write;
  if (setup_server_pipe(&fd_read, &fd_write) == -1 || fd_read == -1 ||
      fd_write == -1) {
    cleanup(scheduler);
    return;
  }

  int is_pipe_empty = 0;

  while (!acabou || current_commands > 0 || has_commands(scheduler) ||
         !is_pipe_empty) {

    int wait_in_pipe = check_wait_condition(
        current_commands, max_commands, has_commands(scheduler), is_pipe_empty);

    if (wait_in_pipe) {
      Request *r = malloc(get_request_size());
      int read_status = handle_pipe_read(&fd_read, r, &is_pipe_empty);

      if (read_status == -1) {
        // Fatal read error -> break loop and trigger cleanup at the end
        free(r);
        break;
      }

      // Success -> process the message
      if (read_status == 1)
        process_request(r, scheduler, &acabou, &terminate_command_id,
                        &current_commands, fd_write);
      free(r);
    }

    dispatch_pending_commands(scheduler, &current_commands, max_commands);
  }

  // Clean up routines
  cleanup(scheduler);
  if (fd_read != -1)
    close(fd_read);
  unlink("tmp/controller_pipe");

  if (terminate_command_id != -1) {
    notify_runner(terminate_command_id, "ENDED");
  }
}

int main(int n, char *args[]) {
  if (n == 3)
    controller(atoi(args[1]), atoi(args[2]));
  else {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "Incorrect number of arguments\n");
    write(STDERR_FILENO, buffer, strlen(buffer));
    return 1;
  }
  return 0;
}