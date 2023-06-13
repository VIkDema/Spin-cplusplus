#ifndef FATAL_SPIN_H
#define FATAL_SPIN_H

#include <optional>
#include <string>
#include <string_view>

/**
 * @brief Namespace containing logging functions for error handling and
 * explanation.
 *
 * The `loger` namespace provides functions for logging fatal and non-fatal
 * errors, as well as converting an integer explanation code to a string.
 */
namespace loger {

/**
 * @brief Logs a fatal error with an optional additional message.
 * @param s1 The main error message.
 * @param s2 Optional additional message.
 */
void fatal(const std::string_view &s1,
           const std::optional<std::string> &s2 = std::nullopt);

/**
 * @brief Logs a non-fatal error with an optional additional message.
 * @param s1 The main error message.
 * @param s2 Optional additional message.
 */
void non_fatal(const std::string_view &s1,
               const std::optional<std::string> &s2 = std::nullopt);

/**
 * @brief Converts an explanation code to a string representation.
 * @param n The explanation code.
 * @return The string representation of the explanation code.
 */
std::string explainToString(int n);

} // namespace loger

#endif