#pragma once

#include "../models/models_fwd.hpp"

namespace run {

int PCHighest(models::Lextok *n);

int Enabled(models::Element *);

int Eval(models::Lextok *now);

models::Element *EvalSub(models::Element *e);

} // namespace run