#pragma once

struct Sequence;
struct SeqList;

namespace models {

enum { INIV, PUTV, LOGV }; /* used in pangen1.c */
enum btypes { NONE, N_CLAIM, I_PROC, A_PROC, P_PROC, E_TRACE, N_TRACE };

struct Access;
struct Slicer;
struct Lextok;
struct Symbol;
struct Ordered;
struct FSM_state;
struct FSM_trans;
struct FSM_use;

} // namespace models