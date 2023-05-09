#pragma once

#include <array>
#include <string>
#include <vector>

namespace lexer {

class ScopeProcessor {
public:
  static void InitScopeName();
  static void SetCurrScope();
  static void AddScope();
  static void RemoveScope();
  static std::string GetCurrScope();
  static int GetCurrScopeLevel();
  static int GetCurrSegment();
  static void SetCurrScopeLevel(int scope_level);

private:
  static int scope_level_;
  static std::array<int, 256> scope_seq_;
  static std::string curr_scope_name_;
};

} // namespace lexer