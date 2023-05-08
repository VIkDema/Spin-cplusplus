#pragma once

#include "models_fwd.hpp"

namespace models {

struct Lextok {
  unsigned short node_type; /* node type  OLD: node_type*/
  bool is_mtype_token;      /* CONST derived from MTYP OLD: ismtyp*/
  int value;                /* value attribute old: val*/
  int line_number;          /* line number old:ln */
  int index_step;           /* part of d_step sequence, indstep */
  int opt_inline_id;        /* inline id, if non-zero,  uiid*/
  Symbol *file_name;        /* file name old: fn*/
  Symbol *symbol;           /* symbol reference old: sym*/
  Sequence *sequence;       /* sequence old: sq*/
  SeqList *seq_list;        /* sequence list old: sl*/
  Lextok *left, *right;     /* children in parse tree old: fn*/
};
} // namespace models