#pragma once
#include "../models/models_fwd.hpp"
#include <string>

namespace symbol {

void CheckChanAccess();

void DumpSymbols();

void PrintSymbolVariable(models::Symbol *);

int IsMtype(const std::string &);

std::string GetMtypeName(const std::string &);

void AddMtype(models::Lextok *, models::Lextok *);

models::Lextok **GetListOfMtype(const std::string &s);

void SetXus(models::Lextok *, int);

void SetPname(models::Lextok *, models::Lextok *, int, models::Lextok *);

void TrackUseChan(models::Lextok *, models::Lextok *, int);

void TrackVar(models::Lextok* n, models::Lextok* m);

void EliminateAmbiguity();

void CheckRun(models::Symbol *, int);
void TrackRun(models::Lextok* n);

} // namespace symbol