#pragma once
#include "../models/models_fwd.hpp"

namespace trail {

int GetProgramCounterValue(models::Lextok *);

void ProcessTrailFile();

int FindMinSequence(models::Sequence *);

int FindMaxSequence(models::Sequence *);
} // namespace trail