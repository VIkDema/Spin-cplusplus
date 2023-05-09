#pragma once

#include "models_fwd.hpp"

namespace models {

struct FSM_state { /* used in pangen5.c - dataflow */
  int from;        /* state number */
  int seen;        /* used for dfs */
  int in;          /* nr of incoming edges */
  int cr;          /* has reachable 1-relevant successor */
  int scratch;
  unsigned long *dom;
  unsigned long *mod; /* to mark dominant nodes */
  struct FSM_trans *t;      /* outgoing edges */
  struct FSM_trans *p;      /* incoming edges, predecessors */
  struct FSM_state *nxt;    /* linked list of all states */
};

struct FSM_trans { /* used in pangen5.c - dataflow */
  int to;
  short relevant;         /* when sliced */
  short round;            /* ditto: iteration when marked */
  struct FSM_use *Val[2]; /* 0=reads, 1=writes */
  struct Element *step;
  struct FSM_trans *nxt;
};

struct FSM_use { /* used in pangen5.c - dataflow */
  models::Lextok *n;
  models::Symbol *var;
  int special;
  struct FSM_use *nxt;
};
} // namespace models