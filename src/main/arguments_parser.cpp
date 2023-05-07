#include "arguments_parser.hpp"

#include "../utils/seed/seed.hpp"
#include "../utils/verbose/verbose.hpp"
#include "pre_proc_settings.hpp"
#include <cstdlib>
#include <stdexcept>

// TODO: change it
extern void qhide(int);

LaunchSettings ArgumentsParser::Parse(int argc, char **argv) {
  LaunchSettings result;
  auto &verbose_flags = utils::verbose::Flags::getInstance();
  auto &seed = utils::seed::Seed::getInstance();
  /* unused flags: y, z, G, L, Q, R, W */
  while (argc > 1 && argv[1][0] == '-') {
    switch (argv[1][1]) {
    case 'A': {
      result.need_export_ast = true;
      break;
    }
    case 'a': {
      result.need_to_analyze = true;
      break;
    }
    case 'B': {
      result.need_disable_final_state_reporting = true;
      break;
    }
    case 'b': {
      result.need_dont_execute_printfs_in_sumulation = true;
      break;
    }
    case 'C': {
      result.need_print_channel_access_info = true;
      break;
    }
    case 'c': {
      result.need_columnated_output = true;
      break;
    }
    case 'D': {
      result.pre_args.push_back(std::string((char *)&argv[1][0]));
      break;
    }
    case 'd': {
      result.need_produce_symbol_table_information = true;
      break;
    }
    case 'E': {
      result.pre_args.push_back(std::string((char *)&argv[1][2]));
      break;
    }
    case 'e': {
      result.need_compute_synchronous_product_multiple_never_claims = true;
      break;
    }
    case 'F': {
      for (int i = 2; i < argc; i++) {
        result.ltl_file.push_back(argv[i]);
      }
      argc--;
      argv++;
      break;
    }
    case 'f': {
      result.ltl_add = std::vector<std::string>(argv, argv + argc);
      argc--;
      argv++;
      break;
    }
    case 'g': {
      verbose_flags.SetNeedToPrintGlobalVariables();
      break;
    }
    case 'h': {
      seed.SetNeedToPrintSeed(true);
    }
    case 'i': {
      result.need_to_run_in_interactive_mode = true;
      break;
    }
    case 'I': {
      result.need_to_print_result_of_inlining_and_preprocessing = true;
      break;
    }
    case 'J': {
      result.reverse_eval_order_of_nested_unlesses = true;
      break;
    }
    case 'j': {
      result.count_of_skipping_steps = std::atoi(&argv[1][2]);
      break;
    }
    case 'k': {
      result.need_save_trail = false;
      for (int i = 2; i < argc; i++) {
        result.trail_file_name.push_back(argv[i]);
      }
      argc--;
      argv++;
    }
    case 'L': {
      result.need_use_strict_lang_intersection = true;
      break;
    }
    case 'l': {
      verbose_flags.SetNeedToPrintLocalVariables();
      break;
    }
    case 'M': {
      result.need_generate_mas_flow_tcl_tk = true;
      break;
    }
    case 'm': {
      result.need_lose_msgs_sent_to_full_queues = true;
      break;
    }
    case 'N': {
      for (int i = 2; i < argc; i++) {
        result.never_claim_file_name.push_back(argv[i]);
      }
      argc--;
      argv++;
      break;
    }
    case 'n': {
      seed.SetSeed(std::atoi(&argv[1][2]));
      result.need_short_output = true;
      break;
    }
    case 'O': {
      result.need_old_scope_rules = true;
      break;
    }
    case 'o': {
      try {
        result.SetOptimizationsOptions(argv[1][2]);
      } catch (const std::runtime_error &error) {
        std::cout << error.what();
        // print help
      }
      break;
    }
    case 'P': {
      pre_proc_processor.SetCommand(std::string((const char *)&argv[1][2]));
      break;
    }
    case 'p': {
      if (argv[1][2] == 'p') {
        result.need_pretty_print = true;
      }
      verbose_flags.SetNeedToPrintAllProcessActions();
      break;
    }
    case 'q': {
      if (isdigit((int)argv[1][2])) {
        qhide(atoi(&argv[1][2]));
      }
      break;
    }
    case 'r': {
      // TODO: разобраться с add_runtime
      if (strcmp(&argv[1][1], "run") == 0) {
      samecase:
        if (result.buzzed != 0) {
          loger::fatal("cannot combine -x with -run -replay or -search");
        }
        result.buzzed = 2;
        result.need_to_analyze = true;
        argc--;
        argv++;
        while (argc > 1 && argv[1][0] == '-') {
          switch (argv[1][1]) {
          case 'D':
          case 'O':
          case 'U':
            // add_comptime(argv[1]);
            break;
          case 'W':
            result.need_to_recompile = false;
            break;
          case 'l':
            if (strcmp(&argv[1][1], "ltl") == 0) {
              // add_runtime("-N");
              argc--;
              argv++;
              // add_runtime(argv[1]);
              break;
            }
            if (strcmp(&argv[1][1], "link") == 0) {
              argc--;
              argv++;
              // add_comptime(argv[1]);
              break;
            }
          default:
            // add_runtime(argv[1]);
            break;
          }
          argc--;
          argv++;
        }
        argc++;
        argv--;
      } else if (strcmp(&argv[1][1], "replay") == 0) {
        result.need_to_replay = true;
        // add_runtime("-r");
        goto samecase;
      } else {
        verbose_flags.SetNeedToPrintReceives();
      }
      break;
    }
    case 'S': {
      result.separate_version = std::atoi(&argv[1][2]);
      result.need_to_analyze = true;
      break;
    }
    case 's': {
      std::string temp(&argv[1][1]);
      if (temp == "simulate") {
        break; /* ignore */
      }
      if (temp == "search") {
        goto samecase;
      }
      verbose_flags.SetNeedToPrintSends();
      break;
    }
    case 'T': {
      result.need_to_tabs = false;
      break;
    }
    case 't': {
      result.need_save_trail = true;
      if (isdigit((int)argv[1][2])) {
        result.nubmer_trail = std::atoi(&argv[1][2]);
      }
      break;
    }
    case 'U': {
      result.pre_args.push_back(std::string((char *)&argv[1][0]));
      break;
    }
    case 'u': {
      result.count_of_steps = std::atoi(&argv[1][2]);
      break;
    }
    case 'v': {
      verbose_flags.SetNeedToPrintVerbose();
      break;
    }
    case 'V': {
      result.need_to_print_version_and_stop = true;
      break;
    }
    case 'w': {
      verbose_flags.SetNeedToPrintVeryVerbose();
      break;
    }
    case 'W': {
      result.need_to_recompile = false;
      break;
    }
    case 'x': {
      if (result.buzzed != 0) {
        loger::fatal("cannot combine -x with -run -search or -replay");
      }
      result.buzzed = 1;
      // pan_runtime = "-d";
      result.need_to_analyze = 1;
      result.SetOptimizationsOptions(3);
      break;
    }
    default: {
      result.need_to_print_help_and_stop = true;
      break;
    }
    }
    argc--;
    argv++;
  }
  return result;
}