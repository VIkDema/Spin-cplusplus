#pragma once

#include "models_fwd.hpp"

namespace models {

struct Sequence {
  Element *frst;
  Element *last;   /* links onto continuations */
  Element *extent; /* last element in original */
  int minel;       /* minimum Seqno, set and used only in guided.c */
  int maxel;       /* 1+largest id in sequence */
};

struct SeqList {
  Sequence *this_sequence; /* one sequence */
  SeqList *nxt;            /* linked list  */
};

} // namespace models