/***** tl_spin: tl_parse.c *****/

/*
 * This file is part of the public release of Spin. It is subject to the
 * terms in the LICENSE file that is included in this source directory.
 * Tool documentation is available at http://spinroot.com
 *
 * Based on the translation algorithm by Gerth, Peled, Vardi, and Wolper,
 * presented at the PSTV Conference, held in 1995, Warsaw, Poland 1995.
 */

#include "tl.hpp"

extern int tl_yylex(void);
extern int tl_verbose, tl_errs;

int tl_yychar = 0;
YYSTYPE tl_yylval;

static Node *tl_formula(void);
static Node *tl_factor(void);
static Node *tl_level(int);

static int prec[2][4] = {
    {U_OPER, V_OPER, 0, 0}, /* left associative */
    {
        OR,
        AND,
        IMPLIES,
        EQUIV,
    }, /* left associative */
};

static Node *tl_factor(void) {
  Node *ptr = ZN;

  switch (tl_yychar) {
  case '(':
    ptr = tl_formula();
    if (tl_yychar != ')')
      tl_yyerror("expected ')'");
    tl_yychar = tl_yylex();
    break;
  case NOT:
    ptr = tl_yylval;
    tl_yychar = tl_yylex();
    ptr->left = tl_factor();
    if (!ptr->left) {
      loger::fatal("malformed expression");
    }
    ptr = push_negation(ptr);
    break;
  case ALWAYS:
    tl_yychar = tl_yylex();
    ptr = tl_factor();
#ifndef NO_OPT
    if (ptr->ntyp == FALSE || ptr->ntyp == TRUE)
      break; /* [] false == false */

    if (ptr->ntyp == V_OPER) {
      if (ptr->left->ntyp == FALSE)
        break; /* [][]p = []p */

      ptr = ptr->right; /* [] (p V q) = [] q */
    }
#endif
    ptr = tl_nn(V_OPER, False, ptr);
    break;
#ifdef NXT
  case NEXT:
    tl_yychar = tl_yylex();
    ptr = tl_factor();
    if (ptr->ntyp == TRUE)
      break; /* X true = true */
    ptr = tl_nn(NEXT, ptr, ZN);
    break;
#endif
  case CEXPR:
    tl_yychar = tl_yylex();
    ptr = tl_factor();
    if (ptr->ntyp != PREDICATE) {
      tl_yyerror("expected {...} after c_expr");
    }
    ptr = tl_nn(CEXPR, ptr, ZN);
    break;
  case EVENTUALLY:
    tl_yychar = tl_yylex();

    ptr = tl_factor();
#ifndef NO_OPT
    if (ptr->ntyp == TRUE || ptr->ntyp == FALSE)
      break; /* <> true == true */

    if (ptr->ntyp == U_OPER && ptr->left->ntyp == TRUE)
      break; /* <><>p = <>p */

    if (ptr->ntyp == U_OPER) { /* <> (p U q) = <> q */
      ptr = ptr->right;
      /* fall thru */
    }
#endif
    ptr = tl_nn(U_OPER, True, ptr);

    break;
  case PREDICATE:
    ptr = tl_yylval;
    tl_yychar = tl_yylex();
    break;
  case TRUE:
  case FALSE:
    ptr = tl_yylval;
    tl_yychar = tl_yylex();
    break;
  }
  if (!ptr)
    tl_yyerror("expected predicate");
  return ptr;
}

static Node *bin_simpler(Node *ptr) {
  Node *a, *b;

  if (ptr)
    switch (ptr->ntyp) {
    case U_OPER:
#ifndef NO_OPT
      if (ptr->right->ntyp == TRUE || ptr->right->ntyp == FALSE ||
          ptr->left->ntyp == FALSE) {
        ptr = ptr->right;
        break;
      }
      if (isequal(ptr->left, ptr->right)) { /* p U p = p */
        ptr = ptr->right;
        break;
      }
      if (ptr->left->ntyp == U_OPER &&
          isequal(ptr->left->left, ptr->right)) { /* (p U q) U p = (q U p) */
        ptr->left = ptr->left->right;
        break;
      }
      if (ptr->right->ntyp == U_OPER &&
          ptr->right->left->ntyp == TRUE) { /* p U (T U q)  = (T U q) */
        ptr = ptr->right;
        break;
      }
#ifdef NXT
      /* X p U X q == X (p U q) */
      if (ptr->right->ntyp == NEXT && ptr->left->ntyp == NEXT) {
        ptr = tl_nn(NEXT, tl_nn(U_OPER, ptr->left->left, ptr->right->left), ZN);
      }
#endif
#endif
      break;
    case V_OPER:
#ifndef NO_OPT
      if (ptr->right->ntyp == FALSE || ptr->right->ntyp == TRUE ||
          ptr->left->ntyp == TRUE) {
        ptr = ptr->right;
        break;
      }
      if (isequal(ptr->left, ptr->right)) { /* p V p = p */
        ptr = ptr->right;
        break;
      }
      /* F V (p V q) == F V q */
      if (ptr->left->ntyp == FALSE && ptr->right->ntyp == V_OPER) {
        ptr->right = ptr->right->right;
        break;
      }
      /* p V (F V q) == F V q */
      if (ptr->right->ntyp == V_OPER && ptr->right->left->ntyp == FALSE) {
        ptr->left = False;
        ptr->right = ptr->right->right;
        break;
      }
#endif
      break;
    case IMPLIES:
#ifndef NO_OPT
      if (isequal(ptr->left, ptr->right)) {
        ptr = True;
        break;
      }
#endif
      ptr = tl_nn(OR, Not(ptr->left), ptr->right);
      ptr = rewrite(ptr);
      break;
    case EQUIV:
#ifndef NO_OPT
      if (isequal(ptr->left, ptr->right)) {
        ptr = True;
        break;
      }
#endif
      a = rewrite(tl_nn(AND, dupnode(ptr->left), dupnode(ptr->right)));
      b = rewrite(tl_nn(AND, Not(ptr->left), Not(ptr->right)));
      ptr = tl_nn(OR, a, b);
      ptr = rewrite(ptr);
      break;
    case AND:
#ifndef NO_OPT
      /* p && (q U p) = p */
      if (ptr->right->ntyp == U_OPER && isequal(ptr->right->right, ptr->left)) {
        ptr = ptr->left;
        break;
      }
      if (ptr->left->ntyp == U_OPER && isequal(ptr->left->right, ptr->right)) {
        ptr = ptr->right;
        break;
      }

      /* p && (q V p) == q V p */
      if (ptr->right->ntyp == V_OPER && isequal(ptr->right->right, ptr->left)) {
        ptr = ptr->right;
        break;
      }
      if (ptr->left->ntyp == V_OPER && isequal(ptr->left->right, ptr->right)) {
        ptr = ptr->left;
        break;
      }

      /* (p U q) && (r U q) = (p && r) U q*/
      if (ptr->right->ntyp == U_OPER && ptr->left->ntyp == U_OPER &&
          isequal(ptr->right->right, ptr->left->right)) {
        ptr = tl_nn(U_OPER, tl_nn(AND, ptr->left->left, ptr->right->left),
                    ptr->left->right);
        break;
      }

      /* (p V q) && (p V r) = p V (q && r) */
      if (ptr->right->ntyp == V_OPER && ptr->left->ntyp == V_OPER &&
          isequal(ptr->right->left, ptr->left->left)) {
        ptr = tl_nn(V_OPER, ptr->right->left,
                    tl_nn(AND, ptr->left->right, ptr->right->right));
        break;
      }
#ifdef NXT
      /* X p && X q == X (p && q) */
      if (ptr->right->ntyp == NEXT && ptr->left->ntyp == NEXT) {
        ptr = tl_nn(NEXT, tl_nn(AND, ptr->right->left, ptr->left->left), ZN);
        break;
      }
#endif

      if (isequal(ptr->left, ptr->right) /* (p && p) == p */
          || ptr->right->ntyp == FALSE  /* (p && F) == F */
          || ptr->left->ntyp == TRUE)  /* (T && p) == p */
      {
        ptr = ptr->right;
        break;
      }
      if (ptr->right->ntyp == TRUE      /* (p && T) == p */
          || ptr->left->ntyp == FALSE) /* (F && p) == F */
      {
        ptr = ptr->left;
        break;
      }

      /* (p V q) && (r U q) == p V q */
      if (ptr->right->ntyp == U_OPER && ptr->left->ntyp == V_OPER &&
          isequal(ptr->left->right, ptr->right->right)) {
        ptr = ptr->left;
        break;
      }
#endif
      break;

    case OR:
#ifndef NO_OPT
      /* p || (q U p) == q U p */
      if (ptr->right->ntyp == U_OPER && isequal(ptr->right->right, ptr->left)) {
        ptr = ptr->right;
        break;
      }

      /* p || (q V p) == p */
      if (ptr->right->ntyp == V_OPER && isequal(ptr->right->right, ptr->left)) {
        ptr = ptr->left;
        break;
      }

      /* (p U q) || (p U r) = p U (q || r) */
      if (ptr->right->ntyp == U_OPER && ptr->left->ntyp == U_OPER &&
          isequal(ptr->right->left, ptr->left->left)) {
        ptr = tl_nn(U_OPER, ptr->right->left,
                    tl_nn(OR, ptr->left->right, ptr->right->right));
        break;
      }

      if (isequal(ptr->left, ptr->right) /* (p || p) == p */
          || ptr->right->ntyp == FALSE  /* (p || F) == p */
          || ptr->left->ntyp == TRUE)  /* (T || p) == T */
      {
        ptr = ptr->left;
        break;
      }
      if (ptr->right->ntyp == TRUE      /* (p || T) == T */
          || ptr->left->ntyp == FALSE) /* (F || p) == p */
      {
        ptr = ptr->right;
        break;
      }

      /* (p V q) || (r V q) = (p || r) V q */
      if (ptr->right->ntyp == V_OPER && ptr->left->ntyp == V_OPER &&
          isequal(ptr->left->right, ptr->right->right)) {
        ptr = tl_nn(V_OPER, tl_nn(OR, ptr->left->left, ptr->right->left),
                    ptr->right->right);
        break;
      }

      /* (p V q) || (r U q) == r U q */
      if (ptr->right->ntyp == U_OPER && ptr->left->ntyp == V_OPER &&
          isequal(ptr->left->right, ptr->right->right)) {
        ptr = ptr->right;
        break;
      }
#endif
      break;
    }
  return ptr;
}

static Node *tl_level(int nr) {
  int i;
  Node *ptr = ZN;

  if (nr < 0)
    return tl_factor();

  ptr = tl_level(nr - 1);
again:
  for (i = 0; i < 4; i++)
    if (tl_yychar == prec[nr][i]) {
      tl_yychar = tl_yylex();
      ptr = tl_nn(prec[nr][i], ptr, tl_level(nr - 1));
      ptr = bin_simpler(ptr);
      goto again;
    }
  if (!ptr)
    tl_yyerror("syntax error");

  return ptr;
}

static Node *tl_formula(void) {
  tl_yychar = tl_yylex();
  return tl_level(1); /* 2 precedence levels, 1 and 0 */
}

void tl_parse(void) {
  Node *n;

  /* tl_verbose = 1; */
  n = tl_formula();
  if (tl_verbose) {
    printf("formula: ");
    dump(n);
    printf("\n");
  }
  if (tl_Getchar() != -1) {
    tl_yyerror("syntax error");
    tl_errs++;
    return;
  }
  trans(n);
}
