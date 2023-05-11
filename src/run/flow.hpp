#pragma once
#include "../models/models_fwd.hpp"

namespace flow {

void DumpLabels();

models::Lextok *SelectIndex(models::Lextok *, models::Lextok *,
                            models::Lextok *);

models::Lextok *BuildForBody(models::Lextok *, int);

models::Lextok *BuildForIndex(models::Lextok *, models::Lextok *);

void SetupForLoop(models::Lextok *, models::Lextok *, models::Lextok *);

void MakeAtomic(models::Sequence *, int);

models::Symbol *GetBreakDestination();

void AddBreakDestination();

void RestoreBreakDestinantion();

void SaveBreakDestinantion();

int FindLabel(models::Symbol *, models::Symbol *, int);

void FixLabelRef(models::Symbol *, models::Symbol *);

models::Symbol *HasLabel(models::Element *, int);

models::Element *GetLabel(models::Lextok *, int);

void SetLabel(models::Symbol *, models::Element *);

void AddSequence(models::Lextok *);

models::Sequence *CloseSequence(int nottop);

void OpenSequence(int top);

void TieUpLooseEnds();

models::Lextok *DoUnless(models::Lextok *, models::Lextok *);

void PruneOpts(models::Lextok *);

void CrossDsteps(models::Lextok *, models::Lextok *);

void StartDStepSequence();
void EndDStepSequence();

void SetLabel(models::Symbol *, models::Element *);

} // namespace flow