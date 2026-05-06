#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "request.h"
#include "scheduler/historical.h"

typedef struct scheduler Scheduler;

/**
 * @brief Initializes the scheduler based on the specified algorithm.
 * @param scheduler_algorithm An integer representing the scheduling algorithm
 * to use (e.g., 0 for FCFS, 1 for SJF).
 * @return A pointer to a Scheduler initialized with the appropriate function
 * pointers and data for the specified algorithm.
 */
Scheduler *initialize_scheduler(int scheduler_algorithm);

/**
 * @brief Adds a command to the scheduler's structure.
 * @param s A pointer to the Scheduler instance.
 * @param user_id The ID of the user who issued the command.
 * @param created_timestamp The timestamp when the command was created.
 * @param command_id The ID of the command to be added.
 */
int add_command(Scheduler *s, int user_id, double created_timestamp,
                int command_id);

/**
 * @brief Retrieves the next command to be executed based on the scheduling
 * algorithm.
 * @param s A pointer to the Scheduler instance.
 * @return A pointer to the Request structure containing command_id and user_id.
 *         The caller is responsible for freeing this memory.
 */
Request *get_next(Scheduler *s);

/**
 * @brief Cleans up the scheduler's data and resources.
 * @param s A pointer to the Scheduler instance to be cleaned up.
 */
void cleanup(Scheduler *s);

/**
 * @brief Sets the specific add command function for the active scheduling
 * algorithm.
 * @param s A pointer to the Scheduler instance.
 * @param addCommand A function pointer to the specific implementation.
 */
void set_add_command_function(Scheduler *s,
                              void (*addCommand)(int user_id, double created_at,
                                                 int command_id, void *data));

/**
 * @brief Sets the specific get next function for the active scheduling
 * algorithm.
 * @param s A pointer to the Scheduler instance.
 * @param getNext A function pointer to the specific implementation.
 */
void set_get_next_function(Scheduler *s, Request *(*getNext)(void *data));

/**
 * @brief Sets the specific cleanup function for the active scheduling
 * algorithm.
 * @param s A pointer to the Scheduler instance.
 * @param cleanup A function pointer to the specific implementation.
 */
void set_cleanup_function(Scheduler *s, void (*cleanup)(void *data));

/**
 * @brief Define a estrutura de dados específica a ser usada pelo algoritmo de
 * escalonamento.
 * * Esta função permite injetar a estrutura de dados correta (como uma GQueue
 * para FCFS) no Scheduler genérico, mantendo o encapsulamento da struct.
 * * @param s Um ponteiro para a instância do Scheduler.
 * @param data Um ponteiro genérico (void *) para a estrutura de dados do
 * algoritmo.
 */
void set_data(Scheduler *s, void *data);

/**
 * @brief Verifica se existem comandos à espera de serem executados no
 * escalonador.
 * @param s Um ponteiro para a instância do Scheduler.
 * @return O número de comandos na fila (ou um valor maior que 0 se houver
 * comandos), e 0 se a fila estiver vazia.
 */
int has_commands(Scheduler *s);

/**
 * @brief Define a função específica do algoritmo ativo para verificar a
 * existência de comandos na fila.
 * @param s Um ponteiro para a instância do Scheduler.
 * @param has_commands Um ponteiro de função para a implementação específica do
 * algoritmo.
 */
void set_has_commands_function(Scheduler *s, int (*has_commands)(void *data));

/**
 * @brief Lista todos os comandos no escalonador em formato string.
 * @param s Um ponteiro para a instância do Scheduler.
 * @return Uma string formatada com a lista de todos os comandos.
 *         O caller é responsável por fazer free() desta string.
 */
char *list_commands(Scheduler *s);

/**
 * @brief Define a função específica do algoritmo ativo para listar os comandos.
 * @param s Um ponteiro para a instância do Scheduler.
 * @param listCommands Um ponteiro de função para a implementação específica do
 * algoritmo.
 */
void set_list_commands_function(Scheduler *s,
                                char *(*listCommands)(void *data));

/**
 * @brief Remove um comando específico da lista de comandos em execução.
 * @param s Um ponteiro para a instância do Scheduler.
 * @param command_id O ID do comando a remover.
 * @param r Um ponteiro para a estrutura Request associada ao comando, usada
 * para salvar o log antes de remover.
 * @return 0 se o comando foi encontrado e removido, -1 se o comando não foi
 * encontrado.
 */
int remove_command(Scheduler *s, int command_id, Request *r);

/**
 * @brief Insere um comando na lista de comandos em execução.
 * @param s Um ponteiro para a instância do Scheduler.
 * @param cmd_info Um ponteiro para a estrutura Request a ser inserida.
 */
void insert_command(Scheduler *s, Request *cmd_info);

/**
 * @brief Retrieves the historical records of users from the scheduler.
 * @param s A pointer to the Scheduler instance.
 * @return A pointer to the Historical structure containing user statistics.
 */
Historical *get_users_history(Scheduler *s);

/**
 * @brief Returns the queue of commands currently executing.
 *
 * This queue is owned by the scheduler and must not be modified by callers.
 *
 * @param s A pointer to the Scheduler instance.
 * @return GQueue* Queue with Request pointers for running commands.
 */
GQueue *get_running_commands(Scheduler *s);

#endif // DISPATCHER_H