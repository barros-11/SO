#ifndef EXECUTOR_H
#define EXECUTOR_H

/**
 * @brief Executa um comando com argumentos, tratando redirecionamentos.
 * * Esta função deve implementar o suporte a >, 2>, < e |[cite: 63].
 * Não deve utilizar a função system() ou o interpretador bash[cite: 37].
 * * @param argc Número de argumentos no array.
 * @param argv Array de strings contendo o comando e os seus argumentos.
 * @return Retorna o status de saída do comando executado.
 */
int execute_command(int argc, char *argv[]);

#endif // EXECUTOR_H