/**
 * @file communication.c
 * @brief Implementação das funções de comunicação via FIFOs entre Runner e
 * Controller.
 */

#include "runner/communication.h"
#include "protocol.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SERVER_FIFO "tmp/controller_pipe"

/**
 * @brief Envia uma mensagem de protocolo para o pipe principal do Controller.
 * * @param p Ponteiro para a estrutura Protocol a ser enviada.
 * @return 0 em caso de sucesso, -1 se o pipe não puder ser aberto.
 */
int send_to_controller(Protocol *p) {
  int fd = open(SERVER_FIFO, O_WRONLY);
  if (fd == -1)
    return -1;

  write(fd, p, getProtocolSize());
  close(fd);
  return 0;
}

/**
 * @brief Cria um Named Pipe (FIFO) exclusivo para a resposta ao Runner baseado
 * no seu PID.
 * * @param pid O identificador do processo runner.
 * @return 0 em caso de sucesso, ou o valor de erro do mkfifo.
 */
int create_response_pipe(int pid) {
  char path[64];
  snprintf(path, 64, "tmp/runner_%d", pid);
  return mkfifo(path, 0666);
}

/**
 * @brief Bloqueia a execução até receber um sinal de autorização (GO) do
 * Controller.
 * * @param pid O identificador do processo runner.
 * @return Número de bytes lidos ou -1 em caso de erro.
 */
int wait_for_go(int pid) {
  char path[64];
  snprintf(path, 64, "tmp/runner_%d", pid);

  int fd = open(path, O_RDONLY);
  if (fd == -1)
    return -1;

  char buffer[10];
  int bytes = read(fd, buffer, sizeof(buffer));
  close(fd);
  return bytes;
}

/**
 * @brief Remove o ficheiro de pipe do sistema de ficheiros.
 * * @param pid O identificador do processo runner associado ao pipe.
 */
void cleanup_pipe(int pid) {
  char path[64];
  snprintf(path, 64, "tmp/runner_%d", pid);
  unlink(path);
}

/**
 * @brief Lê a listagem de comandos enviada pelo Controller e imprime-a no
 * terminal.
 * * @param pid O identificador do processo runner.
 */
void read_list_response(int pid) {
  char path[64];
  snprintf(path, 64, "tmp/runner_%d", pid);

  int fd = open(path, O_RDONLY);
  if (fd == -1)
    return;

  char buffer[1024];
  ssize_t bytes_read;

  while ((bytes_read = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
    buffer[bytes_read] = '\0';
    write(STDOUT_FILENO, buffer, bytes_read);
  }

  close(fd);
}