#pragma once

#include "models_fwd.hpp"

namespace models {

/**
 * @struct Element
 * Structure representing an element.
 */
struct Element {
    Lextok *n; /**< Defines the type and contents. */
    int Seqno; /**< Identifies this element within the system. */
    int seqno; /**< Identifies this element within a procedure. */
    int merge; /**< Set by -O if the step can be merged. */
    int merge_start; /**< Merge start. */
    int merge_single; /**< Single merge. */
    short merge_in; /**< Number of incoming edges. */
    short merge_mark; /**< State was generated in the merge sequence. */
    unsigned int status; /**< Used by the analyzer generator. */
    FSM_use *dead; /**< Optional dead variable list. */
    SeqList *sub; /**< Subsequences, used for compounds. */
    SeqList *esc; /**< Zero or more escape sequences. */
    Element *Nxt; /**< Linked list for global lookup. */
    Element *next; /**< Linked list for program structure. */
};

} // namespace models