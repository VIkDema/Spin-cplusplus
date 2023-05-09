#pragma once

#include "models_fwd.hpp"

namespace models {

struct Queue {
  short qid;         /* runtime q index */
  int qlen;          /* nr messages stored */
  int nslots, nflds; /* capacity, flds/slot */
  int setat;         /* last depth value changed */
  int *fld_width;    /* type of each field */
  int *contents;     /* the values stored */
  int *stepnr;       /* depth when each msg was sent */
  char **mtp;        /* if mtype, name of list, else 0 */
  struct Queue *next; /* linked list */
};

} // namespace models