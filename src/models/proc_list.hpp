#pragma once

#include "models_fwd.hpp"

namespace models {

struct ProcList {
  Symbol *n;      /* name       */
  Lextok *p;      /* parameters */
  Sequence *s;            /* body       */
  Lextok *prov;   /* provided clause */
  btypes b;       /* e.g., claim, trace, proc */
  short tn;               /* ordinal number */
  unsigned char det;      /* deterministic */
  unsigned char unsafe;   /* contains global var inits */
  unsigned char priority; /* process priority, if any */
  ProcList *nxt;   /* linked list */
};

} // namespace models