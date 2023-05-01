#pragma once

#include <string>
#include <vector>
#include <array>

namespace lexer {
class ScopeProcessor {
public:
  ScopeProcessor();
  void SetCurrScope();
  void AddScope();
  void RemoveScope();
private:
  int scope_level_;
  //TODO: change on vector
  std::array<int, 256> scope_seq_;
  std::string curr_scope_name_;
};
} // namespace lexer