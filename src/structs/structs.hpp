#pragma once
#include "../models/models_fwd.hpp"
#include <string>

namespace structs {

void SetPname(models::Lextok *);

models::Lextok *mk_explicit(models::Lextok *, int, int);

models::Lextok *ExpandLextok(models::Lextok *, int);

void DumpStruct(models::Symbol *, const std::string &, models::RunList *);

void CStruct(FILE *, const std::string &, models::Symbol *);

void WalkStruct(FILE *, int, const std::string &, models::Symbol *,
                const std::string &, const std::string &, const std::string &);

void WalkStruct(const std::string &, models::Symbol *);

void GetStructName(models::Lextok *, models::Symbol *, int, std::string &);

void CheckValidRef(models::Lextok *, models::Lextok *);

int GetFullName(FILE *, models::Lextok *, models::Symbol *, int);

void InitStruct(models::Symbol *);

int GetWidth(int *, int, models::Lextok *);

int CountFields(models::Lextok *);

int Lval_struct(models::Lextok *, models::Symbol *, int, int);

int Rval_struct(models::Lextok *, models::Symbol *, int);

void SetUtype(models::Lextok *, models::Symbol *, models::Lextok *);
models::Lextok *GetUname(models::Symbol *);

void PrintUnames(FILE *);

void SetUname(models::Lextok *);

bool IsUtype(const std::string &);

} // namespace structs