/**
 * @file executor.c
 * @brief Módulo responsável pela execução de comandos, gestão de pipes e
 * redirecionamentos.
 */

#include "runner/executor.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_CMDS 20

/**
 * @brief Ajusta o array de argumentos removendo elementos processados.
 * * @param argv Array de strings com os argumentos.
 * @param index Índice inicial da remoção.
 * @param count Número de elementos a remover.
 */
static void remove_args(char *argv[], int index, int count) {
  int i;
  for (i = index; argv[i + count] != NULL; i++) {
    argv[i] = argv[i + count];
  }
  argv[i] = NULL;
}

/**
 * @brief Configura redirecionamentos de entrada, saída e erro ( <, >, 2> ).
 * * @param argv Array de argumentos a ser analisado e limpo.
 */
static void setup_redirections(char *argv[]) {
  int i = 0;
  while (argv[i] != NULL) {
    int fd = -1;
    int flags = 0;
    int target_fd = -1;

    if (strcmp(argv[i], ">") == 0) {
      target_fd = STDOUT_FILENO;
      flags = O_WRONLY | O_CREAT | O_TRUNC;
    } else if (strcmp(argv[i], "2>") == 0) {
      target_fd = STDERR_FILENO;
      flags = O_WRONLY | O_CREAT | O_TRUNC;
    } else if (strcmp(argv[i], "<") == 0) {
      target_fd = STDIN_FILENO;
      flags = O_RDONLY;
    }

    if (target_fd != -1) {
      if (argv[i + 1] == NULL) {
        _exit(1);
      }

      fd = open(argv[i + 1], flags, 0666);
      if (fd == -1) {
        perror("[runner] open redirection");
        _exit(1);
      }

      if (dup2(fd, target_fd) == -1) {
        perror("[runner] dup2");
        _exit(1);
      }
      close(fd);

      remove_args(argv, i, 2);
    } else {
      i++;
    }
  }
}

/**
 * @brief Configura o ambiente do processo filho e executa o comando.
 * * @param args Array de argumentos do comando.
 * @param in_fd Descritor de leitura do pipe (ou -1).
 * @param out_fd Descritor de escrita do pipe (ou -1).
 * @param pipe_to_close Descritor de pipe a ser fechado imediatamente (ou -1).
 */
static void run_child_process(char **args, int in_fd, int out_fd,
                              int pipe_to_close) {
  if (in_fd != -1) {
    if (dup2(in_fd, STDIN_FILENO) == -1) {
      perror("[runner] dup2 stdin");
      _exit(1);
    }
    close(in_fd);
  }
  if (out_fd != -1) {
    if (dup2(out_fd, STDOUT_FILENO) == -1) {
      perror("[runner] dup2 stdout");
      _exit(1);
    }
    close(out_fd);
  }
  if (pipe_to_close != -1) {
    close(pipe_to_close);
  }

  setup_redirections(args);

  if (args[0] == NULL)
    _exit(0);

  execvp(args[0], args);
  perror("[runner] execvp");
  _exit(1);
}

/**
 * @brief Divide o array de argumentos original em múltiplos sub-comandos
 * baseados no operador pipe.
 * * @param argc Número total de argumentos.
 * @param argv Array de argumentos original.
 * @param commands Array de ponteiros para armazenar o início de cada comando.
 * @param max_cmds Limite máximo de comandos suportados.
 * @return Número de comandos identificados.
 */
static int parse_pipe_commands(int argc, char *argv[], char **commands[],
                               int max_cmds) {
  int count = 0;
  if (argc == 0 || argv[0] == NULL)
    return 0;

  commands[count++] = &argv[0];
  for (int i = 0; i < argc; i++) {
    if (argv[i] != NULL && strcmp(argv[i], "|") == 0) {
      argv[i] = NULL;
      if (count < max_cmds) {
        commands[count++] = &argv[i + 1];
      }
    }
  }
  return count;
}

/**
 * @brief Executa uma cadeia de comandos (simples ou em pipe).
 * * @param argc Número de argumentos recebidos do runner.
 * @param argv Vetor de strings com o comando e argumentos.
 * @return Status de saída do último comando da cadeia.
 */
int execute_command(int argc, char *argv[]) {
  char **commands[MAX_CMDS];
  pid_t pids[MAX_CMDS];
  int num_cmds = parse_pipe_commands(argc, argv, commands, MAX_CMDS);

  int prev_read_fd = -1;
  int pipe_fds[2];

  for (int i = 0; i < num_cmds; i++) {
    int is_last = (i == num_cmds - 1);

    if (!is_last) {
      if (pipe(pipe_fds) == -1) {
        perror("[runner] pipe");
        return -1;
      }
    }

    pids[i] = fork();
    if (pids[i] == -1) {
      perror("[runner] fork");
      return -1;
    }

    if (pids[i] == 0) {
      int input = prev_read_fd;
      int output = is_last ? -1 : pipe_fds[1];
      int to_close = is_last ? -1 : pipe_fds[0];
      run_child_process(commands[i], input, output, to_close);
    }

    if (prev_read_fd != -1)
      close(prev_read_fd);
    if (!is_last) {
      close(pipe_fds[1]);
      prev_read_fd = pipe_fds[0];
    }
  }

  int status, last_exit_status = 0;
  for (int i = 0; i < num_cmds; i++) {
    if (waitpid(pids[i], &status, 0) != -1) {
      if (i == num_cmds - 1) {
        if (WIFEXITED(status))
          last_exit_status = WEXITSTATUS(status);
      }
    }
  }

  return last_exit_status;
}