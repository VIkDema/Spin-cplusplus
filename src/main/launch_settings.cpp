#include "launch_settings.hpp"

#include "../utils/seed/seed.hpp"
#include "../utils/verbose/verbose.hpp"
#include "pre_proc_settings.hpp"
#include <fmt/core.h>

void LaunchSettings::SetOptimizationsOptions(int value) {
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  switch (value) {
  case '1':
    need_use_dataflow_optimizations = false;
    if (verbose_flags.NeedToPrintVerbose()) {
      std::cout << "spin: dataflow optimizations turned off" << std::endl;
    }
    break;
  case '2':
    need_hide_write_only_variables = false;
    if (verbose_flags.NeedToPrintVerbose()) {
      std::cout << "spin: dead variable elimination turned off" << std::endl;
    }
    break;
  case '3':
    need_statemate_merging = false;
    if (verbose_flags.NeedToPrintVerbose()) {
      std::cout << "spin: statement merging turned off" << std::endl;
    }
    break;

  case '4':
    need_rendezvous_optimizations = false;
    if (verbose_flags.NeedToPrintVerbose()) {
      std::cout << "spin: rendezvous optimization turned off" << std::endl;
    }
    break;
  case '5':
    need_case_caching = false;
    if (verbose_flags.NeedToPrintVerbose()) {
      std::cout << "spin: case caching turned off" << std::endl;
    }
    break;
  case '6':
    need_revert_old_rultes_for_priority = true;
    if (verbose_flags.NeedToPrintVerbose()) {
      std::cout << "spin: using old priority rules (pre version 6.2)"
                << std::endl;
    }
    return; /* no break */
  case '7':
    lexer_.SetImpliedSemis(0);
    if (verbose_flags.NeedToPrintVerbose()) {
      std::cout << "spin: no implied semi-colons (pre version 6.3)"
                << std::endl;
    }
    return; /* no break */
  default:
    throw std::runtime_error("spin: bad or missing parameter on -o");
    break;
  }
  need_use_optimizations = true;
}

std::string LaunchSettings::BuildPanRuntime() {
  extern QH *qh_lst;
  QH *j;
  auto &verbose_flags = utils::verbose::Flags::getInstance();
  auto &seed = utils::seed::Seed::getInstance();

  std::string pan_runtime_ = fmt::format("-n{} ", seed.GetSeed());

  if (count_of_skipping_steps.has_value()) {
    pan_runtime_ += fmt::format("-j{} ", count_of_skipping_steps.value());
  }
  if (!trail_file_name.empty()) {
    pan_runtime_ += fmt::format("-k{} ", trail_file_name.front());
  }
  if (count_of_steps.has_value()) {
    pan_runtime_ += fmt::format("-u{} ", count_of_steps.value());
  }
  for (const auto &pre_arg : pre_args) {
    pan_runtime_ += pre_arg + " ";
  }

  for (j = qh_lst; j; j = j->nxt) {
    pan_runtime_ += fmt::format("-q{} ", j->n);
  }
  if (!pre_proc_processor.IsDefault()) {
    pan_runtime_ += fmt::format("-P{} ", pre_proc_processor.GetCommand());
  }

  /* -oN options 1..5 are ignored in simulations */
  if (need_revert_old_rultes_for_priority) {
    pan_runtime_ += "-o6 ";
  }

  if (!lexer_.GetImpliedSemis()) {
    pan_runtime_ += "-o7 ";
  }

  if (need_dont_execute_printfs_in_sumulation) {
    pan_runtime_ += "-b ";
  }
  if (need_disable_final_state_reporting) {
    pan_runtime_ += "-B ";
  }
  if (need_columnated_output) {
    pan_runtime_ += "-c ";
  }
  if (need_generate_mas_flow_tcl_tk) {
    pan_runtime_ += "-M ";
  }
  if (seed.NeedToPrintSeed()) {
    pan_runtime_ += "-h ";
  }
  if (reverse_eval_order_of_nested_unlesses) {
    pan_runtime_ += "-J ";
  }
  if (need_old_scope_rules) {
    pan_runtime_ += "-O ";
  }
  if (need_to_tabs) {
    pan_runtime_ += "-T ";
  }
  if (verbose_flags.NeedToPrintGlobalVariables()) {
    pan_runtime_ += "-G ";
  }
  if (verbose_flags.NeedToPrintLocalVariables()) {
    pan_runtime_ += "-l ";
  }
  if (verbose_flags.NeedToPrintAllProcessActions()) {
    pan_runtime_ += "-p ";
  }

  if (verbose_flags.NeedToPrintReceives()) {
    pan_runtime_ += "-r ";
  }
  if (verbose_flags.NeedToPrintSends()) {
    pan_runtime_ += "-s ";
  }
  if (verbose_flags.NeedToPrintVerbose()) {
    pan_runtime_ += "-v ";
  }
  if (verbose_flags.NeedToPrintVeryVerbose()) {
    pan_runtime_ += "-w ";
  }
  if (need_lose_msgs_sent_to_full_queues) {
    pan_runtime_ += "-m ";
  }
  return pan_runtime_;
}