/**
 * @file runner.c
 * @brief Ponto de entrada para o componente Runner do sistema de orquestração.
 * * Este componente atua como cliente, enviando pedidos de execução, consulta
 * e encerramento ao Controller através de Named Pipes (FIFOs).
 */

#include "runner/runner.h"
#include "request.h"
#include "runner/communication.h"
#include "runner/executor.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

/**
 * @brief Imprime uma mensagem no terminal com a etiqueta padrão [runner].
 * * @param message A mensagem a ser exibida.
 */
void print_runner_msg(char *message) {
  char buffer[256];
  int len = snprintf(buffer, sizeof(buffer), "[runner] %s\n", message);
  if (len > 0) {
    write(STDOUT_FILENO, buffer, len);
  }
}

/**
 * @brief Função principal do Runner.
 * * Gere os argumentos da linha de comandos e despacha para os modos:
 * - EXECUÇÃO (-e): Submete um comando para ser escalonado e executado.
 * - CONSULTA (-c): Lista os comandos em execução e em espera no sistema.
 * - SHUTDOWN (-s): Notifica o controller para encerrar os serviços.
 */
int main(int argc, char *argv[]) {
  // Validação mínima de argumentos: ./runner <modo> [args]
  if (argc < 2) {
    write(STDERR_FILENO, "Uso: ./runner [-e user_id cmd | -c | -s]\n", 41);
    return 1;
  }

  Request *r = malloc(get_request_size());
  int my_pid = getpid();
  char msg[128];

  set_command_id(my_pid, r);

  struct timeval tv;
  gettimeofday(&tv, NULL);
  long timestamp = tv.tv_sec * 1000 + tv.tv_usec / 1000;
  set_created_timestamps(timestamp, r);
  set_duration(r, 0);

  if (create_response_pipe(my_pid) !=
      0) { // criar a pipe de resposta do controller para o runner
    perror("[runner] Erro ao criar pipe de resposta");
    free(r);
    return 1;
  }

  int fd = open_controller_connection();
  if (fd == -1) {
    perror("[runner] Error opening the controller pipe");
    cleanup_pipe(my_pid);
    free(r);
    return 1;
  }

  // --- MODO DE EXECUÇÃO (-e) ---
  if (strcmp(argv[1], "-e") == 0 && argc >= 4) {
    set_status(EXECUTE, r);
    set_user_id(atoi(argv[2]), r);

    if (send_to_controller(r, fd) != 0) {
      print_runner_msg(
          "Erro: o controller não está a correr ou o pipe principal falhou");
      cleanup_pipe(my_pid);
      free(r);
      return 1;
    }

    snprintf(msg, sizeof(msg), "command %d submitted", my_pid);
    print_runner_msg(msg);

    if (wait_for_go(my_pid) == -1) {
      print_runner_msg("Controller não aceita mais comandos");
      cleanup_pipe(my_pid);
      free(r);
      return 1;
    }

    snprintf(msg, sizeof(msg), "executing command %d...", my_pid);
    print_runner_msg(msg);

    struct timeval tv_start;
    gettimeofday(&tv_start, NULL);
    long start_time = tv_start.tv_sec * 1000 + tv_start.tv_usec / 1000;

    char **argv_exec = NULL;
    int argc_exec = 0;
    if (!g_shell_parse_argv(argv[3], &argc_exec, &argv_exec, NULL)) {
      print_runner_msg("Erro ao parsear o comando");
      cleanup_pipe(my_pid);
      free(r);
      return 1;
    }
    execute_command(argc_exec, argv_exec, fd);

    for (int i = 0; i < argc_exec; i++) {
      if (argv_exec[i] != NULL) {
        g_free(argv_exec[i]);
      }
    }
    g_free(argv_exec);

    struct timeval tv_end;
    gettimeofday(&tv_end, NULL);
    long end_time = tv_end.tv_sec * 1000 + tv_end.tv_usec / 1000;
    long duration_execution = end_time - start_time;
    long duration_since_creation = end_time - get_created_timestamps(r);

    set_status(TERMINATED, r);
    set_created_timestamps(duration_since_creation, r);
    set_duration(r, duration_execution);
    send_to_controller(r, fd);
    close_controller_connection(fd);

    snprintf(msg, sizeof(msg), "command %d finished", my_pid);
    print_runner_msg(msg);
  }
  // --- MODO DE CONSULTA (-c) ---
  else if (strcmp(argv[1], "-c") == 0) {
    set_status(LIST, r);

    if (send_to_controller(r, fd) != 0) {
      print_runner_msg("[runner] Erro: o controller não está a correr");
      cleanup_pipe(my_pid);
      free(r);
      return 1;
    }
    close_controller_connection(fd);
    read_list_response(my_pid);
  }
  // --- MODO DE ENCERRAMENTO (-s) ---
  else if (strcmp(argv[1], "-s") == 0) {
    set_status(END, r);

    if (send_to_controller(r, fd) != 0) {
      print_runner_msg("[runner] Erro: o controller não está a correr");
      cleanup_pipe(my_pid);
      free(r);
      return 1;
    }
    close_controller_connection(fd);

    print_runner_msg("sent shutdown notification");
    print_runner_msg("waiting for controller to shutdown....");

    wait_for_go(my_pid);
    print_runner_msg("controller exited.");
  } else {
    write(STDERR_FILENO, "[runner] Erro: Argumento invalido\n", 25);
    cleanup_pipe(my_pid);
    free(r);
    return 1;
  }
  cleanup_pipe(my_pid);
  free(r);
  return 0;
}