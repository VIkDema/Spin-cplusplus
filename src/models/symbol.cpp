#include "symbol.hpp"

#include "../lexer/inline_processor.hpp"
#include "../lexer/line_number.hpp"
#include "access.hpp"

extern models::Symbol *Fname;
extern models::Symbol *context;
extern char *emalloc(size_t);

namespace models {
void Symbol::AddAccess(models::Symbol *what, int count, int type) {
  models::Access *a;

  for (a = access; a; a = a->next)
    if (a->who == context && a->what == what && a->count == count &&
        a->type == type)
      return;

  a = (models::Access *)emalloc(sizeof(models::Access));
  a->who = context;
  a->what = what;
  a->count = count;
  a->type = type;
  a->next = access;
  access = a;
}

bool SideScan(const std::string &t, const std::string &pat) {
  size_t pos = t.find(pat);
  return (pos != std::string::npos && pos > 0 && t[pos - 1] != '"' &&
          t[pos - 1] != '\'');
}

void Symbol::DetectSideEffects() {
  char *t;
  char *z;

  /* could still defeat this check via hidden_flags
   * side effects in function calls,
   * but this will catch at least some cases
   */

  auto tmp = lexer::InlineProcessor::FindInline(name);
  t = (char *)tmp->cn;
  while (t && *t == ' ') {
    t++;
  }

  z = strchr(t, '(');
  if (z && z > t && isalnum((int)*(z - 1)) &&
      strncmp(t, "spin_mutex_free(", strlen("spin_mutex_free(")) != 0) {
    goto bad; /* fct call */
  }

  if (SideScan(t, ";") || SideScan(t, "++") || SideScan(t, "--")) {
  bad:
    file::LineNumber::Set(tmp->dln);
    Fname = tmp->dfn;
    loger::non_fatal("c_expr %s has side-effects", name);
    return;
  }
  while ((t = strchr(t, '=')) != NULL) {
    if (*(t - 1) == '!' || *(t - 1) == '>' || *(t - 1) == '<' ||
        *(t - 1) == '"' || *(t - 1) == '\'') {
      t += 2;
      continue;
    }
    t++;
    if (*t != '=')
      goto bad;
    t++;
  }
}

} // namespace models