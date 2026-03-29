#ifndef PROTOCOL_H
#define PROTOCOL_H


enum type{
    EXECUTE, 
    LIST, 
    TERMINATED,  // a runner ended
    END // to end the process of listenning to the runners (end the program)
};

typedef struct protocol Protocol; 

typedef enum type Type; 

/**
 * @brief Gets the status/type of the protocol message.
 * * @param p Pointer to the Protocol structure.
 * @return The Type of the message.
 */
Type getType( Protocol *p);

/**
 * @brief Sets the status/type of the protocol message.
 * * @param t The Type to set.
 * @param p Pointer to the Protocol structure to modify.
 */
void setType(Type t, Protocol *p);

/**
 * @brief Sets the runner's PID in the protocol message.
 * * @param pid The PID of the runner.
 * @param p Pointer to the Protocol structure to modify.
 */
void setPid(int pid, Protocol *p);

/**
 * @brief Gets the runner's PID from the protocol message.
 * * @param p Pointer to the Protocol structure.
 * @return The PID of the runner.
 */
int getPid( Protocol *p);


/**
 * @brief Sets the user ID in the protocol message.
 * * @param ui The user ID to set.
 * @param p Pointer to the Protocol structure to modify.
 */
void setUserId(int ui, Protocol *p);

/**
 * @brief Gets the user ID from the protocol message.
 * * @param p Pointer to the Protocol structure.
 * @return The user ID.
 */
int getUserId( Protocol *p);

/**
 * @brief Sets the creation timestamp in the protocol message.
 * * @param ts The timestamp (in milliseconds or seconds) to set.
 * @param p Pointer to the Protocol structure to modify.
 */
void setCreatedTimestamps(long ts, Protocol *p);

/**
 * @brief Gets the creation timestamp from the protocol message.
 * * @param p Pointer to the Protocol structure.
 * @return The creation timestamp.
 */
long getCreatedTimestamps( Protocol *p);

/**
 * @brief Gets the size of the Protocol structure.
 * * @param p Pointer to the Protocol structure (not used, but kept for consistency).
 * @return The size of the Protocol structure in bytes.
 */
int getProtocolSize();

#endif // PROTOCOL_H


