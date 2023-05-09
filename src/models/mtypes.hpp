#pragma once

#include "models_fwd.hpp"
#include <string>

namespace models {

struct Mtypes_t {
  std::string name_of_mtype;       /* name of mtype, or "_unnamed_" */
  models::Lextok *list_of_names;   /* the linked list of names */
  struct Mtypes_t *next; /* linked list of mtypes */
};

} // namespace models