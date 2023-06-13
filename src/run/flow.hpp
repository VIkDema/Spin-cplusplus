#pragma once
#include "../models/models_fwd.hpp"

namespace flow {

/**
 * @brief Dumps labels.
 *
 * This function dumps the labels in the current context.
 */
void DumpLabels();

/**
 * @brief Selects an index from multiple Lextok objects.
 *
 * This function selects an index from the given Lextok objects and returns the selected Lextok.
 *
 * @param lextok1 A pointer to the first Lextok object.
 * @param lextok2 A pointer to the second Lextok object.
 * @param lextok3 A pointer to the third Lextok object.
 * @return A pointer to the selected Lextok.
 */
models::Lextok *SelectIndex(models::Lextok *, models::Lextok *,
                            models::Lextok *);

/**
 * @brief Builds the body for a for loop.
 *
 * This function builds the body for a for loop using the specified Lextok object and iteration count.
 *
 * @param lextok A pointer to the Lextok object representing the body.
 * @param count The iteration count for the for loop.
 * @return A pointer to the constructed Lextok object.
 */
models::Lextok *BuildForBody(models::Lextok *, int);

/**
 * @brief Builds the index for a for loop.
 *
 * This function builds the index for a for loop using the specified Lextok objects.
 *
 * @param lextok1 A pointer to the first Lextok object.
 * @param lextok2 A pointer to the second Lextok object.
 * @return A pointer to the constructed Lextok object representing the index.
 */
models::Lextok *BuildForIndex(models::Lextok *, models::Lextok *);

/**
 * @brief Sets up a for loop.
 *
 * This function sets up a for loop with the specified Lextok objects for initialization, condition, and increment.
 *
 * @param init A pointer to the Lextok object for loop initialization.
 * @param cond A pointer to the Lextok object for loop condition.
 * @param incr A pointer to the Lextok object for loop increment.
 */
void SetupForLoop(models::Lextok *, models::Lextok *, models::Lextok *);

/**
 * @brief Makes a sequence atomic.
 *
 * This function makes the specified Sequence object atomic, ensuring that it executes as an atomic operation.
 *
 * @param seq A pointer to the Sequence object to be made atomic.
 * @param atomicIndex The index of the atomic operation.
 */
void MakeAtomic(models::Sequence *, int);

/**
 * @brief Retrieves the break destination symbol.
 *
 * This function retrieves the break destination symbol for the current context.
 *
 * @return A pointer to the break destination symbol.
 */
models::Symbol *GetBreakDestination();

/**
 * @brief Adds a break destination symbol.
 *
 * This function adds a break destination symbol to the current context.
 */
void AddBreakDestination();

/**
 * @brief Restores the previous break destination symbol.
 *
 * This function restores the previous break destination symbol in the current context.
 */
void RestoreBreakDestinantion();

/**
 * @brief Saves the current break destination symbol.
 *
 * This function saves the current break destination symbol in the current context.
 */
void SaveBreakDestinantion();

/**
 * @brief Finds a label within a symbol range.
 *
 * This function finds a label within the specified symbol range and returns its index.
 *
 * @param start A pointer to the starting symbol of the range.
 * @param end A pointer to the ending symbol of the range.
 * @param labelIndex The index of the label to find.
 * @return The index of the found label, or -1 if not found.
 */
int FindLabel(models::Symbol *, models::Symbol *, int);

/**
 * @brief Fixes a label reference.
 *
 * This function fixes a label reference by updating the specified symbol with the correct label.
 *
 * @param ref A pointer to the symbol referencing the label.
 * @param label A pointer to the symbol representing the label.
 */
void FixLabelRef(models::Symbol *, models::Symbol *);

/**
 * @brief Checks if an Element object has a label.
 *
 * This function checks if the specified Element object has a label of the given index.
 *
 * @param element A pointer to the Element object to check.
 * @param labelIndex The index of the label to search for.
 * @return A pointer to the Symbol object representing the label if found, nullptr otherwise.
 */
models::Symbol *HasLabel(models::Element *, int);

/**
 * @brief Retrieves a label from a Lextok object.
 *
 * This function retrieves the label with the specified index from the given Lextok object.
 *
 * @param lextok A pointer to the Lextok object to search for the label.
 * @param labelIndex The index of the label to retrieve.
 * @return A pointer to the Element object representing the label if found, nullptr otherwise.
 */
models::Element *GetLabel(models::Lextok *, int);

/**
 * @brief Sets a label for a Symbol object.
 *
 * This function sets the specified Element object as a label for the given Symbol object.
 *
 * @param symbol A pointer to the Symbol object to set the label for.
 * @param label A pointer to the Element object representing the label.
 */
void SetLabel(models::Symbol *, models::Element *);


/**
 * @brief Adds a Lextok object to the sequence.
 *
 * This function adds the specified Lextok object to the current sequence.
 *
 * @param lextok A pointer to the Lextok object to be added.
 */
void AddSequence(models::Lextok *);

/**
 * @brief Closes the current sequence.
 *
 * This function closes the current sequence and returns a pointer to the closed Sequence object.
 *
 * @param nottop Flag indicating whether the current sequence is not the top sequence.
 * @return A pointer to the closed Sequence object.
 */
models::Sequence *CloseSequence(int nottop);

/**
 * @brief Opens a new sequence.
 *
 * This function opens a new sequence and makes it the current sequence.
 *
 * @param top Flag indicating whether the new sequence is the top sequence.
 */
void OpenSequence(int top);

/**
 * @brief Ties up any loose ends in the current sequence.
 *
 * This function ties up any loose ends in the current sequence, ensuring proper execution flow.
 */
void TieUpLooseEnds();

/**
 * @brief Performs an "unless" operation.
 *
 * This function performs the "unless" operation by evaluating the specified Lextok objects.
 * If the condition evaluates to false, the body is executed.
 *
 * @param condition A pointer to the Lextok object representing the condition.
 * @param body A pointer to the Lextok object representing the body.
 * @return A pointer to the evaluated Lextok object.
 */
models::Lextok *DoUnless(models::Lextok *, models::Lextok *);

/**
 * @brief Prunes unnecessary options.
 *
 * This function prunes unnecessary options from the specified Lextok object.
 *
 * @param lextok A pointer to the Lextok object to prune options from.
 */
void PruneOpts(models::Lextok *);

/**
 * @brief Crosses Dsteps.
 *
 * This function crosses Dsteps by evaluating the specified Lextok objects.
 *
 * @param lextok1 A pointer to the first Lextok object.
 * @param lextok2 A pointer to the second Lextok object.
 */
void CrossDsteps(models::Lextok *, models::Lextok *);

/**
 * @brief Starts a DStep sequence.
 *
 * This function starts a DStep sequence, indicating the beginning of a DStep operation.
 */
void StartDStepSequence();

/**
 * @brief Ends a DStep sequence.
 *
 * This function ends a DStep sequence, indicating the completion of a DStep operation.
 */
void EndDStepSequence();

/**
 * @brief Sets a label for a Symbol object.
 *
 * This function sets the specified Element object as a label for the given Symbol object.
 *
 * @param symbol A pointer to the Symbol object to set the label for.
 * @param label A pointer to the Element object representing the label.
 */
void SetLabel(models::Symbol *, models::Element *);

} // namespace flow