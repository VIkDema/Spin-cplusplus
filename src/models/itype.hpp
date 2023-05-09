#pragma once

#include "models_fwd.hpp"

namespace models {
struct IType {
  Symbol *nm;                /* name of the type */
  Lextok *cn;                /* contents */
  Lextok *params;            /* formal pars if any */
  Lextok *rval;              /* variable to assign return value, if any */
  char **anms;               /* literal text for actual pars */
  char *prec;                /* precondition for c_code or c_expr */
  int uiid;                  /* unique inline id */
  int is_expr;               /* c_expr in an ltl formula */
  int dln, cln;              /* def and call linenr */
  Symbol *dfn, *cfn; /* def and call filename */
  IType *next;               /* linked list */
};
} // namespace models
