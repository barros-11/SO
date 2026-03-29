#ifndef ROUND_ROBIN_H
#define ROUND_ROBIN_H

#include "command.h"
#include <glib.h>

typedef struct scheduler Scheduler;

typedef struct round_robin RoundRobin;
typedef struct job Job;

/**
 * @brief Injeta as funções do Round Robin e a estrutura de dados no Scheduler genérico.
 * @param s Ponteiro para a instância do Scheduler a ser configurada.
 */
void initialize_scheduler_rr(Scheduler *s);

/**
 * @brief Inicializa a estrutura de dados para o escalonamento Round Robin.
 * @return Um ponteiro para a estrutura RoundRobin alocada.
 */
RoundRobin *initialize_data_rr();

/**
 * @brief Adiciona um novo comando à fila do utilizador no Round Robin.
 * @param user_id ID do utilizador que enviou o comando.
 * @param created_at Timestamp de quando o comando chegou (pode ser usado para métricas).
 * @param command_id ID único do comando gerado pelo Dispatcher.
 * @param data Ponteiro para a estrutura de dados do algoritmo (RoundRobin).
 */
void add_command_rr(int user_id, double created_at, int command_id, void *data);

/**
 * @brief Retira e devolve o próximo comando a ser executado segundo a política Round Robin.
 * @param data Ponteiro para a estrutura de dados do algoritmo (RoundRobin).
 * @return Um ponteiro para a estrutura Command com os dados do próximo comando.
 *         O caller é responsável por fazer free() dessa memória.
 */
Command *get_next_rr(void *data);

/**
 * @brief Verifica se existem comandos pendentes no escalonador Round Robin.
 * @param data Ponteiro para a estrutura de dados do algoritmo (RoundRobin).
 * @return 1 (verdadeiro) se houver comandos, 0 (falso) caso contrário.
 */
int has_commands_rr(void *data);

/**
 * @brief Lista todos os comandos pendentes em formato string.
 * @param data Ponteiro para a estrutura de dados do algoritmo (RoundRobin).
 * @return Uma string formatada com a lista de comandos (user-id e command-id).
 *         O caller é responsável por fazer free() desta string.
 */
char *list_commands_rr(void *data);

/**
 * @brief Liberta toda a memória alocada pelo escalonador Round Robin.
 * @param data Ponteiro para a estrutura de dados do algoritmo (RoundRobin).
 */
void cleanup_rr(void *data);

#endif // ROUND_ROBIN_H