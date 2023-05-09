#pragma once

#include "models_fwd.hpp"

namespace models {

struct Element {
  Lextok *n; /* defines the type & contents */
  int Seqno; /* identifies this el within system */
  int seqno; /* identifies this el within a proc */
  int merge; /* set by -O if step can be merged */
  int merge_start;
  int merge_single;
  short merge_in;      /* nr of incoming edges */
  short merge_mark;    /* state was generated in merge sequence */
  unsigned int status; /* used by analyzer generator  */
  FSM_use *dead;       /* optional dead variable list */
  SeqList *sub;        /* subsequences, for compounds */
  SeqList *esc;        /* zero or more escape sequences */
  Element *Nxt;        /* linked list - for global lookup */
  Element *nxt;        /* linked list - program structure */
};
} // namespace models