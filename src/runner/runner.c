/**
 * @file runner.c
 * @brief Ponto de entrada para o componente Runner do sistema de orquestração.
 * * Este componente atua como cliente, enviando pedidos de execução, consulta
 * e encerramento ao Controller através de Named Pipes (FIFOs).
 */

#include "runner/runner.h"
#include "protocol.h"
#include "runner/communication.h"
#include "runner/executor.h"
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
    // Uso de write para garantir atomicidade e cumprir restrições da UC
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

  Protocol *p = malloc(getProtocolSize());
  int my_pid = getpid();
  char msg[128];

  setPid(my_pid, p);

  struct timeval tv;
  gettimeofday(&tv, NULL);
  long timestamp = tv.tv_sec * 1000 + tv.tv_usec / 1000;
  setCreatedTimestamps(timestamp, p);

  // --- MODO DE EXECUÇÃO (-e) ---
  if (strcmp(argv[1], "-e") == 0 && argc >= 4) {
    setType(EXECUTE, p);
    setUserId(atoi(argv[2]), p);

    create_response_pipe(my_pid);

    if (send_to_controller(p) != 0) {
      print_runner_msg(
          "Erro: o controller não está a correr ou o pipe principal falhou");
      cleanup_pipe(my_pid);
      free(p);
      return 1;
    }

    snprintf(msg, sizeof(msg), "command %d submitted", my_pid);
    print_runner_msg(msg);

    wait_for_go(my_pid);

    snprintf(msg, sizeof(msg), "executing command %d...", my_pid);
    print_runner_msg(msg);

    execute_command(argc - 3, &argv[3]);

    setType(TERMINATED, p);
    send_to_controller(p);

    snprintf(msg, sizeof(msg), "command %d finished", my_pid);
    print_runner_msg(msg);
  }
  // --- MODO DE CONSULTA (-c) ---
  else if (strcmp(argv[1], "-c") == 0) {
    setType(LIST, p);

    if (create_response_pipe(my_pid) != 0) {
      perror("[runner] Erro ao criar pipe de resposta");
      free(p);
      return 1;
    }

    if (send_to_controller(p) != 0) {
      print_runner_msg("Erro: o controller não está a correr");
      cleanup_pipe(my_pid);
      free(p);
      return 1;
    }

    read_list_response(my_pid);
  }
  // --- MODO DE ENCERRAMENTO (-s) ---
  else if (strcmp(argv[1], "-s") == 0) {
    setType(END, p);

    if (create_response_pipe(my_pid) != 0) {
      perror("[runner] Erro ao criar pipe de resposta");
      free(p);
      return 1;
    }

    if (send_to_controller(p) != 0) {
      print_runner_msg("Erro: o controller não está a correr");
      cleanup_pipe(my_pid);
      free(p);
      return 1;
    }

    print_runner_msg("sent shutdown notification");
    print_runner_msg("waiting for controller to shutdown....");

    wait_for_go(my_pid);
    print_runner_msg("controller exited.");
  }

  cleanup_pipe(my_pid);
  free(p);
  return 0;
}