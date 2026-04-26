#ifndef ROUND_ROBIN_H
#define ROUND_ROBIN_H

#include "scheduler/dispatcher.h"

/**
 * @brief Injeta as funções do Round Robin e a estrutura de dados no Scheduler
 * genérico.
 * @param s Ponteiro para a instância do Scheduler a ser configurada.
 */
void initialize_scheduler_rr(Scheduler *s);

#endif // ROUND_ROBIN_H