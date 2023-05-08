#pragma once

#include <array>
#include <string>
#include <vector>

namespace lexer {

class ScopeProcessor {
public:
  ScopeProcessor();
  void InitScopeName();
  void SetCurrScope();
  void AddScope();
  void RemoveScope();
  std::string GetCurrScope();

private:
  int scope_level_;
  std::array<int, 256> scope_seq_;
  std::string curr_scope_name_;
};

} // namespace lexer