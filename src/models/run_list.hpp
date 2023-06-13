#pragma once

#include "models_fwd.hpp"

namespace models {

/**
 * @struct RunList
 * Structure representing a run list.
 */
struct RunList {
    Symbol *n; /**< Name of the process. */
    int tn; /**< Ordinal of the type. */
    int pid; /**< Process ID. */
    int priority; /**< Priority for simulations only. */
    btypes b; /**< Type of the process. */
    Element *pc; /**< Current statement. */
    Sequence *ps; /**< Used by the analyzer generator. */
    Lextok *prov; /**< Provided clause. */
    Symbol *symtab; /**< Local variables. */
    RunList *next; /**< Linked list of processes. */
};


} // namespace models