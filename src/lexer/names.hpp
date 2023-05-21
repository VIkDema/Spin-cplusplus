#pragma once

#include "y.tab.h"
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

namespace helpers {

struct Name {
  int token;
  int value = 0;
  std::optional<std::string_view> symbol = std::nullopt;
};

std::optional<int> ParseLtlToken(const std::string &value);
std::optional<Name> ParseNameToken(const std::string &value);

} // namespace helpers