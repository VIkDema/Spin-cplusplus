#ifndef FATAL_SPIN_H
#define FATAL_SPIN_H

#include <optional>
#include <string>
#include <string_view>
namespace log{
void fatal(const std::string_view &s1,
           const std::optional<std::string> &s2 = std::nullopt);
void non_fatal(const std::string_view &s1,
               const std::optional<std::string> &s2 = std::nullopt);
std::string explainToString(int n);
};
#endif