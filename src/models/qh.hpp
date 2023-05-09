#pragma once

#include "models_fwd.hpp"

namespace models {

struct QH {
  int n;
  struct QH *next;
};

} // namespace models