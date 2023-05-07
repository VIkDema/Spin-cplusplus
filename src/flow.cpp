#include "fatal/fatal.hpp"
#include "lexer/lexer.hpp"
#include "models/symbol.hpp"
#include "spin.hpp"
#include "utils/verbose/verbose.hpp"

#include "y.tab.h"
#include "main/launch_settings.hpp"
extern LaunchSettings launch_settings;
extern lexer::Lexer lexer_;
extern models::Symbol *Fname;
extern int nr_errs, lineno;
extern short has_unless, has_badelse, has_xu;
extern char CurScope[MAXSCOPESZ];

Element *Al_El = ZE;
Label *labtab = (Label *)0;
int Unique = 0, Elcnt = 0, DstepStart = -1;
int initialization_ok = 1;
short has_accept;

static Lbreak *breakstack = (Lbreak *)0;
static Lextok *innermost;
static SeqList *cur_s = (SeqList *)0;
static int break_id = 0;

static Element *if_seq(Lextok *);
static Element *new_el(Lextok *);
static Element *unless_seq(Lextok *);
static void add_el(Element *, Sequence *);
static void attach_escape(Sequence *, Sequence *);
static void mov_lab(models::Symbol *, Element *, Element *);
static void walk_atomic(Element *, Element *, int);

void open_seq(int top) {
  SeqList *t;
  Sequence *s = (Sequence *)emalloc(sizeof(Sequence));
  s->minel = -1;

  t = seqlist(s, cur_s);
  cur_s = t;
  if (top) {
    Elcnt = 1;
    initialization_ok = 1;
  } else {
    initialization_ok = 0;
  }
}

void rem_Seq(void) { DstepStart = Unique; }

void unrem_Seq(void) { DstepStart = -1; }

static int Rjumpslocal(Element *q, Element *stop) {
  Element *lb, *f;
  SeqList *h;

  /* allow no jumps out of a d_step sequence */
  for (f = q; f && f != stop; f = f->nxt) {
    if (f && f->n && f->n->ntyp == GOTO) {
      lb = get_lab(f->n, 0);
      if (!lb || lb->Seqno < DstepStart) {
        lineno = f->n->ln;
        Fname = f->n->fn;
        return 0;
      }
    }
    for (h = f->sub; h; h = h->nxt) {
      if (!Rjumpslocal(h->this_sequence->frst, h->this_sequence->last))
        return 0;
    }
  }
  return 1;
}

void cross_dsteps(Lextok *a, Lextok *b) {
  if (a && b && a->indstep != b->indstep) {
    lineno = a->ln;
    Fname = a->fn;
    if (!launch_settings.need_save_trail)
      loger::fatal("jump into d_step sequence");
  }
}

int is_skip(Lextok *n) {
  return (
      n->ntyp == PRINT || n->ntyp == PRINTM ||
      (n->ntyp == 'c' && n->lft && n->lft->ntyp == CONST && n->lft->val == 1));
}

void check_sequence(Sequence *s) {
  Element *e, *le = ZE;
  Lextok *n;
  auto &verbose_flags = utils::verbose::Flags::getInstance();
  int cnt = 0;

  for (e = s->frst; e; le = e, e = e->nxt) {
    n = e->n;
    if (is_skip(n) && !has_lab(e, 0)) {
      cnt++;
      if (cnt > 1 && n->ntyp != PRINT && n->ntyp != PRINTM) {
        if (verbose_flags.NeedToPrintVerbose()) {
          printf("spin: %s:%d, redundant skip\n", n->fn->name.c_str(), n->ln);
        }
        if (e != s->frst && e != s->last && e != s->extent) {
          e->status |= DONE; /* not unreachable */
          le->nxt = e->nxt;  /* remove it */
          e = le;
        }
      }
    } else
      cnt = 0;
  }
}

void prune_opts(Lextok *n) {
  SeqList *l;
  extern models::Symbol *context;
  extern char *claimproc;

  if (!n ||
      (context && claimproc && strcmp(context->name.c_str(), claimproc) == 0))
    return;

  for (l = n->sl; l; l = l->nxt) /* find sequences of unlabeled skips */
    check_sequence(l->this_sequence);
}

Sequence *close_seq(int nottop) {
  Sequence *s = cur_s->this_sequence;
  models::Symbol *z;

  if (nottop == 0) /* end of proctype body */
  {
    initialization_ok = 1;
  }

  if (nottop > 0 && s->frst && (z = has_lab(s->frst, 0))) {
    printf("error: (%s:%d) label %s placed incorrectly\n",
           (s->frst->n) ? s->frst->n->fn->name.c_str() : "-",
           (s->frst->n) ? s->frst->n->ln : 0, z->name.c_str());
    switch (nottop) {
    case 1:
      printf("=====> stmnt unless Label: stmnt\n");
      printf("sorry, cannot jump to the guard of an\n");
      printf("escape (it is not a unique state)\n");
      break;
    case 2:
      printf("=====> instead of  ");
      printf("\"Label: stmnt unless stmnt\"\n");
      printf("=====> always use  ");
      printf("\"Label: { stmnt unless stmnt }\"\n");
      break;
    case 3:
      printf("=====> instead of  ");
      printf("\"atomic { Label: statement ... }\"\n");
      printf("=====> always use  ");
      printf("\"Label: atomic { statement ... }\"\n");
      break;
    case 4:
      printf("=====> instead of  ");
      printf("\"d_step { Label: statement ... }\"\n");
      printf("=====> always use  ");
      printf("\"Label: d_step { statement ... }\"\n");
      break;
    case 5:
      printf("=====> instead of  ");
      printf("\"{ Label: statement ... }\"\n");
      printf("=====> always use  ");
      printf("\"Label: { statement ... }\"\n");
      break;
    case 6:
      printf("=====> instead of\n");
      printf("	do (or if)\n");
      printf("	:: ...\n");
      printf("	:: Label: statement\n");
      printf("	od (of fi)\n");
      printf("=====> use\n");
      printf("Label:	do (or if)\n");
      printf("	:: ...\n");
      printf("	:: statement\n");
      printf("	od (or fi)\n");
      break;
    case 7:
      printf("cannot happen - labels\n");
      break;
    }
    if (nottop != 6) {
      alldone(1);
    }
  }

  if (nottop == 4 && !Rjumpslocal(s->frst, s->last))
    loger::fatal("non_local jump in d_step sequence");

  cur_s = cur_s->nxt;
  s->maxel = Elcnt;
  s->extent = s->last;
  if (!s->last)
    loger::fatal("sequence must have at least one statement");
  return s;
}

Lextok *do_unless(Lextok *No, Lextok *Es) {
  SeqList *Sl;
  Lextok *Re = nn(ZN, UNLESS, ZN, ZN);

  Re->ln = No->ln;
  Re->fn = No->fn;
  has_unless++;

  if (Es->ntyp == NON_ATOMIC) {
    Sl = Es->sl;
  } else {
    open_seq(0);
    add_seq(Es);
    Sl = seqlist(close_seq(1), 0);
  }

  if (No->ntyp == NON_ATOMIC) {
    No->sl->nxt = Sl;
    Sl = No->sl;
  } else if (No->ntyp == ':' &&
             (No->lft->ntyp == NON_ATOMIC || No->lft->ntyp == ATOMIC ||
              No->lft->ntyp == D_STEP)) {
    int tok = No->lft->ntyp;

    No->lft->sl->nxt = Sl;
    Re->sl = No->lft->sl;

    open_seq(0);
    add_seq(Re);
    Re = nn(ZN, tok, ZN, ZN);
    Re->sl = seqlist(close_seq(7), 0);
    Re->ln = No->ln;
    Re->fn = No->fn;

    Re = nn(No, ':', Re, ZN); /* lift label */
    Re->ln = No->ln;
    Re->fn = No->fn;
    return Re;
  } else {
    open_seq(0);
    add_seq(No);
    Sl = seqlist(close_seq(2), Sl);
  }

  Re->sl = Sl;
  return Re;
}

SeqList *seqlist(Sequence *s, SeqList *r) {
  SeqList *t = (SeqList *)emalloc(sizeof(SeqList));

  t->this_sequence = s;
  t->nxt = r;
  return t;
}

static Element *new_el(Lextok *n) {
  Element *m;

  if (n) {
    if (n->ntyp == IF || n->ntyp == DO)
      return if_seq(n);
    if (n->ntyp == UNLESS)
      return unless_seq(n);
  }
  m = (Element *)emalloc(sizeof(Element));
  m->n = n;
  m->seqno = Elcnt++;
  m->Seqno = Unique++;
  m->Nxt = Al_El;
  Al_El = m;
  return m;
}

static int has_chanref(Lextok *n) {
  if (!n)
    return 0;

  switch (n->ntyp) {
  case 's':
  case 'r':
  case FULL:
  case NFULL:
  case EMPTY:
  case NEMPTY:
    return 1;
  default:
    break;
  }
  if (has_chanref(n->lft))
    return 1;

  return has_chanref(n->rgt);
}

void loose_ends(void) /* properly tie-up ends of sub-sequences */
{
  Element *e, *f;

  for (e = Al_El; e; e = e->Nxt) {
    if (!e->n || !e->nxt)
      continue;
    switch (e->n->ntyp) {
    case ATOMIC:
    case NON_ATOMIC:
    case D_STEP:
      f = e->nxt;
      while (f && f->n->ntyp == '.')
        f = f->nxt;
      if (0)
        printf("link %d, {%d .. %d} -> %d (ntyp=%d) was %d\n", e->seqno,
               e->n->sl->this_sequence->frst->seqno,
               e->n->sl->this_sequence->last->seqno, f ? f->seqno : -1,
               f ? f->n->ntyp : -1,
               e->n->sl->this_sequence->last->nxt
                   ? e->n->sl->this_sequence->last->nxt->seqno
                   : -1);
      if (!e->n->sl->this_sequence->last->nxt)
        e->n->sl->this_sequence->last->nxt = f;
      else {
        if (e->n->sl->this_sequence->last->nxt->n->ntyp != GOTO) {
          if (!f || e->n->sl->this_sequence->last->nxt->seqno != f->seqno)
            loger::non_fatal("unexpected: loose ends");
        } else
          e->n->sl->this_sequence->last = e->n->sl->this_sequence->last->nxt;
        /*
         * fix_dest can push a goto into the nxt position
         * in that case the goto wins and f is not needed
         * but the last fields needs adjusting
         */
      }
      break;
    }
  }
}

void popbreak(void) {
  if (!breakstack)
    loger::fatal("cannot happen, breakstack");

  breakstack = breakstack->nxt; /* pop stack */
}

static Lbreak *ob = (Lbreak *)0;

void safe_break(void) {
  ob = breakstack;
  popbreak();
}

void restore_break(void) {
  breakstack = ob;
  ob = (Lbreak *)0;
}

static Element *if_seq(Lextok *n) {
  int tok = n->ntyp;
  SeqList *s = n->sl;
  Element *e = new_el(ZN);
  Element *t = new_el(nn(ZN, '.', ZN, ZN)); /* target */
  SeqList *z, *prev_z = (SeqList *)0;
  SeqList *move_else = (SeqList *)0; /* to end of optionlist */
  int ref_chans = 0;

  for (z = s; z; z = z->nxt) {
    if (!z->this_sequence->frst)
      continue;
    if (z->this_sequence->frst->n->ntyp == ELSE) {
      if (move_else)
        loger::fatal("duplicate `else'");
      if (z->nxt) /* is not already at the end */
      {
        move_else = z;
        if (prev_z)
          prev_z->nxt = z->nxt;
        else
          s = n->sl = z->nxt;
        continue;
      }
    } else
      ref_chans |= has_chanref(z->this_sequence->frst->n);
    prev_z = z;
  }
  if (move_else) {
    move_else->nxt = (SeqList *)0;
    /* if there is no prev, then else was at the end */
    if (!prev_z)
      loger::fatal("cannot happen - if_seq");
    prev_z->nxt = move_else;
    prev_z = move_else;
  }
  if (prev_z && ref_chans && prev_z->this_sequence->frst->n->ntyp == ELSE) {
    prev_z->this_sequence->frst->n->val = 1;
    has_badelse++;
    if (has_xu) {
      loger::fatal(
          "invalid use of 'else' combined with i/o and xr/xs assertions,",
          (char *)0);
    } else {
      loger::non_fatal("dubious use of 'else' combined with i/o,");
    }
    nr_errs--;
  }

  e->n = nn(n, tok, ZN, ZN);
  e->n->sl = s; /* preserve as info only */
  e->sub = s;
  for (z = s; z; z = z->nxt)
    add_el(t, z->this_sequence); /* append target */

  if (tok == DO) {
    add_el(t, cur_s->this_sequence);  /* target upfront */
    t = new_el(nn(n, BREAK, ZN, ZN)); /* break target */
    set_lab(break_dest(), t);         /* new exit  */
    popbreak();
  }
  add_el(e, cur_s->this_sequence);
  add_el(t, cur_s->this_sequence);
  return e; /* destination node for label */
}

static void escape_el(Element *f, Sequence *e) {
  SeqList *z;

  for (z = f->esc; z; z = z->nxt)
    if (z->this_sequence == e)
      return; /* already there */

  /* cover the lower-level escapes of this state */
  for (z = f->esc; z; z = z->nxt)
    attach_escape(z->this_sequence, e);

  /* now attach escape to the state itself */

  f->esc = seqlist(e, f->esc); /* in lifo order... */
#ifdef DEBUG
  printf("attach %d (", e->frst->Seqno);
  comment(stdout, e->frst->n, 0);
  printf(")	to %d (", f->Seqno);
  comment(stdout, f->n, 0);
  printf(")\n");
#endif
  switch (f->n->ntyp) {
  case UNLESS:
    attach_escape(f->sub->this_sequence, e);
    break;
  case IF:
  case DO:
    for (z = f->sub; z; z = z->nxt)
      attach_escape(z->this_sequence, e);
    break;
  case D_STEP:
    /* attach only to the guard stmnt */
    escape_el(f->n->sl->this_sequence->frst, e);
    break;
  case ATOMIC:
  case NON_ATOMIC:
    /* attach to all stmnts */
    attach_escape(f->n->sl->this_sequence, e);
    break;
  }
}

static void attach_escape(Sequence *n, Sequence *e) {
  Element *f;

  for (f = n->frst; f; f = f->nxt) {
    escape_el(f, e);
    if (f == n->extent)
      break;
  }
}

static Element *unless_seq(Lextok *n) {
  SeqList *s = n->sl;
  Element *e = new_el(ZN);
  Element *t = new_el(nn(ZN, '.', ZN, ZN)); /* target */
  SeqList *z;

  e->n = nn(n, UNLESS, ZN, ZN);
  e->n->sl = s; /* info only */
  e->sub = s;

  /* need 2 sequences: normal execution and escape */
  if (!s || !s->nxt || s->nxt->nxt)
    loger::fatal("unexpected unless structure");

  /* append the target state to both */
  for (z = s; z; z = z->nxt)
    add_el(t, z->this_sequence);

  /* attach escapes to all states in normal sequence */
  attach_escape(s->this_sequence, s->nxt->this_sequence);

  add_el(e, cur_s->this_sequence);
  add_el(t, cur_s->this_sequence);
#ifdef DEBUG
  printf("unless element (%d,%d):\n", e->Seqno, t->Seqno);
  for (z = s; z; z = z->nxt) {
    Element *x;
    printf("\t%d,%d,%d :: ", z->this_sequence->frst->Seqno,
           z->this_sequence->extent->Seqno, z->this_sequence->last->Seqno);
    for (x = z->this_sequence->frst; x; x = x->nxt)
      printf("(%d)", x->Seqno);
    printf("\n");
  }
#endif
  return e;
}

Element *mk_skip(void) {
  Lextok *t = nn(ZN, CONST, ZN, ZN);
  t->val = 1;
  return new_el(nn(ZN, 'c', t, ZN));
}

static void add_el(Element *e, Sequence *s) {
  if (e->n->ntyp == GOTO) {
    models::Symbol *z = has_lab(e, (1 | 2 | 4));
    if (z) {
      Element *y; /* insert a skip */
      y = mk_skip();
      mov_lab(z, e, y); /* inherit label */
      add_el(y, s);
    }
  }
#ifdef DEBUG
  printf("add_el %d after %d -- ", e->Seqno, (s->last) ? s->last->Seqno : -1);
  comment(stdout, e->n, 0);
  printf("\n");
#endif
  if (!s->frst)
    s->frst = e;
  else
    s->last->nxt = e;
  s->last = e;
}

static Element *colons(Lextok *n) {
  if (!n)
    return ZE;
  if (n->ntyp == ':') {
    Element *e = colons(n->lft);
    set_lab(n->sym, e);
    return e;
  }
  innermost = n;
  return new_el(n);
}

void add_seq(Lextok *n) {
  Element *e;

  if (!n)
    return;
  innermost = n;
  e = colons(n);
  if (innermost->ntyp != IF && innermost->ntyp != DO &&
      innermost->ntyp != UNLESS)
    add_el(e, cur_s->this_sequence);
}

void set_lab(models::Symbol *s, Element *e) {
  Label *l;
  extern models::Symbol *context;
  int cur_uiid = is_inline();

  if (!s)
    return;

  for (l = labtab; l; l = l->nxt) {
    if (l->s->name == s->name && l->c == context &&
        (launch_settings.need_old_scope_rules || s->block_scope == l->s->block_scope) &&
        l->uiid == cur_uiid) {
      loger::non_fatal("label %s redeclared", s->name);
      break;
    }
  }

  if (s->name.substr(0, 6) == "accept" &&
      s->name.substr(0, 10) != "accept_all") {
    has_accept = 1;
  }

  l = (Label *)emalloc(sizeof(Label));
  l->s = s;
  l->c = context;
  l->e = e;
  l->uiid = cur_uiid;
  l->nxt = labtab;
  labtab = l;
}

static Label *get_labspec(Lextok *n) {
  models::Symbol *s = n->sym;
  Label *l, *anymatch = (Label *)0;
  /*
   * try to find a label with the same inline id (uiid)
   * but if it doesn't exist, return any other match
   * within the same scope
   */
  for (l = labtab; l; l = l->nxt) {
    if (l->s->name == s->name           /* labelname matches */
        && s->context == l->s->context) /* same scope */
    {
      /* same block scope */
      if (s->block_scope == l->s->block_scope) {
        return l; /* definite match */
      }
      /* higher block scope */
      if (s->block_scope.substr(0, l->s->block_scope.length()) ==
          l->s->block_scope) {
        anymatch = l; /* possible match */
      } else if (!anymatch) {
        anymatch = l; /* somewhere else in same context */
      }
    }
  }

  return anymatch; /* return best match */
}

Element *get_lab(Lextok *n, int md) {
  Label *l = get_labspec(n);

  if (l != (Label *)0) {
    return (l->e);
  }

  if (md) {
    lineno = n->ln;
    Fname = n->fn;
    loger::fatal("undefined label %s", n->sym->name);
  }
  return ZE;
}

models::Symbol *has_lab(Element *e, int special) {
  Label *l;

  for (l = labtab; l; l = l->nxt) {
    if (e != l->e)
      continue;
    if (special == 0 ||
        ((special & 1) && l->s->name.substr(0, 6) == "accept") ||
        ((special & 2) && l->s->name.substr(0, 3) == "end") ||
        ((special & 4) && l->s->name.substr(0, 8) == "progress")) {
      return (l->s);
    }
  }
  return ZS;
}

static void mov_lab(models::Symbol *z, Element *e, Element *y) {
  Label *l;

  for (l = labtab; l; l = l->nxt)
    if (e == l->e) {
      l->e = y;
      return;
    }
  if (e->n) {
    lineno = e->n->ln;
    Fname = e->n->fn;
  }
  loger::fatal("cannot happen - mov_lab %s", z->name);
}

void fix_dest(models::Symbol *c,
              models::Symbol *a) /* c:label name, a:proctype name */
{
  Label *l;
  extern models::Symbol *context;

  for (l = labtab; l; l = l->nxt) {
    if (c->name == l->s->name && a->name == l->c->name) /* ? */
      break;
  }
  if (!l) {
    printf("spin: label '%s' (proctype %s)\n", c->name.c_str(),
           a->name.c_str());
    loger::non_fatal("unknown label '%s'", c->name);
    if (context == a)
      printf("spin: cannot remote ref a label inside the same proctype\n");
    return;
  }
  if (!l->e || !l->e->n)
    loger::fatal("fix_dest error (%s)", c->name);
  if (l->e->n->ntyp == GOTO) {
    Element *y = (Element *)emalloc(sizeof(Element));
    int keep_ln = l->e->n->ln;
    models::Symbol *keep_fn = l->e->n->fn;

    /* insert skip - or target is optimized away */
    y->n = l->e->n;           /* copy of the goto   */
    y->seqno = find_maxel(a); /* unique seqno within proc */
    y->nxt = l->e->nxt;
    y->Seqno = Unique++;
    y->Nxt = Al_El;
    Al_El = y;

    /* turn the original element+seqno into a skip */
    l->e->n = nn(ZN, 'c', nn(ZN, CONST, ZN, ZN), ZN);
    l->e->n->ln = l->e->n->lft->ln = keep_ln;
    l->e->n->fn = l->e->n->lft->fn = keep_fn;
    l->e->n->lft->val = 1;
    l->e->nxt = y; /* append the goto  */
  }
  l->e->status |= CHECK2; /* treat as if global */
  if (l->e->status & (ATOM | L_ATOM | D_ATOM)) {
    printf("spin: %s:%d, warning, reference to label ", Fname->name.c_str(),
           lineno);
    printf("from inside atomic or d_step (%s)\n", c->name.c_str());
  }
}

int find_lab(models::Symbol *s, models::Symbol *c, int markit) {
  Label *l, *pm = (Label *)0, *apm = (Label *)0;
  int ln;

  /* generally called for remote references in never claims */
  for (l = labtab; l; l = l->nxt) {
    if (s->name == l->s->name && c->name == l->c->name) {
      ln = l->s->block_scope.length();
      if (0) {
        printf("want '%s' in context '%s', scope ref '%s' - label '%s'\n",
               s->name.c_str(), c->name.c_str(), s->block_scope.c_str(),
               l->s->block_scope.c_str());
      }
      /* same or higher block scope */
      if (s->block_scope == l->s->block_scope) {
        pm = l; /* definite match */
        break;
      }
      if (s->block_scope.substr(0, ln) == l->s->block_scope) {
        pm = l; /* possible match */
      } else {
        apm = l; /* remote */
      }
    }
  }

  if (pm) {
    pm->visible |= markit;
    return pm->e->seqno;
  }
  if (apm) {
    apm->visible |= markit;
    return apm->e->seqno;
  } /* else printf("Not Found\n"); */
  return 0;
}

void pushbreak(void) {
  Lbreak *r = (Lbreak *)emalloc(sizeof(Lbreak));
  models::Symbol *l;
  char buf[64];

  sprintf(buf, ":b%d", break_id++);
  l = lookup(buf);
  r->l = l;
  r->nxt = breakstack;
  breakstack = r;
}

models::Symbol *break_dest(void) {
  if (!breakstack)
    loger::fatal("misplaced break statement");
  return breakstack->l;
}

void make_atomic(Sequence *s, int added) {
  Element *f;

  walk_atomic(s->frst, s->last, added);

  f = s->last;
  switch (f->n->ntyp) { /* is last step basic stmnt or sequence ? */
  case NON_ATOMIC:
  case ATOMIC:
    /* redo and search for the last step of that sequence */
    make_atomic(f->n->sl->this_sequence, added);
    break;

  case UNLESS:
    /* escapes are folded into main sequence */
    make_atomic(f->sub->this_sequence, added);
    break;

  default:
    f->status &= ~ATOM;
    f->status |= L_ATOM;
    break;
  }
}

int match_struct(models::Symbol *s, models::Symbol *t) {
  if (!t || !t->init_value || !t->init_value->rgt || !t->init_value->rgt->sym ||
      t->init_value->rgt->rgt) {
    char *t_name = "--";
    loger::fatal("chan %s in for should have only one field (a typedef)",
                 t_name);
  }
  /* we already know that s is a STRUCT */
  if (0) {
    printf("index type %s %p ==\n", s->struct_name->name.c_str(),
           (void *)s->struct_name);
    printf("chan type  %s %p --\n\n", t->init_value->rgt->sym->name.c_str(),
           (void *)t->init_value->rgt->sym);
  }

  return (s->struct_name == t->init_value->rgt->sym);
}

void valid_name(Lextok *a3, Lextok *a5, Lextok *a8, char *tp) {
  if (a3->ntyp != NAME) {
    loger::fatal("%s ( .name : from .. to ) { ... }", tp);
  }
  if (a3->sym->type == CHAN || a3->sym->type == STRUCT ||
      a3->sym->is_array != 0) {
    loger::fatal("bad index in for-construct %s", a3->sym->name.c_str());
  }
  if (a5->ntyp == CONST && a8->ntyp == CONST && a5->val > a8->val) {
    loger::non_fatal("start value for %s exceeds end-value",
                     a3->sym->name.c_str());
  }
}

void for_setup(Lextok *a3, Lextok *a5, Lextok *a8) { /* for ( a3 : a5 .. a8 ) */

  valid_name(a3, a5, a8, "for");
  /* a5->ntyp = a8->ntyp = CONST; */
  add_seq(nn(a3, ASGN, a3, a5)); /* start value */
  open_seq(0);
  add_seq(nn(ZN, 'c', nn(a3, LE, a3, a8), ZN)); /* condition */
}

Lextok *for_index(Lextok *a3, Lextok *a5) {
  Lextok *z0, *z1, *z2, *z3;
  models::Symbol *tmp_cnt;
  char tmp_nm[MAXSCOPESZ + 16];
  /* for ( a3 in a5 ) { ... } */

  if (a3->ntyp != NAME) {
    loger::fatal("for ( .name in name ) { ... }");
  }

  if (a5->ntyp != NAME) {
    loger::fatal("for ( %s in .name ) { ... }", a3->sym->name.c_str());
  }

  if (a3->sym->type == STRUCT) {
    if (a5->sym->type != CHAN) {
      loger::fatal("for ( %s in .channel_name ) { ... }",
                   a3->sym->name.c_str());
    }
    z0 = a5->sym->init_value;
    if (!z0 || z0->val <= 0 || z0->rgt->ntyp != STRUCT ||
        z0->rgt->rgt != NULL) {
      loger::fatal("bad channel type %s in for", a5->sym->name.c_str());
    }

    if (!match_struct(a3->sym, a5->sym)) {
      loger::fatal("type of %s does not match chan", a3->sym->name.c_str());
    }

    z1 = nn(ZN, CONST, ZN, ZN);
    z1->val = 0;
    z2 = nn(a5, LEN, a5, ZN);

    sprintf(tmp_nm, "_f0r_t3mp%s", CurScope); /* make sure it's unique */
    tmp_cnt = lookup(tmp_nm);
    if (z0->val > 255) /* check nr of slots, i.e. max length */
    {
      tmp_cnt->type = models::SymbolType::kShort; /* should be rare */
    } else {
      tmp_cnt->type = models::SymbolType::kByte;
    }
    z3 = nn(ZN, NAME, ZN, ZN);
    z3->sym = tmp_cnt;

    add_seq(nn(z3, ASGN, z3, z1)); /* start value 0 */

    open_seq(0);

    add_seq(nn(ZN, 'c', nn(z3, LT, z3, z2), ZN)); /* condition */

    /* retrieve  message from the right slot -- for now: rotate contents */
    lexer_.SetInFor(0);
    add_seq(nn(a5, 'r', a5, expand(a3, 1))); /* receive */
    add_seq(nn(a5, 's', a5, expand(a3, 1))); /* put back in to rotate */
    lexer_.SetInFor(1);
    return z3;
  } else {
    Lextok *leaf = a5;
    if (leaf->sym->type == STRUCT) // find leaf node, which should be an array
    {
      while (leaf->rgt && leaf->rgt->ntyp == '.') {
        leaf = leaf->rgt;
      }
      leaf = leaf->lft;
    }
    if (!leaf) {
      loger::fatal("unexpected type of for-loop");
    }
    if (leaf->sym->is_array == 0 || leaf->sym->value_type <= 0) {
      loger::fatal("bad arrayname %s", leaf->sym->name.c_str());
    }
    z1 = nn(ZN, CONST, ZN, ZN);
    z1->val = 0;
    z2 = nn(ZN, CONST, ZN, ZN);
    z2->val = leaf->sym->value_type - 1;
    for_setup(a3, z1, z2);
    return a3;
  }
}

Lextok *for_body(Lextok *a3, int with_else) {
  Lextok *t1, *t2, *t0, *rv;

  rv = nn(ZN, CONST, ZN, ZN);
  rv->val = 1;
  rv = nn(ZN, '+', a3, rv);
  rv = nn(a3, ASGN, a3, rv);
  add_seq(rv); /* initial increment */

  /* completed loop body, main sequence */
  t1 = nn(ZN, 0, ZN, ZN);
  t1->sq = close_seq(8);

  open_seq(0); /* add else -> break sequence */
  if (with_else) {
    add_seq(nn(ZN, ELSE, ZN, ZN));
  }
  t2 = nn(ZN, GOTO, ZN, ZN);
  t2->sym = break_dest();
  add_seq(t2);
  t2 = nn(ZN, 0, ZN, ZN);
  t2->sq = close_seq(9);

  t0 = nn(ZN, 0, ZN, ZN);
  t0->sl = seqlist(t2->sq, seqlist(t1->sq, 0));

  rv = nn(ZN, DO, ZN, ZN);
  rv->sl = t0->sl;

  return rv;
}

Lextok *sel_index(Lextok *a3, Lextok *a5,
                  Lextok *a7) { /* select ( a3 : a5 .. a7 ) */

  valid_name(a3, a5, a7, "select");
  /* a5->ntyp = a7->ntyp = CONST; */

  add_seq(nn(a3, ASGN, a3, a5)); /* start value */
  open_seq(0);
  add_seq(nn(ZN, 'c', nn(a3, LT, a3, a7), ZN)); /* condition */

  pushbreak();            /* new 6.2.1 */
  return for_body(a3, 0); /* no else, just a non-deterministic break */
}

static void walk_atomic(Element *a, Element *b, int added) {
  Element *f;
  models::Symbol *ofn;
  int oln;
  SeqList *h;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  ofn = Fname;
  oln = lineno;
  for (f = a;; f = f->nxt) {
    f->status |= (ATOM | added);
    switch (f->n->ntyp) {
    case ATOMIC:
      if (verbose_flags.NeedToPrintVerbose()) {
        printf("spin: %s:%d, warning, atomic inside %s (ignored)\n",
               f->n->fn->name.c_str(), f->n->ln, (added) ? "d_step" : "atomic");
      }
      goto mknonat;
    case D_STEP:
      if (!verbose_flags.NeedToPrintVerbose()) {
        if (added)
          goto mknonat;
        break;
      }
      printf("spin: %s:%d, warning, d_step inside ", f->n->fn->name.c_str(),
             f->n->ln);
      if (added) {
        printf("d_step (ignored)\n");
        goto mknonat;
      }
      printf("atomic\n");
      break;
    case NON_ATOMIC:
    mknonat:
      f->n->ntyp = NON_ATOMIC; /* can jump here */
      h = f->n->sl;
      walk_atomic(h->this_sequence->frst, h->this_sequence->last, added);
      break;
    case UNLESS:
      if (added) {
        printf("spin: error, %s:%d, unless in d_step (ignored)\n",
               f->n->fn->name.c_str(), f->n->ln);
      }
    }
    for (h = f->sub; h; h = h->nxt)
      walk_atomic(h->this_sequence->frst, h->this_sequence->last, added);
    if (f == b)
      break;
  }
  Fname = ofn;
  lineno = oln;
}

void dumplabels(void) {
  Label *l;

  for (l = labtab; l; l = l->nxt)
    if (l->c != 0 && l->s->name[0] != ':') {
      printf("label	%s	%d	", l->s->name.c_str(), l->e->seqno);
      if (l->uiid == 0)
        printf("<%s>", l->c->name.c_str());
      else
        printf("<%s i%d>", l->c->name.c_str(), l->uiid);
      if (!launch_settings.need_old_scope_rules) {
        printf("\t{scope %s}", l->s->block_scope.c_str());
      }
      printf("\n");
    }
}
