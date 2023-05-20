#include "flow.hpp"

#include "../fatal/fatal.hpp"
#include "../lexer/inline_processor.hpp"
#include "../lexer/lexer.hpp"
#include "../lexer/line_number.hpp"
#include "../lexer/scope.hpp"
#include "../main/launch_settings.hpp"
#include "../main/main_processor.hpp"
#include "../models/lextok.hpp"
#include "../models/symbol.hpp"
#include "../spin.hpp"
#include "../structs/structs.hpp"
#include "../utils/verbose/verbose.hpp"
#include "sched.hpp"
#include "y.tab.h"

extern LaunchSettings launch_settings;
extern lexer::Lexer lexer_;
extern models::Symbol *Fname;
extern int nr_errs;
extern short has_unless, has_badelse, has_xu;
  extern char *claimproc;

models::Element *Al_El = ZE;
models::Label *labtab = nullptr;
int Unique = 0, Elcnt = 0, DstepStart = -1;
int initialization_ok = 1;
short has_accept;

static models::Lbreak *breakstack = nullptr;
static models::Lextok *innermost;
static models::SeqList *cur_s = nullptr;
static int break_id = 0;

static models::Lbreak *ob = nullptr;
 
namespace flow {
namespace {

models::Element *unless_seq(models::Lextok *);
models::Element *if_seq(models::Lextok *);
void attach_escape(models::Sequence *, models::Sequence *);
models::Element *new_el(models::Lextok *);

int has_chanref(models::Lextok *n) {
  if (!n)
    return 0;

  switch (n->node_type) {
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
  if (has_chanref(n->left))
    return 1;

  return has_chanref(n->right);
}

int is_skip(models::Lextok *n) {
  return (n->node_type == PRINT || n->node_type == PRINTM ||
          (n->node_type == 'c' && n->left && n->left->node_type == CONST &&
           n->left->value == 1));
}

void popbreak() {
  if (!breakstack)
    loger::fatal("cannot happen, breakstack");

  breakstack = breakstack->next; /* pop stack */
}

void walk_atomic(models::Element *a, models::Element *b, int added) {
  models::Element *f;
  models::Symbol *ofn;
  int oln;
  models::SeqList *h;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  ofn = Fname;
  oln = file::LineNumber::Get();
  for (f = a;; f = f->next) {
    f->status |= (ATOM | added);
    switch (f->n->node_type) {
    case ATOMIC:
      if (verbose_flags.NeedToPrintVerbose()) {
        printf("spin++: %s:%d, warning, atomic inside %s (ignored)\n",
               f->n->file_name->name.c_str(), f->n->line_number,
               (added) ? "d_step" : "atomic");
      }
      goto mknonat;
    case D_STEP:
      if (!verbose_flags.NeedToPrintVerbose()) {
        if (added)
          goto mknonat;
        break;
      }
      printf("spin++: %s:%d, warning, d_step inside ",
             f->n->file_name->name.c_str(), f->n->line_number);
      if (added) {
        printf("d_step (ignored)\n");
        goto mknonat;
      }
      printf("atomic\n");
      break;
    case NON_ATOMIC:
    mknonat:
      f->n->node_type = NON_ATOMIC; /* can jump here */
      h = f->n->seq_list;
      walk_atomic(h->this_sequence->frst, h->this_sequence->last, added);
      break;
    case UNLESS:
      if (added) {
        printf("spin++: error, %s:%d, unless in d_step (ignored)\n",
               f->n->file_name->name.c_str(), f->n->line_number);
      }
    }
    for (h = f->sub; h; h = h->next)
      walk_atomic(h->this_sequence->frst, h->this_sequence->last, added);
    if (f == b)
      break;
  }
  Fname = ofn;
  file::LineNumber::Set(oln);
}

void valid_name(models::Lextok *a3, models::Lextok *a5, models::Lextok *a8,
                char *tp) {
  if (a3->node_type != NAME) {
    loger::fatal("%s ( .name : from .. to ) { ... }", tp);
  }
  if (a3->symbol->type == CHAN || a3->symbol->type == STRUCT ||
      a3->symbol->is_array != 0) {
    loger::fatal("bad index in for-construct %s", a3->symbol->name.c_str());
  }
  if (a5->node_type == CONST && a8->node_type == CONST &&
      a5->value > a8->value) {
    loger::non_fatal("start value for %s exceeds end-value",
                     a3->symbol->name.c_str());
  }
}

int match_struct(models::Symbol *s, models::Symbol *t) {
  if (!t || !t->init_value || !t->init_value->right ||
      !t->init_value->right->symbol || t->init_value->right->right) {
    char *t_name = "--";
    loger::fatal("chan %s in for should have only one field (a typedef)",
                 t_name);
  }
  /* we already know that s is a STRUCT */
  if (0) {
    printf("index type %s %p ==\n", s->struct_name->name.c_str(),
           (void *)s->struct_name);
    printf("chan type  %s %p --\n\n",
           t->init_value->right->symbol->name.c_str(),
           (void *)t->init_value->right->symbol);
  }

  return (s->struct_name == t->init_value->right->symbol);
}

void mov_lab(models::Symbol *z, models::Element *e, models::Element *y) {
  models::Label *l;

  for (l = labtab; l; l = l->next)
    if (e == l->e) {
      l->e = y;
      return;
    }
  if (e->n) {
    file::LineNumber::Set(e->n->line_number);
    Fname = e->n->file_name;
  }
  loger::fatal("cannot happen - mov_lab %s", z->name);
}

models::Label *get_labspec(models::Lextok *n) {
  models::Symbol *s = n->symbol;
  models::Label *l, *anymatch = nullptr;
  /*
   * try to find a label with the same inline id (uiid)
   * but if it doesn't exist, return any other match
   * within the same scope
   */
  for (l = labtab; l; l = l->next) {
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
models::Element *mk_skip() {
  models::Lextok *t = models::Lextok::nn(ZN, CONST, ZN, ZN);
  t->value = 1;
  return new_el(models::Lextok::nn(ZN, 'c', t, ZN));
}

models::Element *new_el(models::Lextok *n) {
  models::Element *m;

  if (n) {
    if (n->node_type == IF || n->node_type == DO)
      return if_seq(n);
    if (n->node_type == UNLESS)
      return unless_seq(n);
  }
  m = (models::Element *)emalloc(sizeof(models::Element));
  m->n = n;
  m->seqno = Elcnt++;
  m->Seqno = Unique++;
  m->Nxt = Al_El;
  Al_El = m;
  return m;
}
void add_el(models::Element *e, models::Sequence *s) {
  if (e->n->node_type == GOTO) {
    models::Symbol *z = HasLabel(e, (1 | 2 | 4));
    if (z) {
      models::Element *y; /* insert a skip */
      y = mk_skip();
      mov_lab(z, e, y); /* inherit label */
      add_el(y, s);
    }
  }
  if (!s->frst)
    s->frst = e;
  else
    s->last->next = e;
  s->last = e;
}

models::Element *colons(models::Lextok *n) {
  if (!n)
    return ZE;
  if (n->node_type == ':') {
    models::Element *e = colons(n->left);
    SetLabel(n->symbol, e);
    return e;
  }
  innermost = n;
  return new_el(n);
}

models::Element *if_seq(models::Lextok *n) {
  int tok = n->node_type;
  models::SeqList *s = n->seq_list;
  models::Element *e = new_el(ZN);
  models::Element *t = new_el(models::Lextok::nn(ZN, '.', ZN, ZN)); /* target */
  models::SeqList *z, *prev_z = (models::SeqList *)0;
  models::SeqList *move_else = (models::SeqList *)0; /* to end of optionlist */
  int ref_chans = 0;

  for (z = s; z; z = z->next) {
    if (!z->this_sequence->frst)
      continue;
    if (z->this_sequence->frst->n->node_type == ELSE) {
      if (move_else)
        loger::fatal("duplicate `else'");
      if (z->next) /* is not already at the end */
      {
        move_else = z;
        if (prev_z)
          prev_z->next = z->next;
        else
          s = n->seq_list = z->next;
        continue;
      }
    } else
      ref_chans |= has_chanref(z->this_sequence->frst->n);
    prev_z = z;
  }
  if (move_else) {
    move_else->next = (models::SeqList *)0;
    /* if there is no prev, then else was at the end */
    if (!prev_z)
      loger::fatal("cannot happen - if_seq");
    prev_z->next = move_else;
    prev_z = move_else;
  }
  if (prev_z && ref_chans &&
      prev_z->this_sequence->frst->n->node_type == ELSE) {
    prev_z->this_sequence->frst->n->value = 1;
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

  e->n = models::Lextok::nn(n, tok, ZN, ZN);
  e->n->seq_list = s; /* preserve as info only */
  e->sub = s;
  for (z = s; z; z = z->next)
    add_el(t, z->this_sequence); /* append target */

  if (tok == DO) {
    add_el(t, cur_s->this_sequence);                  /* target upfront */
    t = new_el(models::Lextok::nn(n, BREAK, ZN, ZN)); /* break target */
    SetLabel(GetBreakDestination(), t);                /* new exit  */
    popbreak();
  }
  add_el(e, cur_s->this_sequence);
  add_el(t, cur_s->this_sequence);
  return e; /* destination node for label */
}

void escape_el(models::Element *f, models::Sequence *e) {
  models::SeqList *z;

  for (z = f->esc; z; z = z->next)
    if (z->this_sequence == e)
      return; /* already there */

  /* cover the lower-level escapes of this state */
  for (z = f->esc; z; z = z->next)
    attach_escape(z->this_sequence, e);

  /* now attach escape to the state itself */

  f->esc = f->esc->Add(e); /* in lifo order... */

  switch (f->n->node_type) {
  case UNLESS:
    attach_escape(f->sub->this_sequence, e);
    break;
  case IF:
  case DO:
    for (z = f->sub; z; z = z->next)
      attach_escape(z->this_sequence, e);
    break;
  case D_STEP:
    /* attach only to the guard stmnt */
    escape_el(f->n->seq_list->this_sequence->frst, e);
    break;
  case ATOMIC:
  case NON_ATOMIC:
    /* attach to all stmnts */
    attach_escape(f->n->seq_list->this_sequence, e);
    break;
  }
}

void attach_escape(models::Sequence *n, models::Sequence *e) {
  models::Element *f;

  for (f = n->frst; f; f = f->next) {
    escape_el(f, e);
    if (f == n->extent)
      break;
  }
}

models::Element *unless_seq(models::Lextok *n) {
  models::SeqList *s = n->seq_list;
  models::Element *e = new_el(ZN);
  models::Element *t = new_el(models::Lextok::nn(ZN, '.', ZN, ZN)); /* target */
  models::SeqList *z;

  e->n = models::Lextok::nn(n, UNLESS, ZN, ZN);
  e->n->seq_list = s; /* info only */
  e->sub = s;

  /* need 2 sequences: normal execution and escape */
  if (!s || !s->next || s->next->next)
    loger::fatal("unexpected unless structure");

  /* append the target state to both */
  for (z = s; z; z = z->next)
    add_el(t, z->this_sequence);

  /* attach escapes to all states in normal sequence */
  attach_escape(s->this_sequence, s->next->this_sequence);

  add_el(e, cur_s->this_sequence);
  add_el(t, cur_s->this_sequence);
  return e;
}


void check_sequence(models::Sequence *s) {
  models::Element *e, *le = ZE;
  models::Lextok *n;
  auto &verbose_flags = utils::verbose::Flags::getInstance();
  int cnt = 0;

  for (e = s->frst; e; le = e, e = e->next) {
    n = e->n;
    if (is_skip(n) && !HasLabel(e, 0)) {
      cnt++;
      if (cnt > 1 && n->node_type != PRINT && n->node_type != PRINTM) {
        if (verbose_flags.NeedToPrintVerbose()) {
          printf("spin++: %s:%d, redundant skip\n", n->file_name->name.c_str(),
                 n->line_number);
        }
        if (e != s->frst && e != s->last && e != s->extent) {
          e->status |= DONE;  /* not unreachable */
          le->next = e->next; /* remove it */
          e = le;
        }
      }
    } else
      cnt = 0;
  }
}

int Rjumpslocal(models::Element *q, models::Element *stop) {
  models::Element *lb, *f;
  models::SeqList *h;

  /* allow no jumps out of a d_step sequence */
  for (f = q; f && f != stop; f = f->next) {
    if (f && f->n && f->n->node_type == GOTO) {
      lb = GetLabel(f->n, 0);
      if (!lb || lb->Seqno < DstepStart) {
        file::LineNumber::Set(f->n->line_number);
        Fname = f->n->file_name;
        return 0;
      }
    }
    for (h = f->sub; h; h = h->next) {
      if (!Rjumpslocal(h->this_sequence->frst, h->this_sequence->last))
        return 0;
    }
  }
  return 1;
}

} // namespace

void SetLabel(models::Symbol *s, models::Element *e) {
  models::Label *l;
  models::Symbol *context = models::Symbol::GetContext();
  int cur_uiid = lexer::InlineProcessor::GetCurrInlineUuid();

  if (!s)
    return;

  for (l = labtab; l; l = l->next) {
    if (l->s->name == s->name && l->c == context &&
        (launch_settings.need_old_scope_rules ||
         s->block_scope == l->s->block_scope) &&
        l->opt_inline_id == cur_uiid) {
      loger::non_fatal("label %s redeclared", s->name);
      break;
    }
  }

  if (s->name.substr(0, 6) == "accept" &&
      s->name.substr(0, 10) != "accept_all") {
    has_accept = 1;
  }

  l = (models::Label *)emalloc(sizeof(models::Label));
  l->s = s;
  l->c = context;
  l->e = e;
  l->opt_inline_id = cur_uiid;
  l->next = labtab;
  labtab = l;
}


models::Lextok *DoUnless(models::Lextok *No, models::Lextok *Es) {
  models::SeqList *Sl;
  models::Lextok *Re = models::Lextok::nn(ZN, UNLESS, ZN, ZN);

  Re->line_number = No->line_number;
  Re->file_name = No->file_name;
  has_unless++;

  if (Es->node_type == NON_ATOMIC) {
    Sl = Es->seq_list;
  } else {
    OpenSequence(0);
    AddSequence(Es);
    Sl = models::SeqList::Build(CloseSequence(1));
  }

  if (No->node_type == NON_ATOMIC) {
    No->seq_list->next = Sl;
    Sl = No->seq_list;
  } else if (No->node_type == ':' &&
             (No->left->node_type == NON_ATOMIC ||
              No->left->node_type == ATOMIC || No->left->node_type == D_STEP)) {
    int tok = No->left->node_type;

    No->left->seq_list->next = Sl;
    Re->seq_list = No->left->seq_list;

    OpenSequence(0);
    AddSequence(Re);
    Re = models::Lextok::nn(ZN, tok, ZN, ZN);
    Re->seq_list = models::SeqList::Build(CloseSequence(7));
    Re->line_number = No->line_number;
    Re->file_name = No->file_name;

    Re = models::Lextok::nn(No, ':', Re, ZN); /* lift label */
    Re->line_number = No->line_number;
    Re->file_name = No->file_name;
    return Re;
  } else {
    OpenSequence(0);
    AddSequence(No);
    Sl = models::SeqList::Build(CloseSequence(2));
  }

  Re->seq_list = Sl;
  return Re;
}

void DumpLabels(void) {
  models::Label *l;

  for (l = labtab; l; l = l->next)
    if (l->c != 0 && l->s->name[0] != ':') {
      printf("label	%s	%d	", l->s->name.c_str(), l->e->seqno);
      if (l->opt_inline_id == 0)
        printf("<%s>", l->c->name.c_str());
      else
        printf("<%s i%d>", l->c->name.c_str(), l->opt_inline_id);
      if (!launch_settings.need_old_scope_rules) {
        printf("\t{scope %s}", l->s->block_scope.c_str());
      }
      printf("\n");
    }
}

models::Lextok *SelectIndex(models::Lextok *a3, models::Lextok *a5,
                            models::Lextok *a7) { /* select ( a3 : a5 .. a7 ) */

  valid_name(a3, a5, a7, "select");
  /* a5->node_type = a7->node_type = CONST; */

  AddSequence(models::Lextok::nn(a3, ASGN, a3, a5)); /* start value */
  OpenSequence(0);
  AddSequence(models::Lextok::nn(ZN, 'c', models::Lextok::nn(a3, LT, a3, a7),
                                 ZN)); /* condition */

  AddBreakDestination();      /* new 6.2.1 */
  return BuildForBody(a3, 0); /* no else, just a non-deterministic break */
}

models::Lextok *BuildForIndex(models::Lextok *a3, models::Lextok *a5) {
  models::Lextok *z0, *z1, *z2, *z3;
  models::Symbol *tmp_cnt;
  char tmp_nm[MAXSCOPESZ + 16];
  /* for ( a3 in a5 ) { ... } */

  if (a3->node_type != NAME) {
    loger::fatal("for ( .name in name ) { ... }");
  }

  if (a5->node_type != NAME) {
    loger::fatal("for ( %s in .name ) { ... }", a3->symbol->name.c_str());
  }

  if (a3->symbol->type == STRUCT) {
    if (a5->symbol->type != CHAN) {
      loger::fatal("for ( %s in .channel_name ) { ... }",
                   a3->symbol->name.c_str());
    }
    z0 = a5->symbol->init_value;
    if (!z0 || z0->value <= 0 || z0->right->node_type != STRUCT ||
        z0->right->right != NULL) {
      loger::fatal("bad channel type %s in for", a5->symbol->name.c_str());
    }

    if (!match_struct(a3->symbol, a5->symbol)) {
      loger::fatal("type of %s does not match chan", a3->symbol->name.c_str());
    }

    z1 = models::Lextok::nn(ZN, CONST, ZN, ZN);
    z1->value = 0;
    z2 = models::Lextok::nn(a5, LEN, a5, ZN);

    sprintf(tmp_nm, "_f0r_t3mp%s",
            lexer::ScopeProcessor::GetCurrScope()
                .c_str()); /* make sure it's unique */
    tmp_cnt = models::Symbol::BuildOrFind(tmp_nm);
    if (z0->value > 255) /* check nr of slots, i.e. max length */
    {
      tmp_cnt->type = models::SymbolType::kShort; /* should be rare */
    } else {
      tmp_cnt->type = models::SymbolType::kByte;
    }
    z3 = models::Lextok::nn(ZN, NAME, ZN, ZN);
    z3->symbol = tmp_cnt;

    AddSequence(models::Lextok::nn(z3, ASGN, z3, z1)); /* start value 0 */

    OpenSequence(0);

    AddSequence(models::Lextok::nn(ZN, 'c', models::Lextok::nn(z3, LT, z3, z2),
                                   ZN)); /* condition */

    /* retrieve  message from the right slot -- for now: rotate contents */
    lexer_.SetInFor(0);
    AddSequence(models::Lextok::nn(a5, 'r', a5,
                                   structs::ExpandLextok(a3, 1))); /* receive */
    AddSequence(models::Lextok::nn(
        a5, 's', a5, structs::ExpandLextok(a3, 1))); /* put back in to rotate */
    lexer_.SetInFor(1);
    return z3;
  } else {
    models::Lextok *leaf = a5;
    if (leaf->symbol->type ==
        STRUCT) // find leaf node, which should be an array
    {
      while (leaf->right && leaf->right->node_type == '.') {
        leaf = leaf->right;
      }
      leaf = leaf->left;
    }
    if (!leaf) {
      loger::fatal("unexpected type of for-loop");
    }
    if (leaf->symbol->is_array == 0 || leaf->symbol->value_type <= 0) {
      loger::fatal("bad arrayname %s", leaf->symbol->name.c_str());
    }
    z1 = models::Lextok::nn(ZN, CONST, ZN, ZN);
    z1->value = 0;
    z2 = models::Lextok::nn(ZN, CONST, ZN, ZN);
    z2->value = leaf->symbol->value_type - 1;
    SetupForLoop(a3, z1, z2);
    return a3;
  }
}

models::Lextok *BuildForBody(models::Lextok *a3, int with_else) {
  models::Lextok *t1, *t2, *t0, *rv;

  rv = models::Lextok::nn(ZN, CONST, ZN, ZN);
  rv->value = 1;
  rv = models::Lextok::nn(ZN, '+', a3, rv);
  rv = models::Lextok::nn(a3, ASGN, a3, rv);
  AddSequence(rv); /* initial increment */

  /* completed loop body, main sequence */
  t1 = models::Lextok::nn(ZN, 0, ZN, ZN);
  t1->sequence = CloseSequence(8);

  OpenSequence(0); /* add else -> break sequence */
  if (with_else) {
    AddSequence(models::Lextok::nn(ZN, ELSE, ZN, ZN));
  }
  t2 = models::Lextok::nn(ZN, GOTO, ZN, ZN);
  t2->symbol = GetBreakDestination();
  AddSequence(t2);
  t2 = models::Lextok::nn(ZN, 0, ZN, ZN);
  t2->sequence = CloseSequence(9);

  t0 = models::Lextok::nn(ZN, 0, ZN, ZN);
  t0->seq_list = models::SeqList::Build(t1->sequence)->Add(t2->sequence);

  rv = models::Lextok::nn(ZN, DO, ZN, ZN);
  rv->seq_list = t0->seq_list;

  return rv;
}

void SetupForLoop(models::Lextok *a3, models::Lextok *a5,
                  models::Lextok *a8) { /* for ( a3 : a5 .. a8 ) */

  valid_name(a3, a5, a8, "for");
  AddSequence(models::Lextok::nn(a3, ASGN, a3, a5)); /* start value */
  OpenSequence(0);
  AddSequence(models::Lextok::nn(ZN, 'c', models::Lextok::nn(a3, LE, a3, a8),
                                 ZN)); /* condition */
}

void MakeAtomic(models::Sequence *s, int added) {
  models::Element *f;

  walk_atomic(s->frst, s->last, added);

  f = s->last;
  switch (f->n->node_type) { /* is last step basic stmnt or sequence ? */
  case NON_ATOMIC:
  case ATOMIC:
    /* redo and search for the last step of that sequence */
    MakeAtomic(f->n->seq_list->this_sequence, added);
    break;

  case UNLESS:
    /* escapes are folded into main sequence */
    MakeAtomic(f->sub->this_sequence, added);
    break;

  default:
    f->status &= ~ATOM;
    f->status |= L_ATOM;
    break;
  }
}

models::Symbol *GetBreakDestination() {
  if (!breakstack)
    loger::fatal("misplaced break statement");
  return breakstack->l;
}

void AddBreakDestination(void) {
  models::Lbreak *r = (models::Lbreak *)emalloc(sizeof(models::Lbreak));
  models::Symbol *l;
  char buf[64];

  sprintf(buf, ":b%d", break_id++);
  l = models::Symbol::BuildOrFind(buf);
  r->l = l;
  r->next = breakstack;
  breakstack = r;
}

int FindLabel(models::Symbol *s, models::Symbol *c, int markit) {
  models::Label *l, *pm = nullptr, *apm = nullptr;
  int ln;

  /* generally called for remote references in never claims */
  for (l = labtab; l; l = l->next) {
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

void FixLabelRef(models::Symbol *c,
                 models::Symbol *a) /* c:label name, a:proctype name */
{
  models::Label *l;
  models::Symbol *context = models::Symbol::GetContext();

  for (l = labtab; l; l = l->next) {
    if (c->name == l->s->name && a->name == l->c->name) /* ? */
      break;
  }
  if (!l) {
    printf("spin++: label '%s' (proctype %s)\n", c->name.c_str(),
           a->name.c_str());
    loger::non_fatal("unknown label '%s'", c->name);
    if (context == a)
      printf("spin++: cannot remote ref a label inside the same proctype\n");
    return;
  }
  if (!l->e || !l->e->n)
    loger::fatal("fix_dest error (%s)", c->name);
  if (l->e->n->node_type == GOTO) {
    models::Element *y = (models::Element *)emalloc(sizeof(models::Element));
    int keep_ln = l->e->n->line_number;
    models::Symbol *keep_fn = l->e->n->file_name;

    /* insert skip - or target is optimized away */
    y->n = l->e->n;           /* copy of the goto   */
    y->seqno = sched::IncrementSymbolMaxElement(a); /* unique seqno within proc */
    y->next = l->e->next;
    y->Seqno = Unique++;
    y->Nxt = Al_El;
    Al_El = y;

    /* turn the original element+seqno into a skip */
    l->e->n =
        models::Lextok::nn(ZN, 'c', models::Lextok::nn(ZN, CONST, ZN, ZN), ZN);
    l->e->n->line_number = l->e->n->left->line_number = keep_ln;
    l->e->n->file_name = l->e->n->left->file_name = keep_fn;
    l->e->n->left->value = 1;
    l->e->next = y; /* append the goto  */
  }
  l->e->status |= CHECK2; /* treat as if global */
  if (l->e->status & (ATOM | L_ATOM | D_ATOM)) {
    printf("spin++: %s:%d, warning, reference to label ", Fname->name.c_str(),
           file::LineNumber::Get());
    printf("from inside atomic or d_step (%s)\n", c->name.c_str());
  }
}

models::Symbol *HasLabel(models::Element *e, int special) {
  models::Label *l;

  for (l = labtab; l; l = l->next) {
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

models::Element *GetLabel(models::Lextok *n, int md) {
  models::Label *l = get_labspec(n);

  if (l != nullptr) {
    return (l->e);
  }

  if (md) {
    file::LineNumber::Set(n->line_number);
    Fname = n->file_name;
    loger::fatal("undefined label %s", n->symbol->name);
  }
  return ZE;
}

void AddSequence(models::Lextok *n) {
  models::Element *e;

  if (!n)
    return;
  innermost = n;
  e = colons(n);
  if (innermost->node_type != IF && innermost->node_type != DO &&
      innermost->node_type != UNLESS)
    add_el(e, cur_s->this_sequence);
}

void SaveBreakDestinantion(void) {
  ob = breakstack;
  popbreak();
}

void RestoreBreakDestinantion(void) {
  breakstack = ob;
  ob = nullptr;
}

void TieUpLooseEnds() /* properly tie-up ends of sub-sequences */
{
  models::Element *e, *f;

  for (e = Al_El; e; e = e->Nxt) {
    if (!e->n || !e->next)
      continue;
    switch (e->n->node_type) {
    case ATOMIC:
    case NON_ATOMIC:
    case D_STEP:
      f = e->next;
      while (f && f->n->node_type == '.')
        f = f->next;
      if (0)
        printf("link %d, {%d .. %d} -> %d (node_type=%d) was %d\n", e->seqno,
               e->n->seq_list->this_sequence->frst->seqno,
               e->n->seq_list->this_sequence->last->seqno, f ? f->seqno : -1,
               f ? f->n->node_type : -1,
               e->n->seq_list->this_sequence->last->next
                   ? e->n->seq_list->this_sequence->last->next->seqno
                   : -1);
      if (!e->n->seq_list->this_sequence->last->next)
        e->n->seq_list->this_sequence->last->next = f;
      else {
        if (e->n->seq_list->this_sequence->last->next->n->node_type != GOTO) {
          if (!f ||
              e->n->seq_list->this_sequence->last->next->seqno != f->seqno)
            loger::non_fatal("unexpected: loose ends");
        } else
          e->n->seq_list->this_sequence->last =
              e->n->seq_list->this_sequence->last->next;
        /*
         * fix_dest can push a goto into the next position
         * in that case the goto wins and f is not needed
         * but the last fields needs adjusting
         */
      }
      break;
    }
  }
}

models::Sequence *CloseSequence(int nottop) {
  models::Sequence *s = cur_s->this_sequence;
  models::Symbol *z;

  if (nottop == 0) /* end of proctype body */
  {
    initialization_ok = 1;
  }

  if (nottop > 0 && s->frst && (z = HasLabel(s->frst, 0))) {
    printf("error: (%s:%d) label %s placed incorrectly\n",
           (s->frst->n) ? s->frst->n->file_name->name.c_str() : "-",
           (s->frst->n) ? s->frst->n->line_number : 0, z->name.c_str());
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
      MainProcessor::Exit(1);
    }
  }

  if (nottop == 4 && !Rjumpslocal(s->frst, s->last))
    loger::fatal("non_local jump in d_step sequence");

  cur_s = cur_s->next;
  s->maxel = Elcnt;
  s->extent = s->last;
  if (!s->last)
    loger::fatal("sequence must have at least one statement");
  return s;
}

void PruneOpts(models::Lextok *n) {
  models::SeqList *l;
  models::Symbol *context = models::Symbol::GetContext();

  if (!n ||
      (context && claimproc && strcmp(context->name.c_str(), claimproc) == 0))
    return;

  for (l = n->seq_list; l; l = l->next) /* find sequences of unlabeled skips */
    check_sequence(l->this_sequence);
}

void CrossDsteps(models::Lextok *a, models::Lextok *b) {
  if (a && b && a->index_step != b->index_step) {
    file::LineNumber::Set(a->line_number);
    Fname = a->file_name;
    if (!launch_settings.need_save_trail)
      loger::fatal("jump into d_step sequence");
  }
}

void OpenSequence(int top) {
  models::SeqList *t;
  models::Sequence *s = (models::Sequence *)emalloc(sizeof(models::Sequence));
  s->minel = -1;

  if (cur_s == nullptr) {
    t = models::SeqList::Build(s);
  } else {
    t = cur_s->Add(s);
  }

  cur_s = t;
  if (top) {
    Elcnt = 1;
    initialization_ok = 1;
  } else {
    initialization_ok = 0;
  }
}

void StartDStepSequence() { DstepStart = Unique; }

void EndDStepSequence() { DstepStart = -1; }
} // namespace flow
