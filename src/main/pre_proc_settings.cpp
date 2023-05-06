#include "pre_proc_settings.hpp"

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
bool PreProcSettings::IsDefault() {
  return !was_changed_;
}
