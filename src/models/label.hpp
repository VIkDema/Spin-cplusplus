#pragma once

#include "models_fwd.hpp"

namespace models {
/**
 * @struct Label
 * Structure representing a label.
 */
struct Label {
    Symbol *s; /**< Symbol representing the label. */
    Symbol *c; /**< Symbol representing the context. */
    Element *e; /**< Pointer to the element. */
    int opt_inline_id; /**< Non-zero if the label appears in an inline. */
    int visible; /**< Indicates if the label is referenced in a claim (slice relevant). */
    Label *next; /**< Pointer to the next label. */
};

/**
 * @struct Lbreak
 * Structure representing a label break.
 */
struct Lbreak {
    Symbol *l; /**< Symbol representing the label. */
    Lbreak *next; /**< Pointer to the next label break. */
};

/**
 * @struct L_List
 * Structure representing a list of elements.
 */
struct L_List {
    Lextok *n; /**< Pointer to the element. */
    L_List *next; /**< Pointer to the next element in the list. */
};


} // namespace models