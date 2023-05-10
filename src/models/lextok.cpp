#pragma once

#include "lextok.hpp"

#include "../fatal/fatal.hpp"
#include "../lexer/inline_processor.hpp"
#include "../lexer/line_number.hpp"
#include "../main/launch_settings.hpp"
#include "symbol.hpp"
#include "y.tab.h"
#include <fmt/core.h>
#include <iostream>

extern LaunchSettings launch_settings;
extern models::Symbol *Fname;
extern int realread;
extern char *emalloc(size_t);
extern models::Symbol *context;
extern int DstepStart;
int Etimeouts; /* nr timeouts in program */
int Ntimeouts; /* nr timeouts in never claim */
extern char *claimproc;
int has_remote, has_remvar;

namespace models {
void Lextok::ProcessSymbolForRead() {
  if (symbol == nullptr) {
    return;
  }
  if (realread &&
      (symbol->type == SymbolType::kMtype ||
       symbol->type == SymbolType::kUnsigned ||
       symbol->type == SymbolType::kShort ||
       symbol->type == SymbolType::kByte || symbol->type == SymbolType::kInt)) {
    symbol->hidden_flags |= 128; /* var is read at least once */
  }
}

Lextok *Lextok::nn(models::Lextok *symbol, int type, models::Lextok *left,
                   models::Lextok *right) {
  models::Lextok *n = (models::Lextok *)emalloc(sizeof(models::Lextok));
  static int warn_nn = 0;

  n->opt_inline_id =
      lexer::InlineProcessor::GetCurrInlineUuid(); /* record origin of the
                                                      statement */
  n->node_type = (unsigned short)type;
  if (symbol && symbol->file_name) {
    n->line_number = symbol->line_number;
    n->file_name = symbol->file_name;
  } else if (right && right->file_name) {
    n->line_number = right->line_number;
    n->file_name = right->file_name;
  } else if (left && left->file_name) {
    n->line_number = left->line_number;
    n->file_name = left->file_name;
  } else {
    n->line_number = file::LineNumber::Get();
    n->file_name = Fname;
  }

  if (symbol)
    n->symbol = symbol->symbol;
  n->left = left;
  n->right = right;
  n->index_step = DstepStart;

  if (type == TIMEOUT)
    Etimeouts++;

  if (!context)
    return n;

  if (type == 'r' || type == 's') {
    n->symbol->AddAccess(ZS, 0, 't');
  }
  if (type == 'R') {
    n->symbol->AddAccess(ZS, 0, 'P');
  }

  if (context->name.c_str() == claimproc) {
    int forbidden = launch_settings.separate_version;
    switch (type) {
    case ASGN:
      printf("spin: Warning, never claim has side-effect\n");
      break;
    case 'r':
    case 's':
      loger::non_fatal("never claim contains i/o stmnts");
      break;
    case TIMEOUT:
      /* never claim polls timeout */
      if (Ntimeouts && Etimeouts)
        forbidden = 0;
      Ntimeouts++;
      Etimeouts--;
      break;
    case LEN:
    case EMPTY:
    case FULL:
    case 'R':
    case NFULL:
    case NEMPTY:
      /* status becomes non-exclusive */
      if (n->symbol && !(n->symbol->xu & XX)) {
        n->symbol->xu |= XX;
        if (launch_settings.separate_version == 2) {
          printf("spin: warning, make sure that the S1 model\n");
          printf("      also polls channel '%s' in its claim\n",
                 n->symbol->name.c_str());
        }
      }
      forbidden = 0;
      break;
    case 'c':
      AST_track(n, 0); /* register as a slice criterion */
                       /* fall thru */
    default:
      forbidden = 0;
      break;
    }
    if (forbidden) {
      std::cout << "spin: never, saw " << loger::explainToString(type)
                << std::endl;
      loger::fatal("incompatible with separate compilation");
    }
  } else if ((type == ENABLED || type == PC_VAL) && !(warn_nn & type)) {
    std::cout << fmt::format("spin: Warning, using {} outside never claim",
                             (type == ENABLED) ? "enabled()" : "pc_value()")
              << std::endl;
    warn_nn |= type;
  } else if (type == NONPROGRESS) {
    loger::fatal("spin: Error, using np_ outside never claim\n");
  }
  return n;
}

Lextok *Lextok::CreateRemoteLabelAssignment(models::Symbol *proctype_name,
                                            models::Lextok *pid,
                                            models::Symbol *label_name) {
  models::Lextok *tmp1, *tmp2, *tmp3;
  has_remote++;
  label_name->type = models::kLabel;   /* refered to in global context here */
  fix_dest(label_name, proctype_name); /* in case target of rem_lab is jump */
  tmp1 = nn(ZN, '?', pid, ZN);
  tmp1->symbol = proctype_name;
  tmp1 = nn(ZN, 'p', tmp1, ZN);
  tmp1->symbol = lookup("_p");
  tmp2 = nn(ZN, NAME, ZN, ZN);
  tmp2->symbol = proctype_name;
  tmp3 = nn(ZN, 'q', tmp2, ZN);
  tmp3->symbol = label_name;
  return nn(ZN, EQ, tmp1, tmp3);
#if 0
	      .---------------EQ-------.
	     /                          \
	   'p' -sym-> _p               'q' -sym-> c (label name)
	   /                           /
	 '?' -sym-> a (proctype)     NAME -sym-> a (proctype name)
	 / 
	b (pid expr)
#endif
}

Lextok *Lextok::CreateRemoteVariableAssignment(models::Symbol *proctype_name,
                                               models::Lextok *pid,
                                               models::Symbol *variable_name,
                                               models::Lextok *ndx) {
  models::Lextok *tmp1;

  has_remote++;
  has_remvar++;
  launch_settings.need_use_dataflow_optimizations = false;
  launch_settings.need_statemate_merging = false;

  tmp1 = models::Lextok::nn(ZN, '?', pid, ZN);
  tmp1->symbol = proctype_name;
  tmp1 = models::Lextok::nn(ZN, 'p', tmp1, ndx);
  tmp1->symbol = variable_name;
  return tmp1;
#if 0
	cannot refer to struct elements
	only to scalars and arrays

	    'p' -sym-> c (variable name)
	    / \______  possible arrayindex on c
	   /
	 '?' -sym-> a (proctype)
	 / 
	b (pid expr)
#endif
}

int Lextok::ResolveSymbolType() {

  if (symbol == nullptr) {
    return 0;
  }
  if (symbol->type != models::SymbolType::kStruct) {
    return symbol->type;
  }

  if (!right || right->node_type != '.' /* gh: had ! in wrong place */
      || !right->left) {
    return STRUCT; /* not a field reference */
  }
  return right->left->ResolveSymbolType();
}

} // namespace models