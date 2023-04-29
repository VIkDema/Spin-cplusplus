#include "verbose.hpp"

#include <time.h>

namespace utils::verbose {

Flags::Flags() {}

Flags::Flags(const Flags &) {}

Flags &Flags::operator=(Flags &) {
  auto flags = Flags();
  return flags;
}

bool Flags::NeedToPrintGlobalVariables() {
  return need_to_print_global_variables_ && !clean_;
}

bool Flags::NeedToPrintLocalVariables() {
  return need_to_print_local_variables_ && !clean_;
}

bool Flags::NeedToPrintAllProcessActions() {
  return need_to_print_all_process_actions_ && !clean_;
}

bool Flags::NeedToPrintReceives() { return need_to_print_receives_ && !clean_; }

bool Flags::NeedToPrintSends() { return need_to_print_sends_ && !clean_; }

bool Flags::NeedToPrintVerbose() { return need_to_print_verbose_ && !clean_; }

bool Flags::NeedToPrintVeryVerbose() {
  return need_to_print_very_verbose_ && !clean_;
}

bool Flags::Active() {
  return need_to_print_global_variables_ || need_to_print_local_variables_ ||
         need_to_print_all_process_actions_ || need_to_print_receives_ ||
         need_to_print_sends_ || need_to_print_verbose_ ||
         need_to_print_very_verbose_;
}
bool Flags::Clean() { clean_ = true; }

bool Flags::Activate() { clean_ = false; }

void Flags::SetNeedToPrintGlobalVariables() {
  need_to_print_global_variables_ = true;
}

void Flags::SetNeedToPrintLocalVariables() {
  need_to_print_local_variables_ = true;
}

void Flags::SetNeedToPrintAllProcessActions() {
  need_to_print_all_process_actions_ = true;
}

void Flags::SetNeedToPrintReceives() { need_to_print_receives_ = true; }

void Flags::SetNeedToPrintSends() { need_to_print_sends_ = true; }

void Flags::SetNeedToPrintVerbose() { need_to_print_verbose_ = true; }

void Flags::SetNeedToPrintVeryVerbose() { need_to_print_very_verbose_ = true; }

} // namespace utils::verbose