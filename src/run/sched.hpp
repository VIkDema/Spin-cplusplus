#pragma once

#include "../models/models_fwd.hpp"
#include <string>

namespace sched {
/**
 * @brief Schedules the processes for execution.
 *
 * This function is used to schedule the execution of processes.
 */
void ScheduleProcesses();

/**
 * @brief Resolves a remote variable reference.
 *
 * This function is used to resolve a remote variable reference.
 *
 * @param lexToken A pointer to the object representing the remote variable reference.
 * @return The integer result of resolving the reference.
 */
int ResolveRemoteVariableReference(models::Lextok *);

/**
 * @brief Resolves a remote label reference.
 *
 * This function is used to resolve a remote label reference.
 *
 * @param lexToken A pointer to the object representing the remote label reference.
 * @return The integer result of resolving the reference.
 */
int ResolveRemoteLabelReference(models::Lextok *);

/**
 * @brief Displays the execution status.
 *
 * This function is used to display the execution status.
 *
 * @param element A pointer to the element object.
 * @param status An integer representing the execution status.
 */
void DisplayExecutionStatus(models::Element *, int);


/**
 * @brief Prints information about the current process.
 *
 * This function is used to print information about the current process.
 *
 * @param info An integer representing the information about the current process.
 */
void PrintCurrentProcessInfo(int);

/**
 * @brief Assigns a local value.
 *
 * This function is used to assign a local value.
 *
 * @param lexToken A pointer to the object representing the value.
 * @param value The integer value to assign.
 * @return The integer result of assigning the value.
 */
int AssignLocalValue(models::Lextok *, int);

/**
 * @brief Gets the local value.
 *
 * This function is used to get the local value.
 *
 * @param lexToken A pointer to the object representing the value.
 * @return The integer local value.
 */
int GetLocalValue(models::Lextok *);

/**
 * @brief Checks if the index is within bounds.
 *
 * This function is used to check if the index is within the bounds.
 *
 * @param symbol A pointer to the symbol object.
 * @param index The integer index value.
 * @return An integer indicating whether the index is within bounds.
 */
int IsIndexInBounds(models::Symbol *, int);

/**
 * @brief Finds or creates a local symbol.
 *
 * This function is used to find or create a local symbol.
 *
 * @param symbol A pointer to the symbol object.
 * @return A pointer to the found or created local symbol.
 */
models::Symbol *FindOrCreateLocalSymbol(models::Symbol *);

/**
 * @brief Performs rendezvous completion.
 *
 * This function is used to perform rendezvous completion.
 *
 * @return An integer representing the result of rendezvous completion.
 */
int PerformRendezvousCompletion();

/**
 * @brief Renames the wrap-up.
 *
 * This function is used to rename the wrap-up.
 *
 * @param name The new name for the wrap-up.
 */
void RenameWrapup(int);

/**
 * @brief Initializes claim execution.
 *
 * This function is used to initialize claim execution.
 *
 * @param executionId The ID for claim execution.
 */
void InitializeClaimExecution(int);

/**
 * @brief Validates the parameter count.
 *
 * This function is used to validate the parameter count.
 *
 * @param count The count of parameters.
 * @param lexToken A pointer to the object representing the parameters.
 */
void ValidateParameterCount(int, models::Lextok *);

/**
 * @brief Activates a process.
 *
 * This function is used to activate a process.
 *
 * @param lexToken A pointer to the object representing the process.
 * @return An integer representing the result of process activation.
 */
int ActivateProcess(models::Lextok *);

/**
 * @brief Displays process creation.
 *
 * This function is used to display process creation.
 *
 * @param name The name of the process.
 */
void DisplayProcessCreation(const std::string &);

/**
 * @brief Increments the maximum element of a symbol.
 *
 * This function is used to increment the maximum element of a symbol.
 *
 * @param symbol A pointer to the symbol object.
 * @return An integer representing the incremented maximum element.
 */
int IncrementSymbolMaxElement(models::Symbol *);

/**
 * @brief Validates MType arguments.
 *
 * This function is used to validate MType arguments.
 *
 * @param arg1 A pointer to the first argument object.
 * @param arg2 A pointer to the second argument object.
 */
void ValidateMTypeArguments(models::Lextok *, models::Lextok *);


/**
 * @brief Creates a process entry.
 *
 * This function is used to create a process entry.
 *
 * @param symbol A pointer to the symbol object.
 * @param lexToken A pointer to the object representing the process.
 * @param sequence A pointer to the sequence object.
 * @param count The count of processes.
 * @param lexToken2 A pointer to the object representing additional parameters.
 * @param btypes The process types.
 * @return A pointer to the created process entry.
 */
models::ProcList *CreateProcessEntry(models::Symbol *, models::Lextok *,
                                     models::Sequence *, int, models::Lextok *,
                                     models::btypes);
/**
 * @brief Initializes a runnable process.
 *
 * This function is used to initialize a runnable process.
 *
 * @param process A pointer to the process list object.
 * @param value1 The first value.
 * @param value2 The second value.
 */
void InitializeRunnableProcess(models::ProcList *, int, int);

/**
 * @brief Adds a tag to a file.
 *
 * This function is used to add a tag to a file.
 *
 * @param fd A pointer to the file descriptor.
 * @param s The tag string.
 */
void DoTag(FILE *fd, char *s);
} // namespace sched
