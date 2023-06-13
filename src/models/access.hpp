#pragma once

#include "models_fwd.hpp"

namespace models {

/**
 * @struct Access
 * Structure representing access to an object.
 */
struct Access {
    models::Symbol *who; /**< Pointer to the proctype symbol representing the accessing object. */
    models::Symbol *what; /**< Pointer to the proctype symbol representing the accessed object. */
    int count; /**< Parameter number and access type ('s' for write, 'r' for read). */
    int type; /**< Access type. */
    struct Access *next; /**< Pointer to the next element in the linked list. */
};


} // namespace models