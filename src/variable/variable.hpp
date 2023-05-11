#pragma once
#include "../models/models_fwd.hpp"
#include <cstdio>
#include <string>

namespace variable {

void DumpGlobals();

void DumpLocal(models::RunList *, int);

void DumpClaims(FILE *, int, const std::string &);

int GetValue(models::Lextok *);

int SetVal(models::Lextok *, int);

int CheckVar(models::Symbol *, int);

int CastValue(int, int, int);

} // namespace variable