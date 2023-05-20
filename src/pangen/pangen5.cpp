/***** spin: pangen5.c *****/

#include "../fatal/fatal.hpp"
#include "../main/launch_settings.hpp"
#include "../main/main_processor.hpp"
#include "../spin.hpp"
#include "../utils/verbose/verbose.hpp"
#include "y.tab.h"
#include "../run/flow.hpp"

extern LaunchSettings launch_settings;

struct BuildStack {
  models::FSM_trans *t;
  struct BuildStack *next;
};

extern models::ProcList *ready;
extern int verbose, eventmapnr, claimnr, u_sync;
extern models::Element *Al_El;

static models::FSM_state *fsm_free;
static models::FSM_trans *trans_free;
static BuildStack *bs, *bf;
static int max_st_id;
static int cur_st_id;
int o_max;
models::FSM_state *fsmx;
models::FSM_state **fsm_tbl;
models::FSM_use *use_free;

static void ana_seq(models::Sequence *);
static void ana_stmnt(models::FSM_trans *, models::Lextok *, int);

extern void AST_slice(void);
extern void AST_store(models::ProcList *, int);
extern int has_global(models::Lextok *);
extern void exit(int);

static void fsm_table(void) {
  models::FSM_state *f;
  max_st_id += 2;
  /* fprintf(stderr, "omax %d, max=%d\n", o_max, max_st_id); */
  if (o_max < max_st_id) {
    o_max = max_st_id;
    fsm_tbl = (models::FSM_state **)emalloc(max_st_id * sizeof(models::FSM_state *));
  } else
    memset((char *)fsm_tbl, 0, max_st_id * sizeof(models::FSM_state *));
  cur_st_id = max_st_id;
  max_st_id = 0;

  for (f = fsmx; f; f = f->next)
    fsm_tbl[f->from] = f;
}

static int FSM_DFS(int from, models::FSM_use *u) {
  models::FSM_state *f;
  models::FSM_trans *t;
  models::FSM_use *v;
  int n;

  if (from == 0)
    return 1;

  f = fsm_tbl[from];

  if (!f) {
    printf("cannot find state %d\n", from);
    loger::fatal("fsm_dfs: cannot happen\n");
  }

  if (f->seen)
    return 1;
  f->seen = 1;

  for (t = f->t; t; t = t->next) {
    for (n = 0; n < 2; n++)
      for (v = t->Val[n]; v; v = v->next)
        if (u->var == v->var)
          return n; /* a read or write */

    if (!FSM_DFS(t->to, u))
      return 0;
  }
  return 1;
}

static void new_dfs(void) {
  int i;

  for (i = 0; i < cur_st_id; i++)
    if (fsm_tbl[i])
      fsm_tbl[i]->seen = 0;
}

static int good_dead(models::Element *e, models::FSM_use *u) {
  switch (u->special) {
  case 2: /* ok if it's a receive */
    if (e->n->node_type == ASGN && e->n->right->node_type == CONST && e->n->right->value == 0)
      return 0;
    break;
  case 1: /* must be able to use oval */
    if (e->n->node_type != 'c' && e->n->node_type != 'r')
      return 0; /* can't really happen */
    break;
  }
  return 1;
}

static int eligible(models::FSM_trans *v) {
  models::Element *el = ZE;
  models::Lextok *lt = ZN;

  if (v)
    el = v->step;
  if (el)
    lt = v->step->n;

  if (!lt                      /* dead end */
      || v->next                /* has alternatives */
      || el->esc               /* has an escape */
      || (el->status & CHECK2) /* remotely referenced */
      || lt->node_type == ATOMIC ||
      lt->node_type ==
          NON_ATOMIC /* used for inlines -- should be able to handle this */
      || lt->node_type == IF || lt->node_type == C_CODE || lt->node_type == C_EXPR ||
      flow::HasLabel(el, 0)       /* any label at all */
      || lt->node_type == SET_P /* to prevent multiple set_p merges */

      || lt->node_type == DO || lt->node_type == UNLESS || lt->node_type == D_STEP ||
      lt->node_type == ELSE || lt->node_type == '@' || lt->node_type == 'c' ||
      lt->node_type == 'r' || lt->node_type == 's')
    return 0;

  if (!(el->status & (2 | 4))) /* not atomic */
  {
    int unsafe = (el->status & I_GLOB) ? 1 : has_global(el->n);
    if (unsafe)
      return 0;
  }

  return 1;
}

static int canfill_in(models::FSM_trans *v) {
  models::Element *el = v->step;
  models::Lextok *lt = v->step->n;

  if (!lt                       /* dead end */
      || v->next                 /* has alternatives */
      || el->esc                /* has an escape */
      || (el->status & CHECK2)) /* remotely referenced */
    return 0;

  if (!(el->status & (2 | 4))                          /* not atomic */
      && ((el->status & I_GLOB) || has_global(el->n))) /* and not safe */
    return 0;

  return 1;
}

static int pushbuild(models::FSM_trans *v) {
  BuildStack *b;

  for (b = bs; b; b = b->next)
    if (b->t == v)
      return 0;
  if (bf) {
    b = bf;
    bf = bf->next;
  } else
    b = (BuildStack *)emalloc(sizeof(BuildStack));
  b->t = v;
  b->next = bs;
  bs = b;
  return 1;
}

static void popbuild(void) {
  BuildStack *f;
  if (!bs)
    loger::fatal("cannot happen, popbuild");
  f = bs;
  bs = bs->next;
  f->next = bf;
  bf = f; /* freelist */
}

static int build_step(models::FSM_trans *v) {
  models::FSM_state *f;
  models::Element *el;
  int st;
  int r;

  if (!v)
    return -1;

  el = v->step;
  st = v->to;

  if (!el)
    return -1;

  if (v->step->merge)
    return v->step->merge; /* already done */

  if (!eligible(v)) /* non-blocking */
    return -1;

  if (!pushbuild(v)) /* cycle detected */
    return -1;       /* break cycle */

  f = fsm_tbl[st];
  r = build_step(f->t);
  v->step->merge = (r == -1) ? st : r;
  popbuild();

  return v->step->merge;
}

static void FSM_MERGER(
    /* char *pname */ void) /* find candidates for safely merging steps */
{
  models::FSM_state *f, *g;
  models::FSM_trans *t;
  models::Lextok *lt;

  for (f = fsmx; f; f = f->next)   /* all states */
    for (t = f->t; t; t = t->next) /* all edges */
    {
      if (!t->step)
        continue; /* happens with 'unless' */

      t->step->merge_in = f->in; /* ?? */

      if (t->step->merge)
        continue;
      lt = t->step->n;

      if (lt->node_type == 'c' || lt->node_type == 'r' ||
          lt->node_type == 's') /* blocking stmnts */
        continue;          /* handled in 2nd scan */

      if (!eligible(t))
        continue;

      g = fsm_tbl[t->to];
      if (!g || !eligible(g->t)) {
#define SINGLES
#ifdef SINGLES
        t->step->merge_single = t->to;
#endif
        /* t is an isolated eligible step:
         *
         * a merge_start can connect to a proper
         * merge chain or to a merge_single
         * a merge chain can be preceded by
         * a merge_start, but not by a merge_single
         */

        continue;
      }

      (void)build_step(t);
    }

  /* 2nd scan -- find possible merge_starts */

  for (f = fsmx; f; f = f->next)   /* all states */
    for (t = f->t; t; t = t->next) /* all edges */
    {
      if (!t->step || t->step->merge)
        continue;

      lt = t->step->n;
#if 0
	4.1.3:
	an rv send operation ('s') inside an atomic, *loses* atomicity
	when executed, and should therefore never be merged with a subsequent
	statement within the atomic sequence
	the same is not true for non-rv send operations;
	6.2.2:
	RUN statements can start a new process at a higher priority level
	which interferes with statement merging, so it too is not a suitable
	merge target
#endif

      if ((lt->node_type == 'c' && !any_oper(lt->left, RUN)) /* 2nd clause 6.2.2 */
          || lt->node_type == 'r' ||
          (lt->node_type == 's' && u_sync == 0)) /* added !u_sync in 4.1.3 */
      {
        if (!canfill_in(t)) /* atomic, non-global, etc. */
          continue;

        g = fsm_tbl[t->to];
        if (!g || !g->t || !g->t->step)
          continue;
        if (g->t->step->merge)
          t->step->merge_start = g->t->step->merge;
#ifdef SINGLES
        else if (g->t->step->merge_single)
          t->step->merge_start = g->t->step->merge_single;
#endif
      }
    }
}

static void FSM_ANA(void) {
  models::FSM_state *f;
  models::FSM_trans *t;
  models::FSM_use *u, *v, *w;
  int n;

  for (f = fsmx; f; f = f->next)   /* all states */
    for (t = f->t; t; t = t->next) /* all edges */
      for (n = 0; n < 2; n++)     /* reads and writes */
        for (u = t->Val[n]; u; u = u->next) {
          if (!u->var->context /* global */
              || u->var->type == CHAN || u->var->type == STRUCT)
            continue;
          new_dfs();
          if (FSM_DFS(t->to, u)) /* cannot hit read before hitting write */
            u->special = n + 1;  /* means, reset to 0 after use */
        }

  if (!launch_settings.need_export_ast)
    for (f = fsmx; f; f = f->next)
      for (t = f->t; t; t = t->next)
        for (n = 0; n < 2; n++)
          for (u = t->Val[n], w = nullptr; u;) {
            if (u->special) {
              v = u->next;
              if (!w) /* remove from list */
                t->Val[n] = v;
              else
                w->next = v;
              if (good_dead(t->step, u)) {
                u->next = t->step->dead; /* insert into dead */
                t->step->dead = u;
              }
              u = v;
            } else {
              w = u;
              u = u->next;
            }
          }
}

void rel_use(models::FSM_use *u) {
  if (!u)
    return;
  rel_use(u->next);
  u->var = (models::Symbol *)0;
  u->special = 0;
  u->next = use_free;
  use_free = u;
}

static void rel_trans(models::FSM_trans *t) {
  if (!t)
    return;
  rel_trans(t->next);
  rel_use(t->Val[0]);
  rel_use(t->Val[1]);
  t->Val[0] = t->Val[1] = nullptr;
  t->next = trans_free;
  trans_free = t;
}

static void rel_state(models::FSM_state *f) {
  if (!f)
    return;
  rel_state(f->next);
  rel_trans(f->t);
  f->t = nullptr;
  f->next = fsm_free;
  fsm_free = f;
}

static void FSM_DEL(void) {
  rel_state(fsmx);
  fsmx = (models::FSM_state *)0;
}

static models::FSM_state *mkstate(int s) {
  models::FSM_state *f;

  /* fsm_tbl isn't allocated yet */
  for (f = fsmx; f; f = f->next)
    if (f->from == s)
      break;
  if (!f) {
    if (fsm_free) {
      f = fsm_free;
      memset(f, 0, sizeof(models::FSM_state));
      fsm_free = fsm_free->next;
    } else
      f = (models::FSM_state *)emalloc(sizeof(models::FSM_state));
    f->from = s;
    f->t = nullptr;
    f->next = fsmx;
    fsmx = f;
    if (s > max_st_id)
      max_st_id = s;
  }
  return f;
}

static models::FSM_trans *get_trans(int to) {
  models::FSM_trans *t;

  if (trans_free) {
    t = trans_free;
    memset(t, 0, sizeof(models::FSM_trans));
    trans_free = trans_free->next;
  } else
    t = (models::FSM_trans *)emalloc(sizeof(models::FSM_trans));

  t->to = to;
  return t;
}

static void FSM_EDGE(int from, int to, models::Element *e) {
  models::FSM_state *f;
  models::FSM_trans *t;

  f = mkstate(from); /* find it or else make it */
  t = get_trans(to);

  t->step = e;
  t->next = f->t;
  f->t = t;

  f = mkstate(to);
  f->in++;

  if (launch_settings.need_export_ast) {
    t = get_trans(from);
    t->step = e;
    t->next = f->p; /* from is a predecessor of to */
    f->p = t;
  }

  if (t->step) {
    ana_stmnt(t, t->step->n, 0);
  }
}

#define LVAL 1
#define RVAL 0

static void ana_var(models::FSM_trans *t, models::Lextok *now, int usage) {
  models::FSM_use *u, *v;

  if (!t || !now || !now->symbol)
    return;
  if (now->symbol->name[0] == '_' &&
      (now->symbol->name == "_" || now->symbol->name == "_pid" ||
       now->symbol->name == "_priority" || now->symbol->name == "_last"))
    return;

  v = t->Val[usage];
  for (u = v; u; u = u->next)
    if (u->var == now->symbol)
      return; /* it's already there */

  if (!now->left) { /* not for array vars -- it's hard to tell statically
                      if the index would, at runtime, evaluate to the
                      same values at lval and rval references
                   */
    if (use_free) {
      u = use_free;
      use_free = use_free->next;
    } else
      u = (models::FSM_use *)emalloc(sizeof(models::FSM_use));

    u->var = now->symbol;
    u->next = t->Val[usage];
    t->Val[usage] = u;
  } else
    ana_stmnt(t, now->left, RVAL); /* index */

  if (now->symbol->type == STRUCT && now->right && now->right->left)
    ana_var(t, now->right->left, usage);
}

static void ana_stmnt(models::FSM_trans *t, models::Lextok *now, int usage) {
  models::Lextok *v;

  if (!t || !now)
    return;

  switch (now->node_type) {
  case '.':
  case BREAK:
  case GOTO:
  case CONST:
  case TIMEOUT:
  case NONPROGRESS:
  case ELSE:
  case '@':
  case 'q':
  case IF:
  case DO:
  case ATOMIC:
  case NON_ATOMIC:
  case D_STEP:
  case C_CODE:
  case C_EXPR:
    break;

  case ',': /* reached with SET_P and array initializers */
    if (now->left && now->left->right) {
      ana_stmnt(t, now->left->right, RVAL);
    }
    break;

  case '!':
  case UMIN:
  case '~':
  case ENABLED:
  case GET_P:
  case PC_VAL:
  case LEN:
  case FULL:
  case EMPTY:
  case NFULL:
  case NEMPTY:
  case ASSERT:
  case 'c':
    ana_stmnt(t, now->left, RVAL);
    break;

  case SET_P:
    ana_stmnt(t, now->left, RVAL); /* ',' */
    ana_stmnt(t, now->left->right, RVAL);
    break;

  case '/':
  case '*':
  case '-':
  case '+':
  case '%':
  case '&':
  case '^':
  case '|':
  case LT:
  case GT:
  case LE:
  case GE:
  case NE:
  case EQ:
  case OR:
  case AND:
  case LSHIFT:
  case RSHIFT:
    ana_stmnt(t, now->left, RVAL);
    ana_stmnt(t, now->right, RVAL);
    break;

  case ASGN:
    if (check_track(now) == STRUCT) {
      break;
    }

    ana_stmnt(t, now->left, LVAL);
    if (now->right->node_type)
      ana_stmnt(t, now->right, RVAL);
    break;

  case PRINT:
  case RUN:
    for (v = now->left; v; v = v->right)
      ana_stmnt(t, v->left, RVAL);
    break;

  case PRINTM:
    if (now->left && !now->left->is_mtype_token)
      ana_stmnt(t, now->left, RVAL);
    break;

  case 's':
    ana_stmnt(t, now->left, RVAL);
    for (v = now->right; v; v = v->right)
      ana_stmnt(t, v->left, RVAL);
    break;

  case 'R':
  case 'r':
    ana_stmnt(t, now->left, RVAL);
    for (v = now->right; v; v = v->right) {
      if (v->left->node_type == EVAL) {
        if (v->left->left->node_type == ',') {
          ana_stmnt(t, v->left->left->left, RVAL);
        } else {
          ana_stmnt(t, v->left->left, RVAL);
        }
      } else {
        if (v->left->node_type != CONST && now->node_type != 'R') /* was v->left->node_type */
        {
          ana_stmnt(t, v->left, LVAL);
        }
      }
    }
    break;

  case '?':
    ana_stmnt(t, now->left, RVAL);
    if (now->right) {
      ana_stmnt(t, now->right->left, RVAL);
      ana_stmnt(t, now->right->right, RVAL);
    }
    break;

  case NAME:
    ana_var(t, now, usage);
    break;

  case 'p':                            /* remote ref */
    ana_stmnt(t, now->left->left, RVAL); /* process id */
    ana_var(t, now, RVAL);
    ana_var(t, now->right, RVAL);
    break;

  default:
    if (0)
      printf("spin++: %s:%d, bad node type %d usage %d (ana_stmnt)\n",
             now->file_name->name.c_str(), now->line_number, now->node_type, usage);
    loger::fatal("aborting (ana_stmnt)");
  }
}

void ana_src(int dataflow, int merger) /* called from main.c and guided.c */
{
  models::ProcList *p;
  models::Element *e;
  for (p = ready; p; p = p->next) {
    ana_seq(p->s);
    fsm_table();

    e = p->s->frst;
    if (dataflow) {
      FSM_ANA();
    }
    if (merger) {
      FSM_MERGER(/* p->n->name */);
      huntele(e, e->status, -1)->merge_in = 1; /* start-state */
    }
    if (launch_settings.need_export_ast)
      AST_store(p, huntele(e, e->status, -1)->seqno);

    FSM_DEL();
  }
  auto &verbose_flags = utils::verbose::Flags::getInstance();
  for (e = Al_El; e; e = e->Nxt) {
    if (!(e->status & DONE) && verbose_flags.NeedToPrintVerbose()) {
      printf("unreachable code: ");
      printf("%s:%3d  ", e->n->file_name->name.c_str(), e->n->line_number);
      comment(stdout, e->n, 0);
      printf("\n");
    }
    e->status &= ~DONE;
  }
  if (launch_settings.need_export_ast) {
    AST_slice();
    MainProcessor::Exit(0);
  }
}

void spit_recvs(FILE *f1, FILE *f2) /* called from pangen2.c */
{
  models::Element *e;
  models::Sequence *s;
  extern int Unique;

  fprintf(f1, "unsigned char Is_Recv[%d];\n", Unique);

  fprintf(f2, "void\nset_recvs(void)\n{\n");
  for (e = Al_El; e; e = e->Nxt) {
    if (!e->n)
      continue;

    switch (e->n->node_type) {
    case 'r':
    markit:
      fprintf(f2, "\tIs_Recv[%d] = 1;\n", e->Seqno);
      break;
    case D_STEP:
      s = e->n->seq_list->this_sequence;
      switch (s->frst->n->node_type) {
      case DO:
        loger::fatal("unexpected: do at start of d_step");
      case IF: /* conservative: fall through */
      case 'r':
        goto markit;
      }
      break;
    }
  }
  fprintf(f2, "}\n");

  if (launch_settings.need_rendezvous_optimizations) {
    fprintf(f2, "int\nno_recvs(int me)\n{\n");
    fprintf(f2, "	int h; uchar ot; short tt;\n");
    fprintf(f2, "	Trans *t;\n");
    fprintf(f2, "	for (h = BASE; h < (int) now._nr_pr; h++)\n");
    fprintf(f2, "	{	if (h == me) continue;\n");
    fprintf(f2, "		tt = (short) ((P0 *)pptr(h))->_p;\n");
    fprintf(f2, "		ot = (uchar) ((P0 *)pptr(h))->_t;\n");
    fprintf(f2, "		for (t = trans[ot][tt]; t; t = t->next)\n");
    fprintf(f2, "			if (Is_Recv[t->t_id]) return 0;\n");
    fprintf(f2, "	}\n");
    fprintf(f2, "	return 1;\n");
    fprintf(f2, "}\n");
  }
}

static void ana_seq(models::Sequence *s) {
  models::SeqList *h;
  models::Sequence *t;
  models::Element *e, *g;
  int From, To;

  for (e = s->frst; e; e = e->next) {
    if (e->status & DONE)
      goto checklast;

    e->status |= DONE;

    From = e->seqno;

    if (e->n->node_type == UNLESS)
      ana_seq(e->sub->this_sequence);
    else if (e->sub) {
      for (h = e->sub; h; h = h->next) {
        g = huntstart(h->this_sequence->frst);
        To = g->seqno;

        if (g->n->node_type != 'c' || g->n->left->node_type != CONST ||
            g->n->left->value != 0 || g->esc)
          FSM_EDGE(From, To, e);
        /* else it's a dead link */
      }
      for (h = e->sub; h; h = h->next)
        ana_seq(h->this_sequence);
    } else if (e->n->node_type == ATOMIC || e->n->node_type == D_STEP ||
               e->n->node_type == NON_ATOMIC) {
      t = e->n->seq_list->this_sequence;
      g = huntstart(t->frst);
      t->last->next = e->next;
      To = g->seqno;
      FSM_EDGE(From, To, e);

      ana_seq(t);
    } else {
      if (e->n->node_type == GOTO) {
        g = flow::GetLabel(e->n, 1);
        g = huntele(g, e->status, -1);
        if (!g) {
          loger::fatal("unexpected error 2");
        }
        To = g->seqno;
      } else if (e->next) {
        g = huntele(e->next, e->status, -1);
        if (!g) {
          loger::fatal("unexpected error 3");
        }
        To = g->seqno;
      } else
        To = 0;

      FSM_EDGE(From, To, e);

      if (e->esc && e->n->node_type != GOTO && e->n->node_type != '.')
        for (h = e->esc; h; h = h->next) {
          g = huntstart(h->this_sequence->frst);
          To = g->seqno;
          FSM_EDGE(From, To, ZE);
          ana_seq(h->this_sequence);
        }
    }

  checklast:
    if (e == s->last)
      break;
  }
}
