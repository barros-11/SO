#ifndef COMMAND_H
#define COMMAND_H

/**
 * @brief Estrutura pública que representa um comando.
 * Esta é a interface uniforme usada por todos os algoritmos de escalonamento.
 * Cada algoritmo pode ter sua própria estrutura interna, mas sempre devolve este Command.
 */
typedef struct {
    int user_id;      /**< ID do utilizador que emitiu o comando */
    int command_id;   /**< ID único do comando */
} Command;

/**
 * @brief Cria um novo comando com os valores fornecidos.
 * @param user_id ID do utilizador.
 * @param command_id ID único do comando.
 * @return Ponteiro para a nova estrutura Command alocada em memória.
 *         O caller é responsável por fazer free() dessa memória.
 */
Command *command_create(int user_id, int command_id);

/**
 * @brief Liberta a memória de um comando.
 * @param cmd Ponteiro para a estrutura Command a ser libertada.
 */
void command_free(Command *cmd);

/**
 * @brief Obtém o ID do utilizador de um comando.
 * @param cmd Ponteiro para a estrutura Command.
 * @return O ID do utilizador.
 */
int command_get_user_id(Command *cmd);

/**
 * @brief Obtém o ID do comando.
 * @param cmd Ponteiro para a estrutura Command.
 * @return O ID do comando.
 */
int command_get_command_id(Command *cmd);

/**
 * @brief Define o ID do utilizador de um comando.
 * @param cmd Ponteiro para a estrutura Command.
 * @param user_id O novo ID do utilizador.
 */
void command_set_user_id(Command *cmd, int user_id);

/**
 * @brief Define o ID do comando.
 * @param cmd Ponteiro para a estrutura Command.
 * @param command_id O novo ID do comando.
 */
void command_set_command_id(Command *cmd, int command_id);

#endif // COMMAND_H
