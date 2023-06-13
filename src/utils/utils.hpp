#pragma once
#include <string>

namespace utils {
/**
 * @brief Calculates the hash value of a given string.
 *
 * This function calculates the hash value of the provided string using an
 * unspecified algorithm.
 *
 * @param s The input string to calculate the hash value for.
 * @return The calculated hash value as an unsigned integer.
 */
unsigned int hash(const std::string &s);
} // namespace utils
