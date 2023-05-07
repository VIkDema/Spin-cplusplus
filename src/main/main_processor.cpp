#include "main_processor.hpp"

#include "../lexer/scope.hpp"
#include "../spin.hpp"
#include "../utils/format/preprocessed_file_viewer.hpp"
#include "../utils/format/pretty_print_viewer.hpp"
#include "../utils/seed/seed.hpp"
#include "../utils/verbose/verbose.hpp"
#include "arguments_parser.hpp"
#include "help.hpp"
#include "launch_settings.hpp"
#include "pan_processor.hpp"
#include "pre_proc_settings.hpp"
#include "stdio.h"
#include <cassert>
#include <cstdlib>
#include <fmt/core.h>
#include <string>
#include <sys/stat.h>

// TODO: change it
extern FILE *yyin, *yyout, *tl_out;
extern int depth; /* at least some steps were made */
models::Symbol *Fname, *oFname;
static char *ltl_claims = nullptr;
extern PanProcessor pan_processor_;
extern void putprelude(void);
extern void ana_src(int, int);
int nr_errs;
static FILE *fd_ltl = (FILE *)0;

extern LaunchSettings launch_settings;
extern lexer::Lexer lexer_;

std::string MainProcessor::out_;

int MainProcessor::main(int argc, char *argv[]) {
  InitSeed();
  InitStreams();
  InitScope();
  InitPreProcSettings();
  ArgumentsParser parser;
  launch_settings = parser.Parse(argc, argv);
  if (HandleLaunchSettings(argc, argv)) {
    return 0;
  }
  return 0;
}

void MainProcessor::InitSeed() {
  auto &seed = utils::seed::Seed::getInstance();
  seed.GenerateSeed();
}

void MainProcessor::InitStreams() {
  yyin = stdin;
  yyout = stdout;
  tl_out = stdout;
}
void MainProcessor::InitScope() { lexer::scope_processor_.InitScopeName("_"); }

void MainProcessor::InitPreProcSettings() { pre_proc_processor.Init(); }

bool MainProcessor::HandleLaunchSettings(int argc, char *argv[]) {
  auto &verbose_flags = utils::verbose::Flags::getInstance();
  if (launch_settings.need_to_print_help_and_stop) {
    PrintHelp();
    Exit(0);
  }

  if (launch_settings.need_to_print_version_and_stop) {
    PrintVersion();
    Exit(0);
  }

  if (launch_settings.need_pretty_print) {
    format::PrettyPrintViewer pp;
    pp.view();
    Exit(0);
  }

  if (launch_settings.need_generate_mas_flow_tcl_tk &&
      launch_settings.count_of_steps == 0) {
    launch_settings.count_of_steps = 1024;
  }

  if (launch_settings.need_use_optimizations &&
      !launch_settings.need_to_analyze) {
    std::cout << "spin: warning -o[1..5] option ignored in simulations"
              << std::endl;
  }

  if (!launch_settings.ltl_file.empty()) {
    // open_ltl
  }

  if (argc > 1) {
    FILE *fd = stdout;
    std::string cmd, out2;

    /* must remain in current dir */
    out_ = "pan.pre";
    if (!launch_settings.ltl_add.empty() ||
        !launch_settings.never_claim_file_name.empty()) {
      assert(strlen(argv[1]) + 6 < sizeof(out2));
      out2 = fmt::format("{}.nvr", argv[1]);
      if ((fd = fopen(out2.c_str(), MFLAGS)) == NULL) {
        printf("spin: cannot create tmp file %s\n", out2.c_str());
        alldone(1);
      }
      fprintf(fd, "#include \"%s\"\n", argv[1]);
    }

    if (!launch_settings.ltl_add.empty()) {
      tl_out = fd;
      // nr_errs = tl_main(2, add_ltl);
      fclose(fd);
      pre_proc_processor.Preprocess(out2, out_, 1);
    } else if (!launch_settings.never_claim_file_name.empty()) {
      fprintf(fd, "#include \"%s\"\n",
              launch_settings.never_claim_file_name.front().c_str());
      fclose(fd);
      pre_proc_processor.Preprocess(out2, out_, 1);
    } else {
      pre_proc_processor.Preprocess(argv[1], out_, 0);
    }

    if (launch_settings.need_preprocess_only) {
      Exit(0);
    }

    if (!(yyin = fopen(out_.c_str(), "r"))) {
      printf("spin: cannot open %s\n", out_.c_str());
      Exit(1);
    }

    if (strncmp(argv[1], "progress", (size_t)8) == 0 ||
        strncmp(argv[1], "accept", (size_t)6) == 0) {
      cmd = fmt::format("_{}", argv[1]);
    } else {
      cmd = fmt::format("{}", argv[1]);
    }
    oFname = Fname = lookup(cmd);
    if (oFname->name[0] == '\"') {
      oFname->name[oFname->name.length() - 1] = '\0';
      oFname =
          lookup(std::string(oFname->name.begin() + 1, oFname->name.end()));
    }
  } else {
    oFname = Fname = lookup("<stdin>");
    if (!launch_settings.ltl_add.empty()) {
      if (argc > 0)
        //   exit(tl_main(2, add_ltl));
        printf("spin: missing argument to -f\n");
      Exit(1);
    }
    // printf("%s\n", SpinVersion);
    fprintf(stderr, "spin: error, no filename specified\n");
    fflush(stdout);
    Exit(1);
  }

  if (launch_settings.need_generate_mas_flow_tcl_tk) {
    if (verbose_flags.Active()) {
      std::cout << "spin: -c precludes all flags except -t" << std::endl;
      Exit(1);
    }
    putprelude();
  }
  if (launch_settings.need_columnated_output &&
      !verbose_flags.NeedToPrintReceives() &&
      !verbose_flags.NeedToPrintSends()) {
    verbose_flags.SetNeedToPrintSends();
    verbose_flags.SetNeedToPrintReceives();
  }

  InitSymbols();

  yyparse();
  fclose(yyin);

  if (ltl_claims) {
    models::Symbol *r;
    fclose(fd_ltl);
    if (!(yyin = fopen(ltl_claims, "r"))) {
      loger::fatal("cannot open %s", ltl_claims);
    }
    r = oFname;
    oFname = Fname = lookup(ltl_claims);
    yyparse();
    fclose(yyin);
    oFname = Fname = r;
    if (0) {
      (void)unlink(ltl_claims);
    }
  }

  loose_ends();

  if (launch_settings.need_to_print_result_of_inlining_and_preprocessing) {
    format::PreprocessedFileViewer viewer;
    viewer.view();
    return true;
  }
  chanaccess();

  if (!launch_settings.need_print_channel_access_info) {
    if (launch_settings.has_provided &&
        launch_settings.need_statemate_merging) {
      launch_settings.need_statemate_merging =
          false; /* cannot use statement merging in this case */
    }
    if (!launch_settings.need_save_trail &&
        (launch_settings.need_use_dataflow_optimizations ||
         launch_settings.need_statemate_merging) &&
        (!launch_settings.need_to_replay || lexer_.GetHasCode())) {
      ana_src(launch_settings.need_use_dataflow_optimizations,
              launch_settings.need_statemate_merging);
    }
    // Запуск симуляции
    sched();
    Exit(nr_errs);
  }

  return false;
}

void MainProcessor::InitSymbols() {
  models::Symbol *s;
  s = lookup("_");
  s->type = models::SymbolType::kPredef; /* write-only global var */
  s = lookup("_p");
  s->type = models::SymbolType::kPredef;
  s = lookup("_pid");
  s->type = models::SymbolType::kPredef;
  s = lookup("_last");
  s->type = models::SymbolType::kPredef;
  s = lookup("_nr_pr");
  s->type = models::SymbolType::kPredef; /* new 3.3.10 */
  s = lookup("_priority");
  s->type = models::SymbolType::kPredef; /* new 6.2.0 */
}

void MainProcessor::Exit(int estatus) {
#if defined(WIN32) || defined(WIN64)
  struct _stat x;
#else
  struct stat x;
#endif

  if (!launch_settings.need_preprocess_only && !out_.empty()) {
    unlink(out_.c_str());
  }
  unlink(TMP_FILE1);
  unlink(TMP_FILE2);

  auto &seed = utils::seed::Seed::getInstance();

  if (!launch_settings.buzzed && seed.NeedToPrintSeed() &&
      !launch_settings.need_to_analyze && !launch_settings.need_export_ast &&
      !launch_settings.need_save_trail &&
      !launch_settings.need_preprocess_only && depth > 0) {
    std::cout << fmt::format("seed used: {}", seed.GetSeed()) << std::endl;
    printf("seed used: %d\n", seed.GetSeed());
  }

  if (launch_settings.buzzed && launch_settings.need_to_replay &&
      !lexer_.GetHasCode() && !estatus) {
    std::string pan_runtime = launch_settings.BuildPanRuntime();
    std::string tmp;
    tmp = fmt::format("spin -t {} {}", pan_runtime, Fname->name);
    estatus = e_system(1, tmp);
    exit(estatus);
  }

  if (launch_settings.buzzed &&
      (!launch_settings.need_to_replay || lexer_.GetHasCode()) && !estatus) {
    std::string tmp;
    char *P_X = nullptr;
    std::string tmp2 = P_X;

    char *C_X = const_cast<char *>((launch_settings.buzzed == 2) ? "-O" : "");

    if (launch_settings.need_to_replay &&
        pan_processor_.GetPanRuntime().empty()) {
#if defined(WIN32) || defined(WIN64)
      P_X = "pan";
#else
      P_X = "./pan";
#endif
      if (stat(P_X, (struct stat *)&x) < 0) {
        goto recompile; /* no executable pan for replay */
      }
      int size = pan_processor_.GetPanRuntime().size();
      tmp = fmt::format("{} {}", P_X, size);
      goto runit;
    }
#if defined(WIN32) || defined(WIN64)
    P_X = "-o pan pan.c && pan";
#else
    P_X = "-o pan pan.c && ./pan";
#endif
    /* swarm and biterate randomization additions */
    if (!launch_settings.need_to_replay &&
        pan_processor_.GetItsr()) /* iterative search refinement */
    {
      if (pan_processor_.GetPanComptime().find("-DBITSTATE") ==
          std::string::npos) {
        pan_processor_.AddComptime("-DBITSTATE");
      }
      if (pan_processor_.GetPanComptime().find("-DPUTPID") ==
          std::string::npos) {
        pan_processor_.AddComptime("-DPUTPID");
      }

      if (pan_processor_.GetPanComptime().find("-DT_RAND") ==
              std::string::npos &&
          pan_processor_.GetPanComptime().find("-DT_REVERSE") ==
              std::string::npos) {
        pan_processor_.AddRuntime("-T0  "); /* controls t_reverse */
      }

      if (pan_processor_.GetPanRuntime().find("-P") ==
              std::string::npos /* runtime flag */
          || pan_processor_.GetPanRuntime()[2] < '0' ||
          pan_processor_.GetPanRuntime()[2] > '1') /* no -P0 or -P1 */
      {
        pan_processor_.AddRuntime("-P0  "); /* controls p_reverse */
      }
      if (pan_processor_.GetPanRuntime().find("-w") == std::string::npos) {
        pan_processor_.AddRuntime("-w18 ");
      } else {
        int x = OmitStr(pan_processor_.GetPanRuntime(), "-w");
        if (x >= 0) {
          std::string nv = fmt::format("-w{}  ", x);
          pan_processor_.AddRuntime(nv);
        }
      }
      if (pan_processor_.GetPanRuntime().find("-h") == std::string::npos) {
        pan_processor_.AddRuntime("-h0  ");
        /* leave 2 spaces for increments up to -h499 */
      } else if (pan_processor_.GetPanRuntime().find("-hash") ==
                 std::string::npos) {
        int x = OmitStr(pan_processor_.GetPanRuntime(), "-h");
        if (x >= 0) {
          std::string nv = fmt::format("-h{}  ", x % 500);
          pan_processor_.AddRuntime(nv);
        }
      }
      if (pan_processor_.GetPanRuntime().find("-k") == std::string::npos) {
        pan_processor_.AddRuntime("-k1  "); /* 1..3 */
      } else {
        int x = OmitStr(pan_processor_.GetPanRuntime(), "-k");
        if (x >= 0) {
          std::string nv = fmt::format("-k{}  ", x % 4);
          pan_processor_.AddRuntime(nv);
        }
      }

      if (pan_processor_.GetPanRuntime().find("-p_rotate") !=
          std::string::npos) {
        int x = OmitStr(pan_processor_.GetPanRuntime(), "-p_rotate");
        if (x < 0) {
          x = 0;
        }
        std::string nv = fmt::format("-p_rotate{}  ", x % 256);
        pan_processor_.AddRuntime(nv); /* added spaces */
      } else if (pan_processor_.GetPanRuntime().find("-p_rotate") ==
                 std::string::npos) {
        pan_processor_.AddRuntime("-p_rotate0  ");
      }
      if (pan_processor_.GetPanRuntime().find("-RS") != std::string::npos) {
        OmitStr(pan_processor_.GetPanRuntime(), "-RS");
      }
      /* need room for at least 10 digits */
      pan_processor_.AddRuntime("-RS1234567890 ");
      ChangeRandomSeed(pan_processor_.GetPanRuntime());
    }
  recompile:
    if (pre_proc_processor.GetCommand().find("cpp") ==
        std::string::npos) /* unix/linux */
    {
      pre_proc_processor.SetCommand("gcc");
    } else if (pre_proc_processor.GetCommand().find("-E") !=
               std::string::npos) {
      auto temp = pre_proc_processor.GetCommand();
      temp.resize(pre_proc_processor.GetCommand().find("-E"));
      pre_proc_processor.SetCommand(temp);
    }

    // final_fiddle();

    tmp2 = tmp;
    tmp = fmt::format("{} {} {} {} {}", pre_proc_processor.GetCommand(), C_X,
                      pan_processor_.GetPanComptime(), P_X,
                      pan_processor_.GetPanRuntime());

    /* P_X ends with " && ./pan " */

    if (!launch_settings.need_to_replay) {
      if (pan_processor_.GetItsr() < 0) /* swarm only */
      {
        tmp += " &";
        pan_processor_.ReverseItsr();
      }
      /* do compilation first
       * split cc command from run command
       * leave cc in tmp, and set tmp2 to run
       */
      if (auto pos = tmp.find(" && "); pos != std::string::npos) {
        tmp2 = std::string(tmp.begin() + pos + 4, tmp.end());
      }
    }

    if (lexer_.HasLtl()) {
      (void)unlink("_spin_nvr.tmp");
    }

    if (!launch_settings.need_to_recompile) { /* make sure that if compilation
                           fails we do not continue */
#ifdef PC
      (void)unlink("./pan.exe");
#else
      (void)unlink("./pan");
#endif
    }
  runit:
    if (launch_settings.need_to_recompile && tmp != tmp2) {
      estatus = 0;
    } else {
      estatus = e_system(1, tmp); /* compile or run */
    }
    if (launch_settings.need_to_replay || estatus < 0) {
      goto skipahead;
    }
    /* !replay */
    if (pan_processor_.GetItsr() == 0 &&
        !pan_processor_.GetSwOrBt()) /* single run */
    {
      estatus = e_system(1, tmp2);
    } else if (pan_processor_.GetItsr() > 0) /* iterative search refinement */
    {
      int is_swarm = 0;
      if (tmp2 != tmp) /* swarm: did only compilation so far */
      {
        tmp = tmp2;                 /* now we point to the run command */
        estatus = e_system(1, tmp); /* first run */
        pan_processor_.DesItsr();
      }
      pan_processor_.DesItsr();
      /* count down */

      /* the following are added back randomly later */
      OmitStr(tmp, "-p_reverse"); /* replaced by spaces */
      OmitStr(tmp, "-p_normal");

      if (tmp.find(" &") != std::string::npos) {
        OmitStr(tmp, " &");
        is_swarm = 1;
      }

      /* increase -w every itsr_n-th run */
      if ((pan_processor_.GetItsrN() > 0 &&
           (pan_processor_.GetItsr() == 0 ||
            (pan_processor_.GetItsr() % pan_processor_.GetItsrN()) != 0)) ||
          (ChangeParam(tmp, "-w", 36, 18) >= 0)) /* max 4G bit statespace*/
      {
        ChangeParam(tmp, "-h", 500, 0);        /* hash function 0.499*/
        ChangeParam(tmp, "-p_rotate", 256, 0); /* if defined */
        ChangeParam(tmp, "-k", 4, 1);          /* nr bits per state 0->1,2,3 */
        ChangeParam(tmp, "-T", 2, 0);          /* with or without t_reverse*/
        ChangeParam(tmp, "-P", 2, 0);          /* -P 0..1 != p_reverse */

        ChangeRandomSeed(tmp); /* change random seed */
        StringTrim(tmp);
        if (rand() % 5 == 0) /* 20% of all runs */
        {
          tmp += " -p_reverese";
          /* at end, so this overrides -p_rotateN, if there */
          /* but -P0..1 disable this in 50% of the cases */
          /* so its really activated in 10% of all runs */
        } else if (rand() % 6 == 0) /* override p_rotate and p_reverse */
        {
          tmp += " -p_normal";
        }
        if (is_swarm) {
          tmp += " &";
        }
        goto runit;
      }
    }
  skipahead:
    (void)unlink("pan.b");
    (void)unlink("pan.c");
    (void)unlink("pan.h");
    (void)unlink("pan.m");
    (void)unlink("pan.p");
    (void)unlink("pan.t");
  }
  exit(estatus);
}

int MainProcessor::e_system(int code, const std::string &message) {
  static int count = 1;
  auto &verbose_flags = utils::verbose::Flags::getInstance();
  /* v == 0 : checks to find non-linked version of gcc */
  /* v == 1 : all other commands */
  /* v == 2 : preprocessing the promela input */
  if (code != 1) {
    return system(message.c_str());
  }
  if (verbose_flags.NeedToPrintVerbose() ||
      verbose_flags.NeedToPrintVeryVerbose()) /* -v or -w */
  {
    printf("cmd%02d: %s\n", count++, message.c_str());
    fflush(stdout);
  }
  if (verbose_flags.NeedToPrintVeryVerbose()) /* only -w */
  {
    return 0; /* suppress the call to system(s) */
  }
  return system(message.c_str());
}

int MainProcessor::ChangeParam(std::string &t, const std::string &what,
                               int range, int bottom) {
  std::size_t pos = t.find(what);
  if (pos != std::string::npos) {
    pos += what.length();
    if (!isdigit(static_cast<unsigned char>(t[pos]))) {
      return 0;
    }
    int v = std::stoi(t.substr(pos)) + 1; /* было: v = (atoi(ptr)+1)%range */
    if (v >= range) {
      v = bottom;
    }
    if (v >= 100) {
      t[pos++] = '0' + (v / 100);
      v %= 100;
      t[pos++] = '0' + (v / 10);
      t[pos] = '0' + (v % 10);
    } else if (v >= 10) {
      t[pos++] = '0' + (v / 10);
      t[pos++] = '0' + (v % 10);
      t[pos] = ' ';
    } else {
      t[pos++] = '0' + v;
      t[pos++] = ' ';
      t[pos] = ' ';
    }
  }
  return 1;
}

void MainProcessor::ChangeRandomSeed(std::string &t) {
  std::size_t pos = t.find("-RS");
  if (pos != std::string::npos) {
    pos += 3;
    long v = rand() % 1000000000L;
    std::size_t cnt = 0;
    while (v / 10 > 0) {
      t[pos++] = '0' + v % 10;
      v /= 10;
      cnt++;
    }
    t[pos++] = '0' + v;
    cnt++;
    while (cnt++ < 10) {
      t[pos++] = ' ';
    }
  }
}

int MainProcessor::OmitStr(std::string &in, const std::string &s) {
  std::size_t pos = in.find(s);
  int nr = -1;
  if (pos != std::string::npos) {
    for (std::size_t i = 0; i < s.length(); i++) {
      in[pos++] = ' ';
    }
    if (isdigit(in[pos])) {
      nr = std::stoi(in.substr(pos));
      while (isdigit(in[pos])) {
        in[pos++] = ' ';
      }
    }
  }
  return nr;
}

void MainProcessor::StringTrim(std::string &t) {
  std::size_t n = t.length() - 1;
  while (n > 0 && t[n] == ' ') {
    t.pop_back();
    n--;
  }
}
