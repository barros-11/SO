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
 * * This function overwrites the existing binary file with the current in-memory 
 * statistics. It also automatically calls free_history() to clean up memory 
 * before returning.
 * * @param h Pointer to the Historical structure to be saved and freed.
 */
void save_history(Historical *h);

/**
 * @brief Updates the statistics for a specific user.
 * * If the user does not exist in the historical records, a new entry is created.
 * If the user already exists, their run count is incremented, and the duration 
 * is added to their total execution time.
 * * @param h Pointer to the Historical structure.
 * @param user_id The identifier of the user.
 * @param duration The total turnaround time of the command in milliseconds.
 */
void update_user_stats(Historical *h, int user_id, long duration);

/**
 * @brief Frees all memory associated with the historical records.
 * * This destroys the underlying hash table, automatically freeing all the 
 * dynamically allocated UserStats structures inside it, and then frees 
 * the wrapper structure itself.
 * * @param h Pointer to the Historical structure to be freed.
 */
void free_history(Historical *h);

#endif /* HISTORICAL_H */