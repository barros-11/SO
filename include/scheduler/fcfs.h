#ifndef FCFS_H
#define FCFS_H

#include "scheduler/dispatcher.h"

/**
 * @brief Injeta as funções do FCFS e a estrutura de dados no Scheduler
 * genérico.
 * @param s Ponteiro para a instância do Scheduler a ser configurada.
 */
void initialize_scheduler_fcfs(Scheduler *s);

#endif // FCFS_H