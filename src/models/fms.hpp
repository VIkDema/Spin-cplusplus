#pragma once

#include "models_fwd.hpp"

namespace models {
/**
 * @struct FSM_state
 * Structure used in pangen5.c for dataflow.
 */
struct FSM_state {
    int from; /**< State number. */
    int seen; /**< Used for depth-first search. */
    int in; /**< Number of incoming edges. */
    int cr; /**< Indicates whether there is a reachable 1-relevant successor. */
    int scratch;
    unsigned long *dom; /**< Dominant nodes. */
    unsigned long *mod; /**< Marks dominant nodes. */
    struct FSM_trans *t; /**< Outgoing edges. */
    struct FSM_trans *p; /**< Incoming edges (predecessors). */
    struct FSM_state *next; /**< Linked list of all states. */
};

/**
 * @struct FSM_trans
 * Structure used in pangen5.c for dataflow.
 */
struct FSM_trans {
    int to;
    short relevant; /**< Relevant when sliced. */
    short round; /**< Iteration when marked. */
    struct FSM_use *Val[2]; /**< Value array: 0=reads, 1=writes. */
    struct Element *step;
    struct FSM_trans *next;
};

/**
 * @struct FSM_use
 * Structure used in pangen5.c for dataflow.
 */
struct FSM_use {
    models::Lextok *n;
    models::Symbol *var;
    int special;
    struct FSM_use *next;
};

} // namespace models