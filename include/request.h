#ifndef PROTOCOL_H
#define PROTOCOL_H

enum status {
  EXECUTE,
  LIST,
  TERMINATED, // a runner ended
  END,        // stop listening to runners and terminate the controller
  WAIT, // type that means the controller needs to wait for this pid that is in
        // the pid of the command
  FLUSH // type only sent by the controller that means when this command is
        // going to be read the controller has to close is file descriptor for
        // writting
};

typedef struct request Request;

typedef enum status Status;

/**
 * @brief Get the request status.
 * @param r Pointer to the request.
 * @return Current request status.
 */
Status get_status(Request *r);

/**
 * @brief Set the request status.
 * @param t New status value.
 * @param r Pointer to the request.
 */
void set_status(Status t, Request *r);

/**
 * @brief Set the request command identifier.
 *
 * In current usage this value is set to the runner PID and used as the
 * command identifier by the controller.
 *
 * @param command_id Command identifier (runner PID in current implementation).
 * @param r Pointer to the request.
 */
void set_command_id(int pid, Request *r);

/**
 * @brief Get the request command identifier.
 * @param r Pointer to the request.
 * @return Command identifier (runner PID in current implementation).
 */
int get_command_id(Request *r);

/**
 * @brief Set the user ID.
 * @param ui User ID value.
 * @param r Pointer to the request.
 */
void set_user_id(int ui, Request *r);

/**
 * @brief Get the user ID.
 * @param r Pointer to the request.
 * @return User ID.
 */
int get_user_id(Request *r);

/**
 * @brief Set request creation timestamp.
 * @param ts Timestamp value.
 * @param r Pointer to the request.
 */
void set_created_timestamps(long ts, Request *r);

/**
 * @brief Get request creation timestamp.
 * @param r Pointer to the request.
 * @return Creation timestamp.
 */
long get_created_timestamps(Request *r);

/**
 * @brief Get the binary size of the request structure.
 * @return Size of the internal `struct request` in bytes.
 */
int get_request_size();

/**
 * @brief Set command duration metadata.
 * @param r Pointer to the request.
 * @param duration Duration value associated with the command.
 */
void set_duration(Request *r, long duration);

/**
 * @brief Get command duration metadata.
 * @param r Pointer to the request.
 * @return Duration value.
 */
long get_duration(Request *r);

#endif // PROTOCOL_H
