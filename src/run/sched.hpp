#pragma once

#include "../models/models_fwd.hpp"
#include <string>

namespace sched {

void ScheduleProcesses();

int ResolveRemoteVariableReference(models::Lextok *);

int ResolveRemoteLabelReference(models::Lextok *);

void DisplayExecutionStatus(models::Element *, int);

void PrintCurrentProcessInfo(int);

int AssignLocalValue(models::Lextok *, int);

int GetLocalValue(models::Lextok *);

int IsIndexInBounds(models::Symbol *, int);

models::Symbol *FindOrCreateLocalSymbol(models::Symbol *);

int PerformRendezvousCompletion();

void RenameWrapup(int);

void InitializeClaimExecution(int);

void ValidateParameterCount(int, models::Lextok *);

int ActivateProcess(models::Lextok *);

void DisplayProcessCreation(const std::string &);

int IncrementSymbolMaxElement(models::Symbol *);

void ValidateMTypeArguments(models::Lextok *, models::Lextok *);

models::ProcList *CreateProcessEntry(models::Symbol *, models::Lextok *,
                                     models::Sequence *, int, models::Lextok *,
                                     models::btypes);

void InitializeRunnableProcess(models::ProcList *, int, int);

void DoTag(FILE *fd, char *s);
} // namespace sched
