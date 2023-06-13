#pragma once

#include "../models/models_fwd.hpp"

namespace run {
/**
 * @brief Calculates the highest value within a Lextok object.
 *
 * This function evaluates the given Lextok object and returns the highest value found within it.
 *
 * @param n A pointer to the Lextok object to be evaluated.
 * @return The highest value found within the Lextok object.
 */
int PCHighest(models::Lextok *n);

/**
 * @brief Checks if an Element object is enabled.
 *
 * This function determines whether the specified Element object is enabled or not.
 *
 * @param e A pointer to the Element object to be checked.
 * @return 1 if the Element is enabled, 0 otherwise.
 */
int Enabled(models::Element *);

/**
 * @brief Evaluates a Lextok object and returns the result.
 *
 * This function evaluates the specified Lextok object and returns the computed result.
 *
 * @param now A pointer to the Lextok object to be evaluated.
 * @return The result of evaluating the Lextok object.
 */
int Eval(models::Lextok *now);

/**
 * @brief Performs evaluation or computation on an Element object.
 *
 * This function performs some evaluation or computation on the given Element object
 * and returns a modified or processed version of it.
 *
 * @param e A pointer to the Element object to be evaluated.
 * @return A pointer to the modified or processed Element object.
 */
models::Element *EvalSub(models::Element *e);

} // namespace run