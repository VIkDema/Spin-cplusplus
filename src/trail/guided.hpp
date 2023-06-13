#pragma once
#include "../models/models_fwd.hpp"

namespace trail {
/**
 * @brief Processes the trail file.
 *
 * This function is responsible for processing the trail file,
 * which contains the recorded execution trail of a program.
 * It performs various tasks such as parsing the file, executing the recorded
 * instructions, and handling any errors or exceptions encountered during the
 * execution.
 *
 * The trail file typically contains a sequence of instructions along with
 * additional metadata, such as program counters, sequence numbers, and variable
 * values. The function reads the trail file, interprets each instruction, and
 * executes them in the order they appear in the file. It maintains the state of
 * the program and variables during the execution.
 *
 * @note This function may throw exceptions or raise errors
 */
void ProcessTrailFile();
/**
 * @brief Retrieves the value of the program counter for a given node.
 * @param n The node for which to retrieve the program counter.
 * @return The value of the program counter.
 */
int GetProgramCounterValue(models::Lextok *);

/**
 * @brief Finds the minimum sequence number in a given sequence.
 * @param s The sequence for which to find the minimum sequence number.
 * @return The minimum sequence number.
 */
int FindMinSequence(models::Sequence *);
/**
 * @brief Finds the maximum sequence number in a given sequence.
 * @param s The sequence for which to find the maximum sequence number.
 * @return The maximum sequence number.
 */
int FindMaxSequence(models::Sequence *);
} // namespace trail