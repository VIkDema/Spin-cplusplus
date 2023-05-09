/***** tl_spin: tl_cache.c *****/

/*
 * This file is part of the public release of Spin. It is subject to the
 * terms in the LICENSE file that is included in this source directory.
 * Tool documentation is available at http://spinroot.com
 *
 * Based on the translation algorithm by Gerth, Peled, Vardi, and Wolper,
 * presented at the PSTV Conference, held in 1995, Warsaw, Poland 1995.
 */

#include "tl.hpp"

struct Cache {
  Node *before;
  Node *after;
  int same;
  struct Cache *next;
};

static Cache *stored = (Cache *)0;
static unsigned long Caches, CacheHits;

static int ismatch(Node *, Node *);
static int sameform(Node *, Node *);

extern void fatal(char *, char *);

void ini_cache(void) {
  stored = (Cache *)0;
  Caches = 0;
  CacheHits = 0;
}


Node *in_cache(Node *n) {
  Cache *d;
  int nr = 0;

  for (d = stored; d; d = d->next, nr++)
    if (isequal(d->before, n)) {
      CacheHits++;
      if (d->same && ismatch(n, d->before))
        return n;
      return dupnode(d->after);
    }
  return ZN;
}

Node *cached(Node *n) {
  Cache *d;
  Node *m;

  if (!n)
    return n;
  if ((m = in_cache(n)) != ZN)
    return m;

  Caches++;
  d = (Cache *)tl_emalloc(sizeof(Cache));
  d->before = dupnode(n);
  d->after = Canonical(n); /* n is released */

  if (ismatch(d->before, d->after)) {
    d->same = 1;
    releasenode(1, d->after);
    d->after = d->before;
  }
  d->next = stored;
  stored = d;
  return dupnode(d->after);
}

void cache_stats(void) {
  printf("cache stores     : %9ld\n", Caches);
  printf("cache hits       : %9ld\n", CacheHits);
}

void releasenode(int all_levels, Node *n) {
  if (!n)
    return;

  if (all_levels) {
    releasenode(1, n->left);
    n->left = ZN;
    releasenode(1, n->right);
    n->right = ZN;
  }
  tfree((void *)n);
}

Node *tl_nn(int t, Node *ll, Node *rl) {
  Node *n = (Node *)tl_emalloc(sizeof(Node));

  n->ntyp = (short)t;
  n->left = ll;
  n->right = rl;

  return n;
}

Node *getnode(Node *p) {
  Node *n;

  if (!p)
    return p;

  n = (Node *)tl_emalloc(sizeof(Node));
  n->ntyp = p->ntyp;
  n->symbol = p->symbol; /* same name */
  n->left = p->left;
  n->right = p->right;

  return n;
}

Node *dupnode(Node *n) {
  Node *d;

  if (!n)
    return n;
  d = getnode(n);
  d->left = dupnode(n->left);
  d->right = dupnode(n->right);
  return d;
}

int one_lft(int ntyp, Node *x, Node *in) {
  if (!x)
    return 1;
  if (!in)
    return 0;

  if (sameform(x, in))
    return 1;

  if (in->ntyp != ntyp)
    return 0;

  if (one_lft(ntyp, x, in->left))
    return 1;

  return one_lft(ntyp, x, in->right);
}

int all_lfts(int ntyp, Node *from, Node *in) {
  if (!from)
    return 1;

  if (from->ntyp != ntyp)
    return one_lft(ntyp, from, in);

  if (!one_lft(ntyp, from->left, in))
    return 0;

  return all_lfts(ntyp, from->right, in);
}

int sametrees(int ntyp, Node *a, Node *b) { /* toplevel is an AND or OR */
  /* both trees are right-linked, but the leafs */
  /* can be in different places in the two trees */

  if (!all_lfts(ntyp, a, b))
    return 0;

  return all_lfts(ntyp, b, a);
}

static int /* a better isequal() */
sameform(Node *a, Node *b) {
  if (!a && !b)
    return 1;
  if (!a || !b)
    return 0;
  if (a->ntyp != b->ntyp)
    return 0;

  if (a->symbol && b->symbol && strcmp(a->symbol->name, b->symbol->name) != 0)
    return 0;

  switch (a->ntyp) {
  case TRUE:
  case FALSE:
    return 1;
  case PREDICATE:
    if (!a->symbol || !b->symbol)
      loger::fatal("sameform...");
    return !strcmp(a->symbol->name, b->symbol->name);

  case NOT:
#ifdef NXT
  case NEXT:
#endif
  case CEXPR:
    return sameform(a->left, b->left);
  case U_OPER:
  case V_OPER:
    if (!sameform(a->left, b->left))
      return 0;
    if (!sameform(a->right, b->right))
      return 0;
    return 1;

  case AND:
  case OR: /* the hard case */
    return sametrees(a->ntyp, a, b);

  default:
    printf("type: %d\n", a->ntyp);
    loger::fatal("cannot happen, sameform");
  }

  return 0;
}

int isequal(Node *a, Node *b) {
  if (!a && !b)
    return 1;

  if (!a || !b) {
    if (!a) {
      if (b->ntyp == TRUE)
        return 1;
    } else {
      if (a->ntyp == TRUE)
        return 1;
    }
    return 0;
  }
  if (a->ntyp != b->ntyp)
    return 0;

  if (a->symbol && b->symbol && strcmp(a->symbol->name, b->symbol->name) != 0)
    return 0;

  if (isequal(a->left, b->left) && isequal(a->right, b->right))
    return 1;

  return sameform(a, b);
}

static int ismatch(Node *a, Node *b) {
  if (!a && !b)
    return 1;
  if (!a || !b)
    return 0;
  if (a->ntyp != b->ntyp)
    return 0;

  if (a->symbol && b->symbol && strcmp(a->symbol->name, b->symbol->name) != 0)
    return 0;

  if (ismatch(a->left, b->left) && ismatch(a->right, b->right))
    return 1;

  return 0;
}

int any_term(Node *srch, Node *in) {
  if (!in)
    return 0;

  if (in->ntyp == AND)
    return any_term(srch, in->left) || any_term(srch, in->right);

  return isequal(in, srch);
}

int any_and(Node *srch, Node *in) {
  if (!in)
    return 0;

  if (srch->ntyp == AND)
    return any_and(srch->left, in) && any_and(srch->right, in);

  return any_term(srch, in);
}

int any_lor(Node *srch, Node *in) {
  if (!in)
    return 0;

  if (in->ntyp == OR)
    return any_lor(srch, in->left) || any_lor(srch, in->right);

  return isequal(in, srch);
}

int anywhere(int tok, Node *srch, Node *in) {
  if (!in)
    return 0;

  switch (tok) {
  case AND:
    return any_and(srch, in);
  case OR:
    return any_lor(srch, in);
  case 0:
    return any_term(srch, in);
  }
  loger::fatal("cannot happen, anywhere");
  return 0;
}
