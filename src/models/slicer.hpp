#pragma once

#include "models_fwd.hpp"

namespace models {

/**
 * @struct Slicer
 * Structure representing a slicer.
 */
struct Slicer {
    models::Lextok *slice_criterion; /**< Global variable usable as a slice criterion. */
    short code; /**< Type of use: DEREF_USE or normal USE. */
    short used; /**< Flag indicating if the slicer has been handled. */
    struct Slicer *next; /**< Pointer to the next slicer in the linked list. */
};


} // namespace models