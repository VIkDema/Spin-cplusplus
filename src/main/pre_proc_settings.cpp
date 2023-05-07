#include "pre_proc_settings.hpp"
#include "main_processor.hpp"
#include <fmt/core.h>

#ifndef CPP
/* to use visual C++:
        #define CPP	"cl -E/E"
   or call spin as:	spin -P"CL -E/E"

   on OS2:
        #define CPP	"icc -E/Pd+ -E/Q+"
   or call spin as:	spin -P"icc -E/Pd+ -E/Q+"
   make sure the -E arg is always at the end,
   in each case, because the command line
   can later be truncated at that point
*/
#if 1
#define CPP                                                                    \
  "gcc -std=gnu99 -Wno-unknown-warning-option -Wformat-overflow=0 -E -x c"
/* if gcc-4 is available, this setting is modified below */
#else
#if defined(PC) || defined(MAC)
#define CPP "gcc -std=gnu99 -E -x c"
#else
#ifdef SOLARIS
#define CPP "/usr/ccs/lib/cpp"
#else
#define CPP "cpp" /* sometimes: "/lib/cpp" */
#endif
#endif
#endif
#endif

void PreProcSettings::Init() { command_ = CPP; }
std::string PreProcSettings::GetCommand() { return command_; }
void PreProcSettings::SetCommand(const std::string &command) {
  was_changed_ = true;
  command_ = command;
}
bool PreProcSettings::IsDefault() { return !was_changed_; }

void PreProcSettings::Preprocess(const std::string &a, const std::string &b,
                                 int a_tmp, LaunchSettings &launch_settings) {
  std::string precmd, cmd;

  precmd = command_;
  for (auto &pre_arg : launch_settings.pre_args) {
    precmd += " " + pre_arg;
  }
  cmd = fmt::format("{} \"{}\" > \"{}\"", precmd, a, b);

  if (MainProcessor::e_system(2, cmd)) /* preprocessing step */
  {
    unlink(b.c_str());
    if (a_tmp)
      unlink(a.c_str());
    fprintf(stdout, "spin: preprocessing failed %s\n", cmd.c_str());
    MainProcessor::Exit(1, launch_settings); /* no return, error exit */
  }
  if (a_tmp)
    unlink(a.c_str());
}
