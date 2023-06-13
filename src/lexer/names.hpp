#pragma once

#include "y.tab.h"
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

namespace helpers {
/**
 * @brief Represents a name token with associated values.
 */
struct Name {
  int token;     /**< The token value. */
  int value = 0; /**< The default value. */
  std::optional<std::string_view> symbol =
      std::nullopt; /**< An optional symbol. */
};

/**
 * @brief Parses an LTL token from the given string.
 * @param value The string to parse.
 * @return An optional integer value if parsing is successful, otherwise
 * std::nullopt.
 */
std::optional<int> ParseLtlToken(const std::string &value);

/**
 * @brief Parses a name token from the given string.
 * @param value The string to parse.
 * @return An optional Name object if parsing is successful, otherwise
 * std::nullopt.
 */
std::optional<Name> ParseNameToken(const std::string &value);

} // namespace helpers