#pragma once
#include "models_fwd.hpp"

namespace models {
/**
 * @struct UType
 * Structure representing a user-defined type.
 */
struct UType {
    Symbol *nm; /**< Name of the type. */
    Lextok *cn; /**< Contents of the type. */
    UType *next; /**< Pointer to the next UType in the linked list. */
};
} // namespace models
