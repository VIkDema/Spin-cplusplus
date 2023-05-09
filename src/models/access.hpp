#pragma once

#include "models_fwd.hpp"

namespace models {

struct Access {
  models::Symbol *who;  /* proctype name of accessor */
  models::Symbol *what; /* proctype name of accessed */
  int count;            /* parameter nr and, e.g., 's' or 'r' */
  int type;
  struct Access *next; /* linked list */
};

} // namespace models