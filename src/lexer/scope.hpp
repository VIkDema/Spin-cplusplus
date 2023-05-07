#pragma once

#include <array>
#include <string>
#include <vector>

namespace lexer {

class ScopeProcessor {
public:
  ScopeProcessor();
  void InitScopeName(const std::string &init_scope_name){
    curr_scope_name_ = "_";
  }
  void SetCurrScope();
  void AddScope();
  void RemoveScope();

private:
  int scope_level_;
  // TODO: change on vector
  std::array<int, 256> scope_seq_;
  std::string curr_scope_name_;
};

static ScopeProcessor scope_processor_;
} // namespace lexer