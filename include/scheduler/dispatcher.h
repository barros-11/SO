#ifndef DISPATCHER_H
#define DISPATCHER_H

typedef struct scheduler Scheduler;

/** 
 * @brief Initializes the scheduler based on the specified algorithm.
 * @param scheduler_algorithm An integer representing the scheduling algorithm to use (e.g., 0 for FCFS, 1 for SJF).
 * @return A pointer to a Scheduler initialized with the appropriate function pointers and data for the specified algorithm.
 */
Scheduler *initialize_scheduler(int scheduler_algorithm);

/**
 * @brief Adds a command to the scheduler's structure.
 * @param s A pointer to the Scheduler instance.
 * @param user_id The ID of the user who issued the command.
 * @param created_timestamp The timestamp when the command was created.
 * @param command_id The ID of the command to be added.
 */
void addCommand(Scheduler *s, int user_id, double created_timestamp, int command_id);

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

#endif // DISPATCHER_H