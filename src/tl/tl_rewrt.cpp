/***** tl_spin: tl_rewrt.c *****/

/*
 * This file is part of the public release of Spin. It is subject to the
 * terms in the LICENSE file that is included in this source directory.
 * Tool documentation is available at http://spinroot.com
 *
 * Based on the translation algorithm by Gerth, Peled, Vardi, and Wolper,
 * presented at the PSTV Conference, held in 1995, Warsaw, Poland 1995.
 */

#include "tl.hpp"

extern int tl_verbose;

static Node *can = ZN;

void ini_rewrt(void) { can = ZN; }

Node *right_linked(Node *n) {
  if (!n)
    return n;

  if (n->ntyp == AND || n->ntyp == OR)
    while (n->left && n->left->ntyp == n->ntyp) {
      Node *tmp = n->left;
      n->left = tmp->right;
      tmp->right = n;
      n = tmp;
    }

  n->left = right_linked(n->left);
  n->right = right_linked(n->right);

  return n;
}

Node *canonical(Node *n) {
  Node *m; /* assumes input is right_linked */

  if (!n)
    return n;
  if ((m = in_cache(n)) != ZN)
    return m;

  n->right = canonical(n->right);
  n->left = canonical(n->left);

  return cached(n);
}

Node *push_negation(Node *n) {
  Node *m;

  Assert(n->ntyp == NOT, n->ntyp);

  switch (n->left->ntyp) {
  case TRUE:
    Debug("!true => false\n");
    releasenode(0, n->left);
    n->left = ZN;
    n->ntyp = FALSE;
    break;
  case FALSE:
    Debug("!false => true\n");
    releasenode(0, n->left);
    n->left = ZN;
    n->ntyp = TRUE;
    break;
  case NOT:
    Debug("!!p => p\n");
    m = n->left->left;
    releasenode(0, n->left);
    n->left = ZN;
    releasenode(0, n);
    n = m;
    break;
  case V_OPER:
    Debug("!(p V q) => (!p U !q)\n");
    n->ntyp = U_OPER;
    goto same;
  case U_OPER:
    Debug("!(p U q) => (!p V !q)\n");
    n->ntyp = V_OPER;
    goto same;
#ifdef NXT
  case NEXT:
    Debug("!X -> X!\n");
    n->ntyp = NEXT;
    n->left->ntyp = NOT;
    n->left = push_negation(n->left);
    break;
#endif
  case AND:
    Debug("!(p && q) => !p || !q\n");
    n->ntyp = OR;
    goto same;
  case OR:
    Debug("!(p || q) => !p && !q\n");
    n->ntyp = AND;

  same:
    m = n->left->right;
    n->left->right = ZN;

    n->right = Not(m);
    n->left->ntyp = NOT;
    m = n->left;
    n->left = push_negation(m);
    break;
  }

  return rewrite(n);
}

static void addcan(int tok, Node *n) {
  Node *m, *prev = ZN;
  Node **ptr;
  Node *N;
  Symbol *s, *t;
  int cmp;

  if (!n)
    return;

  if (n->ntyp == tok) {
    addcan(tok, n->right);
    addcan(tok, n->left);
    return;
  }

  N = dupnode(n);
  if (!can) {
    can = N;
    return;
  }

  s = DoDump(N);
  if (!s) {
    loger::fatal("unexpected error 6");
  }
  if (can->ntyp != tok) /* only one element in list so far */
  {
    ptr = &can;
    goto insert;
  }

  /* there are at least 2 elements in list */
  prev = ZN;
  for (m = can; m->ntyp == tok && m->right; prev = m, m = m->right) {
    t = DoDump(m->left);
    if (t != ZS)
      cmp = strcmp(s->name, t->name);
    else
      cmp = 0;
    if (cmp == 0) /* duplicate */
      return;
    if (cmp < 0) {
      if (!prev) {
        can = tl_nn(tok, N, can);
        return;
      } else {
        ptr = &(prev->right);
        goto insert;
      }
    }
  }

  /* new entry goes at the end of the list */
  ptr = &(prev->right);
insert:
  t = DoDump(*ptr);
  cmp = strcmp(s->name, t->name);
  if (cmp == 0) /* duplicate */
    return;
  if (cmp < 0)
    *ptr = tl_nn(tok, N, *ptr);
  else
    *ptr = tl_nn(tok, *ptr, N);
}

static void marknode(int tok, Node *m) {
  if (m->ntyp != tok) {
    releasenode(0, m->right);
    m->right = ZN;
  }
  m->ntyp = -1;
}

Node *Canonical(Node *n) {
  Node *m, *p, *k1, *k2, *prev, *dflt = ZN;
  int tok;

  if (!n)
    return n;

  tok = n->ntyp;
  if (tok != AND && tok != OR)
    return n;

  can = ZN;
  addcan(tok, n);
  releasenode(1, n);

  /* mark redundant nodes */
  if (tok == AND) {
    for (m = can; m; m = (m->ntyp == AND) ? m->right : ZN) {
      k1 = (m->ntyp == AND) ? m->left : m;
      if (k1->ntyp == TRUE) {
        marknode(AND, m);
        dflt = True;
        continue;
      }
      if (k1->ntyp == FALSE) {
        releasenode(1, can);
        can = False;
        goto out;
      }
    }
    for (m = can; m; m = (m->ntyp == AND) ? m->right : ZN)
      for (p = can; p; p = (p->ntyp == AND) ? p->right : ZN) {
        if (p == m || p->ntyp == -1 || m->ntyp == -1)
          continue;
        k1 = (m->ntyp == AND) ? m->left : m;
        k2 = (p->ntyp == AND) ? p->left : p;

        if (isequal(k1, k2)) {
          marknode(AND, p);
          continue;
        }
        if (anywhere(OR, k1, k2)) {
          marknode(AND, p);
          continue;
        }
      }
  }
  if (tok == OR) {
    for (m = can; m; m = (m->ntyp == OR) ? m->right : ZN) {
      k1 = (m->ntyp == OR) ? m->left : m;
      if (k1->ntyp == FALSE) {
        marknode(OR, m);
        dflt = False;
        continue;
      }
      if (k1->ntyp == TRUE) {
        releasenode(1, can);
        can = True;
        goto out;
      }
    }
    for (m = can; m; m = (m->ntyp == OR) ? m->right : ZN)
      for (p = can; p; p = (p->ntyp == OR) ? p->right : ZN) {
        if (p == m || p->ntyp == -1 || m->ntyp == -1)
          continue;
        k1 = (m->ntyp == OR) ? m->left : m;
        k2 = (p->ntyp == OR) ? p->left : p;

        if (isequal(k1, k2)) {
          marknode(OR, p);
          continue;
        }
        if (anywhere(AND, k1, k2)) {
          marknode(OR, p);
          continue;
        }
      }
  }
  for (m = can, prev = ZN; m;) /* remove marked nodes */
  {
    if (m->ntyp == -1) {
      k2 = m->right;
      releasenode(0, m);
      if (!prev) {
        m = can = can->right;
      } else {
        m = prev->right = k2;
        /* if deleted the last node in a chain */
        if (!prev->right && prev->left &&
            (prev->ntyp == AND || prev->ntyp == OR)) {
          k1 = prev->left;
          prev->ntyp = prev->left->ntyp;
          prev->symbol = prev->left->symbol;
          prev->right = prev->left->right;
          prev->left = prev->left->left;
          releasenode(0, k1);
        }
      }
      continue;
    }
    prev = m;
    m = m->right;
  }
out:
  if (!can) {
    if (!dflt)
      loger::fatal("cannot happen, Canonical");
    return dflt;
  }

  return can;
}
