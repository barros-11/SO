#ifndef DISPATCHER_H
#define DISPATCHER_H

typedef struct scheduler Scheduler;

/** 
 * @brief Initializes the scheduler based on the specified algorithm.
 * @param scheduler_algorithm An integer representing the scheduling algorithm to use (e.g., 0 for FCFS, 1 for SJF).
 * @return A Scheduler struct initialized with the appropriate function pointers and data for the specified algorithm.
 */
Scheduler* initialize_scheduler(int scheduler_algorithm);

/**
 * @brief Adds a command to the scheduler's structure.
 * @param s A pointer to the Scheduler instance.
 * @param user_id The ID of the user who issued the command.
 * @param created_timestamp The timestamp when the command was created.
 * @param command_id The ID of the command to be added.
 */
int addCommand(Scheduler *s, int user_id, double created_timestamp);

/**
 * @brief Retrieves the next command to be executed based on the scheduling algorithm.
 * @param s A pointer to the Scheduler instance.
 * @return The ID of the next command to be executed.
 */
int getNext(Scheduler *s);

/** 
 * @brief Cleans up the scheduler's data and resources.
 * @param s A pointer to the Scheduler instance to be cleaned up.
 */
void cleanup(Scheduler *s);

/**
 * @brief Sets the specific add command function for the active scheduling algorithm.
 * @param s A pointer to the Scheduler instance.
 * @param addCommand A function pointer to the specific implementation.
 */
void set_add_command_function(Scheduler *s, void (*addCommand)(int user_id, double created_timestamps, int command_id, void *data));

/**
 * @brief Sets the specific get next function for the active scheduling algorithm.
 * @param s A pointer to the Scheduler instance.
 * @param getNext A function pointer to the specific implementation.
 */
void set_get_next_function(Scheduler *s, int (*getNext)(void *data));

/**
 * @brief Sets the specific cleanup function for the active scheduling algorithm.
 * @param s A pointer to the Scheduler instance.
 * @param cleanup A function pointer to the specific implementation.
 */
void set_cleanup_function(Scheduler *s, void (*cleanup)(void *data));

/**
 * @brief Define a estrutura de dados específica a ser usada pelo algoritmo de escalonamento.
 * * Esta função permite injetar a estrutura de dados correta (como uma GQueue para FCFS) 
 * no Scheduler genérico, mantendo o encapsulamento da struct.
 * * @param s Um ponteiro para a instância do Scheduler.
 * @param data Um ponteiro genérico (void *) para a estrutura de dados do algoritmo.
 */
void set_data(Scheduler *s, void *data);

/**
 * @brief Verifica se existem comandos à espera de serem executados no escalonador.
 * @param s Um ponteiro para a instância do Scheduler.
 * @return O número de comandos na fila (ou um valor maior que 0 se houver comandos), e 0 se a fila estiver vazia.
 */
int has_commands(Scheduler *s);

/**
 * @brief Define a função específica do algoritmo ativo para verificar a existência de comandos na fila.
 * @param s Um ponteiro para a instância do Scheduler.
 * @param has_commands Um ponteiro de função para a implementação específica do algoritmo.
 */
void set_has_commands(Scheduler *s, int (*has_commands)(void *data));

#endif // DISPATCHER_H