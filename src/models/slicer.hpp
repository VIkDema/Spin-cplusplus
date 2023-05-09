#pragma once

#include "models_fwd.hpp"

namespace models {

struct Slicer {
  models::Lextok *slice_criterion;  /* global var, usable as slice criterion */
  short code;         /* type of use: DEREF_USE or normal USE */
  short used;         /* set when handled */
  struct Slicer *next; /* linked list */
};

} // namespace models