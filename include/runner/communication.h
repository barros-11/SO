#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include "request.h"

/**
 * @brief Abre a ligação ao pipe principal do Controller.
 *
 * @return O descritor de ficheiro (fd) em caso de sucesso, ou -1 em caso de
 * erro.
 */
int open_controller_connection();

/**
 * @brief Fecha a ligação ao pipe principal do Controller.
 *
 * @param fd O descritor de ficheiro a fechar.
 * @return O retorno da função close.
 */
int close_controller_connection(int fd);

/**
 * @brief Envia uma mensagem de protocolo para o pipe principal do Controller.
 *
 * @param r Ponteiro para a estrutura Request a ser enviada.
 * @param fd O descritor de ficheiro do pipe do Controller.
 * @return 0 em caso de sucesso, -1 se ocorrer algum erro.
 */
int send_to_controller(Request *r, int fd);

int create_response_pipe(int pid);

int wait_for_go(int pid);

void cleanup_pipe(int pid);

void read_list_response(int pid);

#endif