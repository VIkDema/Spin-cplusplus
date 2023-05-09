#pragma once

#include "models_fwd.hpp"

namespace models {

struct RunList {
  Symbol *n;      /* name            */
  int tn;         /* ordinal of type */
  int pid;        /* process id      */
  int priority;   /* for simulations only */
  btypes b;       /* the type of process */
  Element *pc;    /* current stmnt   */
  Sequence *ps;   /* used by analyzer generator */
  Lextok *prov;   /* provided clause */
  Symbol *symtab; /* local variables */
  RunList *nxt;   /* linked list */
};

} // namespace models