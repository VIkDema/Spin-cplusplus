/***** spin: reprosrc.c *****/

/*
 * This file is part of the public release of Spin. It is subject to the
 * terms in the LICENSE file that is included in this source directory.
 * Tool documentation is available at http://spinroot.com
 */

#include "spin.hpp"
#include "y.tab.h"
#include <assert.h>
#include <stdio.h>


extern ProcList *ready;
static void repro_seq(Sequence *);
static int indent = 1;

void doindent() {
  int i;
  for (i = 0; i < indent; i++)
    printf("   ");
}

void repro_sub(Element *e) {
  doindent();
  switch (e->n->ntyp) {
  case D_STEP:
    printf("d_step {\n");
    break;
  case ATOMIC:
    printf("atomic {\n");
    break;
  case NON_ATOMIC:
    printf(" {\n");
    break;
  }
  indent++;
  repro_seq(e->n->sl->this_sequence);
  indent--;

  doindent();
  printf(" };\n");
}

static void repro_seq(Sequence *s) {
  Element *e;
  Symbol *v;
  SeqList *h;

  for (e = s->frst; e; e = e->nxt) {
    v = has_lab(e, 0);
    if (v)
      printf("%s:\n", v->name);

    if (e->n->ntyp == UNLESS) {
      printf("/* normal */ {\n");
      repro_seq(e->n->sl->this_sequence);
      doindent();
      printf("} unless {\n");
      repro_seq(e->n->sl->nxt->this_sequence);
      doindent();
      printf("}; /* end unless */\n");
    } else if (e->sub) {
      switch (e->n->ntyp) {
      case DO:
        doindent();
        printf("do\n");
        indent++;
        break;
      case IF:
        doindent();
        printf("if\n");
        indent++;
        break;
      }

      for (h = e->sub; h; h = h->nxt) {
        indent--;
        doindent();
        indent++;
        printf("::\n");
        repro_seq(h->this_sequence);
        printf("\n");
      }

      switch (e->n->ntyp) {
      case DO:
        indent--;
        doindent();
        printf("od;\n");
        break;
      case IF:
        indent--;
        doindent();
        printf("fi;\n");
        break;
      }
    } else {
      if (e->n->ntyp == ATOMIC || e->n->ntyp == D_STEP ||
          e->n->ntyp == NON_ATOMIC)
        repro_sub(e);
      else if (e->n->ntyp != '.' && e->n->ntyp != '@' && e->n->ntyp != BREAK) {
        doindent();
        if (e->n->ntyp == C_CODE) {
          printf("c_code ");
          plunk_inline(stdout, e->n->sym->name, 1, 1);
        } else if (e->n->ntyp == 'c' && e->n->lft->ntyp == C_EXPR) {
          printf("c_expr { ");
          plunk_expr(stdout, e->n->lft->sym->name);
          printf("} ->\n");
        } else {
          comment(stdout, e->n, 0);
          printf(";\n");
        }
      }
    }
    if (e == s->last)
      break;
  }
}

void repro_proc(ProcList *p) {
  if (!p)
    return;
  if (p->nxt)
    repro_proc(p->nxt);

  if (p->det)
    printf("D"); /* deterministic */
  printf("proctype %s()", p->n->name);
  if (p->prov) {
    printf(" provided ");
    comment(stdout, p->prov, 0);
  }
  printf("\n{\n");
  repro_seq(p->s);
  printf("}\n");
}

void repro_src(void) { repro_proc(ready); }
