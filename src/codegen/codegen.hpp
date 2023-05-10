#pragma once

#include <cstdio>
#include <string>
#include "../models/models_fwd.hpp"

namespace codegen {

void GenCodeTable(FILE *fd);

void PlunkInline(FILE *fd, const std::string &s, int how, int gencode);

void PlunkExpr(FILE *fd, const std::string &s);

void HandleCState(models::Symbol *s, models::Symbol *t, models::Symbol *ival);

void HandleCTrack(models::Symbol *s, models::Symbol *t,
                  models::Symbol *stackonly);

void HandleCDescls(FILE *fd);

void HandleCFCTS(FILE *fd);

void AddLocInit(FILE *fd, int tpnr, const std::string &pnm);

void CPreview();
int CAddSv(FILE *fd);
void CStackSize(FILE *fd);
void CAddStack(FILE *fd);
void CAddLoc(FILE *fd, const std::string &s);
void CAddDef(FILE *fd);

void PreRuse(FILE *fd, models::Lextok *n);
char* PutInline(FILE* fd, const std::string& s);
} // namespace codegen