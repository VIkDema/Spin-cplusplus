#pragma once

#include "models_fwd.hpp"
#include <string>

namespace models {

/**
 * @struct Mtypes_t
 * Structure representing multiple types.
 */
struct Mtypes_t {
    std::string name_of_mtype; /**< Name of the mtype, or "_unnamed_". */
    models::Lextok *list_of_names; /**< Linked list of names. */
    struct Mtypes_t *next; /**< Linked list of multiple types. */
};

} // namespace models