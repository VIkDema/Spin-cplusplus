#pragma once

#include "models_fwd.hpp"

namespace models {

struct Label {
  Symbol *s;
  Symbol *c;
  Element *e;
  int opt_inline_id; /* non-zero if label appears in an inline */
  int visible;       /* label referenced in claim (slice relevant) */
  Label *next;
};

struct Lbreak {
  Symbol *l;
  Lbreak *next;
};

struct L_List {
  Lextok *n;
  L_List *next;
};

} // namespace models