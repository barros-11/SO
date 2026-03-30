#ifndef FCFS_H
#define FCFS_H

#include "request.h"
#include "scheduler/dispatcher.h"

/**
 * @brief Inicializa a estrutura de dados (GQueue) para o escalonamento FCFS.
 * @return Um ponteiro genérico (void *) para a nova fila alocada.
 */
void *initialize_data_fcfs(void);

/**
 * @brief Adiciona um novo comando ao final da fila (First-Come, First-Served).
 * @param user_id ID do utilizador que enviou o comando (ignorado no FCFS).
 * @param created_at Timestamp de quando o comando chegou (ignorado no FCFS).
 * @param command_id ID único do comando gerado pelo Dispatcher.
 * @param data Ponteiro para a estrutura de dados do algoritmo (a GQueue).
 */
void add_command_fcfs(int user_id, double created_at, int command_id,
                      void *data);

/**
 * @brief Retira e devolve o próximo comando a ser executado (o mais antigo da
 * fila).
 * @param data Ponteiro para a estrutura de dados do algoritmo (a GQueue).
 * @return Um ponteiro para a estrutura Request com os dados do próximo comando.
 *         O caller é responsável por fazer free() dessa memória.
 */
Request *get_next_fcfs(void *data);

/**
 * @brief Liberta toda a memória alocada pela fila do FCFS.
 * @param data Ponteiro para a estrutura de dados do algoritmo (a GQueue).
 */
void cleanup_fcfs(void *data);

/**
 * @brief Verifica se existem comandos na fila do escalonador FCFS.
 * @param data Ponteiro para a estrutura de dados do algoritmo (a GQueue).
 * @return 1 (verdadeiro) se a fila tiver comandos, 0 (falso) se estiver vazia.
 */
int has_commands_fcfs(void *data);

/**
 * @brief Lista todos os comandos na fila em formato string.
 * @param data Ponteiro para a estrutura de dados do algoritmo (a GQueue).
 * @return Uma string formatada com a lista de comandos (user-id e command-id).
 *         O caller é responsável por fazer free() desta string.
 */
char *list_commands_fcfs(void *data);

/**
 * @brief Injeta as funções do FCFS e a estrutura de dados no Scheduler
 * genérico.
 * @param s Ponteiro para a instância do Scheduler a ser configurada.
 */
void initialize_scheduler_fcfs(Scheduler *s);

#endif // FCFS_H