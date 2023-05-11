#pragma once
#include "models_fwd.hpp"

namespace models {
struct UType {
  Symbol *nm;  /* name of the type */
  Lextok *cn;  /* contents */
  UType *next; /* linked list */
};
} // namespace models
