#pragma once
#include "../models/models_fwd.hpp"
#include <string>

namespace mesg {

void ValidateSymbolAssignment(models::Lextok *);

void CheckAndProcessChannelAssignment(models::Lextok *, models::Lextok *, int);

void PrintQueueContents(models::Symbol *s, int n, models::RunList *r);

void FormatMessage(int, int, const std::string &);

void PrintFormattedMessage(FILE *, int, int, const std::string &);

void HideQueue(int);

void CheckTypeClash(int, int, const std::string &);

int QReceive(models::Lextok *, int);

int QSend(models::Lextok *);

int QIsSync(models::Lextok *);

int GetCountMPars(models::Lextok *);

int QMake(models::Symbol *);

int QFull(models::Lextok *);

int QLen(models::Lextok *);

} // namespace mesg