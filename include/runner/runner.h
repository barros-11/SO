/**
 * @file runner.h
 * @brief Definições das funções de interface do componente Runner.
 */

#ifndef RUNNER_RUNNER_H
#define RUNNER_RUNNER_H

/**
 * @brief Imprime uma mensagem no terminal com a etiqueta padrão [runner].
 * * Esta função utiliza a system call write para garantir que a saída seja
 * atómica e compatível com os requisitos de concorrência da UC, evitando
 * problemas de intercalação de texto.
 * * @param message A string com a mensagem a ser exibida.
 */
void print_runner_msg(char *message);

#endif /* RUNNER_RUNNER_H */