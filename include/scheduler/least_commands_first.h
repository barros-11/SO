#ifndef LCF_H
#define LCF_H

#include "scheduler/dispatcher.h"

/**
 * @brief Initializes the Least Commands First (LCF) scheduling algorithm.
 * * Configures the provided generic Scheduler structure with the specific
 * data structures and function pointers required to execute the LCF logic.
 * * @param s Pointer to the main Scheduler structure to be initialized.
 */
void initialize_scheduler_lcf(Scheduler *s);

#endif /* LCF_H */