#pragma once

#include "../../spin.hpp"

namespace models {
struct IType {
  models::Symbol *nm;        /* name of the type */
  models::Lextok *cn;        /* contents */
  models::Lextok *params;    /* formal pars if any */
  models::Lextok *rval;      /* variable to assign return value, if any */
  char **anms;       /* literal text for actual pars */
  char *prec;        /* precondition for c_code or c_expr */
  int uiid;          /* unique inline id */
  int is_expr;       /* c_expr in an ltl formula */
  int dln, cln;      /* def and call linenr */
  models::Symbol *dfn, *cfn; /* def and call filename */
  struct IType *nxt; /* linked list */
};
} // namespace models
