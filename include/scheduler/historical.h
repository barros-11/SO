#ifndef HISTORICAL_H
#define HISTORICAL_H

#include <glib.h>

/**
 * @brief Opaque structure representing the historical records of the system.
 */
typedef struct historical Historical;

/**
 * @brief Loads the historical user statistics from the binary file into memory.
 * * If the binary file does not exist (e.g., first execution), it initializes
 * an empty historical structure.
 * * @return Historical* A pointer to the newly allocated Historical structure.
 */
Historical *load_history(void);

/**
 * @brief Saves the historical user statistics to the binary file.
 * * This function overwrites the existing binary file with the current
 * in-memory statistics. It also automatically calls free_history() to clean up
 * memory before returning.
 * * @param h Pointer to the Historical structure to be saved and freed.
 */
void save_history(Historical *h);

/**
 * @brief Frees all memory associated with the historical records.
 * * This destroys the underlying hash table, automatically freeing all the
 * dynamically allocated UserStats structures inside it, and then frees
 * the wrapper structure itself.
 * * @param h Pointer to the Historical structure to be freed.
 */
void free_history(Historical *h);

/**
 * @brief Retrieves the number of commands executed by a specific user.
 *
 * @param h Pointer to the Historical structure.
 * @param user_id The identifier of the user.
 * @return int The total number of commands executed by the user.
 */
int get_user_number_command(Historical *h, int user_id);

/**
 * @brief Retrieves the total execution time for a specific user's commands.
 *
 * @param h Pointer to the Historical structure.
 * @param user_id The identifier of the user.
 * @return long The total execution time in milliseconds for all commands
 * executed by the user.
 */
long get_user_total_execution_time(Historical *h, int user_id);

/**
 * @brief Increments the total number of commands for a specific user.
 * If the user does not exist in the history, they are added with 1 command run.
 *
 * @param h Pointer to the Historical structure.
 * @param user_id The identifier of the user.
 */
void increment_user_command_count(Historical *h, int user_id);

/**
 * @brief Adds a duration to the total execution time for a specific user.
 * If the user does not exist in the history, they are added with this duration.
 *
 * @param h Pointer to the Historical structure.
 * @param user_id The identifier of the user.
 * @param duration The duration in milliseconds to add.
 */
void increment_user_duration(Historical *h, int user_id, long duration);

#endif /* HISTORICAL_H */