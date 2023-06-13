#pragma once

#include "models_fwd.hpp"

namespace models {

/**
 * @struct QH
 * Structure representing a queue header.
 */
struct QH {
    int n; /**< Number. */
    struct QH *next; /**< Pointer to the next queue header. */
};


} // namespace models