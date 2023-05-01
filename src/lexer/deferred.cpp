#include "deferred.hpp"

#include "../fatal/fatal.hpp"
#include "../spin.hpp"
#include "stdio.h"

static FILE *defer_fd;
extern FILE *yyin;

namespace helpers {

Deferred::Deferred() : diferred_(false) {}
bool Deferred::IsDiferred() { return diferred_; }
void Deferred::SetDiferred(bool new_diferred) { diferred_ = new_diferred; }

bool Deferred::GetDeferred() {
  if (!defer_fd) {
    return false; /* nothing was deferred */
  }
  fclose(defer_fd);

  defer_fd = fopen(TMP_FILE2, "r");
  if (!defer_fd) {
    log::non_fatal("cannot retrieve deferred ltl formula");
    return false;
  }
  fclose(yyin);
  yyin = defer_fd;
  return true;
}
void Deferred::ZapDeferred() { (void)unlink(TMP_FILE2); }

//TODO: refactor it
bool Deferred::PutDeffered() {
  int c, cnt;
  if (!defer_fd) {
    defer_fd = fopen(TMP_FILE2, "w+");
    if (!defer_fd) {
      log::non_fatal("cannot defer ltl expansion");
      return false;
    }
  }
  fprintf(defer_fd, "ltl ");
  cnt = 0;
  while ((c = getc(yyin)) != EOF) {
    if (c == '{') {
      cnt++;
    }
    if (c == '}') {
      cnt--;
      if (cnt == 0) {
        break;
      }
    }
    fprintf(defer_fd, "%c", c);
  }
  fprintf(defer_fd, "}\n");
  fflush(defer_fd);
  return true;
}
} // namespace helpers