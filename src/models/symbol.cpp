#include "symbol.hpp"

#include "access.hpp"
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

} // namespace models