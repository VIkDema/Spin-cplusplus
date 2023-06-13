#pragma once

#include "models_fwd.hpp"

namespace models {
/**
 * @struct ProcList
 * Structure representing a process list.
 */
struct ProcList {
    Symbol *n; /**< Name of the process. */
    Lextok *p; /**< Parameters of the process. */
    Sequence *s; /**< Body of the process. */
    Lextok *prov; /**< Provided clause. */
    btypes b; /**< Type of the process (e.g., claim, trace, proc). */
    short tn; /**< Ordinal number. */
    unsigned char det; /**< Indicates if the process is deterministic. */
    unsigned char unsafe; /**< Indicates if the process contains global variable initializations. */
    unsigned char priority; /**< Process priority, if any. */
    ProcList *next; /**< Linked list of processes. */
};

} // namespace models