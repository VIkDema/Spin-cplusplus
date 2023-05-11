#pragma once

#include "../lexer/lexer.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/_types/_size_t.h>
#include <vector>

struct LaunchSettings {
  bool need_export_ast = false;                         // OLD:export_ast
  bool need_to_analyze = false;                         // OLD:analyze
  bool need_disable_final_state_reporting = false;      // OLD: no_wrapup
  bool need_dont_execute_printfs_in_sumulation = false; // OLD: no_print
  bool need_print_channel_access_info = false;          // OLD: Caccess
  bool need_columnated_output = false;                  // OLD: columns
  bool need_produce_symbol_table_information = false;   // OLD:dumptab
  bool need_compute_synchronous_product_multiple_never_claims =
      false;                                    // OLD:product
  bool need_to_run_in_interactive_mode = false; // OLD: interactive
  bool need_to_print_result_of_inlining_and_preprocessing =
      false;                                          // OLD: inlineonly
  bool reverse_eval_order_of_nested_unlesses = false; // OLD: like_java
  bool need_save_trail = false;                       // OLD: s_trail
  bool need_use_strict_lang_intersection = false;     // OLD: Strict
  bool need_lose_msgs_sent_to_full_queues = false;    // OLD: m_loss
  bool need_short_output = false;                     // OLD: tl_terse
  bool need_old_scope_rules =
      false; // OLD: old_scope_ruless возможно стоит выпилить
  bool need_use_optimizations = false; // OLD: usedopts
  bool need_pretty_print = false;
  bool need_to_tabs = true; // OLD: notabs
  // optimizations
  bool need_use_dataflow_optimizations = true;      // OLD: dataflow
  bool need_hide_write_only_variables = true;       // oLD: deadvar
  bool need_statemate_merging = true;               // OLD:merger
  bool need_rendezvous_optimizations = true;        // OLD: rvopt
  bool need_case_caching = true;                    // OLD: ccache
  bool need_revert_old_rultes_for_priority = false; // OLD: old_priority_rules
  bool need_to_print_version_and_stop = false;
  bool need_to_print_help_and_stop = false;
  bool need_to_recompile = true;     // OLD: norecompile
  bool need_to_replay = false;       // OLD: replay
  bool need_preprocess_only = false; // OLD: preprocessonly
  int buzzed = 0;
  short has_provided = 0;

  int separate_version = 0; // OLD: separate
  int nubmer_trail = 0;     // OLD: ntrail

  std::vector<std::string> trail_file_name;       // old: trailfilename
  std::vector<std::string> never_claim_file_name; // old: nvr_file

  std::vector<std::string> ltl_file; // OLD: ltl_file
  std::vector<std::string> ltl_add;  // OLD: add_ltl

  std::vector<std::string> pre_args =
      {}; // OLD: PreArg[++PreCnt] = (char *)&argv[1][0];

  std::size_t count_of_skipping_steps = 0; // OLD: jumpsteps
  std::size_t count_of_steps = 0;          // OLD: cutoff

  void SetOptimizationsOptions(int value);
  std::string BuildPanRuntime();
};