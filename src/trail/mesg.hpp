#pragma once
#include "../models/models_fwd.hpp"
#include <string>

namespace mesg {
/**
 * @brief Validates symbol assignment.
 * 
 * @param n Pointer to the Lextok object.
 */
void ValidateSymbolAssignment(models::Lextok *);
/**
 * @brief Checks and processes channel assignment.
 * 
 * @param p Pointer to the left-hand side Lextok object.
 * @param n Pointer to the right-hand side Lextok object.
 * @param d Integer indicating the depth.
 */

void CheckAndProcessChannelAssignment(models::Lextok *, models::Lextok *, int);
/**
 * @brief Prints the contents of a queue.
 * 
 * @param s Pointer to the Symbol object representing the queue.
 * @param n Integer representing the queue index.
 * @param r Pointer to the RunList object.
 */
void PrintQueueContents(models::Symbol *s, int n, models::RunList *r);

/**
 * @brief Formats a message.
 * 
 * @param v Integer value.
 * @param j Integer indicating whether to use Mtype.
 * @param s Reference to the string value.
 */
void FormatMessage(int, int, const std::string &);

/**
 * @brief Prints a formatted message.
 * 
 * @param fd File pointer to the output file.
 * @param v Integer value.
 * @param j Integer indicating whether to use Mtype.
 * @param s Reference to the string value.
 */
void PrintFormattedMessage(FILE *, int, int, const std::string &);

/**
 * @brief Hides a queue.
 * 
 * @param q Integer representing the queue index.
 */
void HideQueue(int);

/**
 * @brief Checks for a type clash.
 * 
 * @param ft Integer representing the 'from' type.
 * @param at Integer representing the 'to' type.
 * @param s Reference to the string value.
 */
void CheckTypeClash(int, int, const std::string &);

/**
 * @brief Performs a receive operation on a queue.
 * 
 * @param n Pointer to the Lextok object.
 * @param full Integer indicating whether to perform a full receive.
 * @return 1 if successful, 0 otherwise.
 */
int QReceive(models::Lextok *, int);

/**
 * @brief Performs a send operation on a queue.
 * 
 * @param n Pointer to the Lextok object.
 * @return 1 if successful, 0 otherwise.
 */
int QSend(models::Lextok *);


/**
 * @brief Checks if a queue is synchronous.
 * 
 * @param n Pointer to the Lextok object.
 * @return 1 if synchronous, 0 otherwise.
 */
int QIsSync(models::Lextok *);

/**
 * @brief Gets the count of mtype parameters.
 * 
 * @param n Pointer to the Lextok object.
 * @return The count of mtype parameters.
 */
int GetCountMPars(models::Lextok *);

/**
 * @brief Creates a queue based on the given symbol.
 * 
 * @param s Pointer to the Symbol object representing the queue.
 * @return The queue ID.
 */
int QMake(models::Symbol *);

/**
 * @brief Checks if a queue is full.
 * 
 * @param n Pointer to the Lextok object.
 * @return 1 if the queue is full, 0 otherwise.
 */
int QFull(models::Lextok *);

/**
 * @brief Gets the length of a queue.
 * 
 * @param n Pointer to the Lextok object.
 * @return The length of the queue.
 */
int QLen(models::Lextok *);

} // namespace mesg