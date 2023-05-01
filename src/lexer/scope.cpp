#include "scope.hpp"

#include "../spin.hpp"
#include <fmt/core.h>

extern Symbol *context;

namespace lexer {
ScopeProcessor::ScopeProcessor() : scope_level_(0) {}
void ScopeProcessor::SetCurrScope() {
  curr_scope_name_ = "_";

  if (context == nullptr) {
    return;
  }
  for (int i = 0; i < scope_level_; i++) {
    curr_scope_name_ += fmt::format("{}_", scope_seq_[i]);
  }
}
void ScopeProcessor::AddScope() { scope_seq_[scope_level_++]++; }
void ScopeProcessor::RemoveScope() { scope_level_--; }
} // namespace lexer