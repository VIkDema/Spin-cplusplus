/***** spin: pangen6.c *****/

#include "../fatal/fatal.hpp"
#include "../main/launch_settings.hpp"
#include "../spin.hpp"
#include "../utils/verbose/verbose.hpp"
#include "y.tab.h"

extern models::Ordered *all_names;
extern FSM_use *use_free;
extern FSM_state **fsm_tbl;
extern FSM_state *fsmx;
extern int o_max;

static FSM_trans *cur_t;
static FSM_trans *expl_par;
static FSM_trans *expl_var;
static FSM_trans *explicit_;

extern void rel_use(FSM_use *);

#define ulong unsigned long

struct Pair {
  FSM_state *h;
  int b;
  struct Pair *nxt;
};

struct AST {
  ProcList *p; /* proctype decl */
  int i_st;    /* start state */
  int nstates, nwords;
  int relevant;
  Pair *pairs;     /* entry and exit nodes of proper subgraphs */
  FSM_state *fsm;  /* proctype body */
  struct AST *nxt; /* linked list */
};

struct RPN { /* relevant proctype names */
  models::Symbol *rn;
  struct RPN *nxt;
};

struct ALIAS {         /* channel aliasing info */
  models::Lextok *cnm; /* this chan */
  int origin;          /* debugging - origin of the alias */
  struct ALIAS *alias; /* can be an alias for these other chans */
  struct ALIAS *nxt;   /* linked list */
};

struct ChanList {
  models::Lextok *s;    /* containing stmnt */
  models::Lextok *n;    /* point of reference - could be struct */
  struct ChanList *nxt; /* linked list */
};

/* a chan alias can be created in one of three ways:
        assignement to chan name
                a = b -- a is now an alias for b
        passing chan name as parameter in run
                run x(b) -- proctype x(chan a)
        passing chan name through channel
                x!b -- x?a
 */

#define USE 1
#define DEF 2
#define DEREF_DEF 4
#define DEREF_USE 8

static AST *ast;
static ALIAS *chalcur;
static ALIAS *chalias;
static ChanList *chanlist;
static models::Slicer *slicer;
static models::Slicer *rel_vars; /* all relevant variables */
static int AST_Changes;
static int AST_Round;
static RPN *rpn;
static int in_recv = 0;

static int AST_mutual(models::Lextok *, models::Lextok *, int);
static void AST_dominant(void);
static void AST_hidden(void);
static void AST_setcur(models::Lextok *);
static void check_slice(models::Lextok *, int);
static void curtail(AST *);
static void def_use(models::Lextok *, int);
static void name_AST_track(models::Lextok *, int);
static void show_expl(void);

static int AST_isini(models::Lextok *n) /* is this an initialized channel */
{
  models::Symbol *s;

  if (!n || !n->symbol)
    return 0;

  s = n->symbol;

  if (s->type == CHAN)
    return (s->init_value->node_type == CHAN); /* freshly instantiated */

  if (s->type == STRUCT && n->right)
    return AST_isini(n->right->left);

  return 0;
}

static void AST_var(models::Lextok *n, models::Symbol *s, int toplevel) {
  if (!s)
    return;

  if (toplevel) {
    if (s->context && s->type)
      printf(":%s:L:", s->context->name.c_str());
    else
      printf("G:");
  }
  printf("%s", s->name.c_str()); /* array indices ignored */

  if (s->type == STRUCT && n && n->right && n->right->left) {
    printf(":");
    AST_var(n->right->left, n->right->left->symbol, 0);
  }
}

static void name_def_indices(models::Lextok *n, int code) {
  if (!n || !n->symbol)
    return;

  if (n->symbol->value_type > 1 || n->symbol->is_array)
    def_use(n->left, code); /* process the index */

  if (n->symbol->type == STRUCT /* and possible deeper ones */
      && n->right)
    name_def_indices(n->right->left, code);
}

static void name_def_use(models::Lextok *n, int code) {
  FSM_use *u;

  if (!n)
    return;

  if ((code & USE) && cur_t->step && cur_t->step->n) {
    switch (cur_t->step->n->node_type) {
    case 'c':                       /* possible predicate abstraction? */
      n->symbol->color_number |= 2; /* yes */
      break;
    default:
      n->symbol->color_number |= 1; /* no  */
      break;
    }
  }

  for (u = cur_t->Val[0]; u; u = u->nxt)
    if (AST_mutual(n, u->n, 1) && u->special == code)
      return;

  if (use_free) {
    u = use_free;
    use_free = use_free->nxt;
  } else
    u = (FSM_use *)emalloc(sizeof(FSM_use));

  u->n = n;
  u->special = code;
  u->nxt = cur_t->Val[0];
  cur_t->Val[0] = u;

  name_def_indices(n, USE | (code & (~DEF))); /* not def, but perhaps deref */
}

static void def_use(models::Lextok *now, int code) {
  models::Lextok *v;

  if (now)
    switch (now->node_type) {
    case '!':
    case UMIN:
    case '~':
    case 'c':
    case ENABLED:
    case SET_P:
    case GET_P:
    case ASSERT:
      def_use(now->left, USE | code);
      break;

    case EVAL:
      if (now->left->node_type == ',') {
        def_use(now->left->left, USE | code);
      } else {
        def_use(now->left, USE | code);
      }
      break;

    case LEN:
    case FULL:
    case EMPTY:
    case NFULL:
    case NEMPTY:
      def_use(now->left, DEREF_USE | USE | code);
      break;

    case '/':
    case '*':
    case '-':
    case '+':
    case '%':
    case '&':
    case '^':
    case '|':
    case LE:
    case GE:
    case GT:
    case LT:
    case NE:
    case EQ:
    case OR:
    case AND:
    case LSHIFT:
    case RSHIFT:
      def_use(now->left, USE | code);
      def_use(now->right, USE | code);
      break;

    case ASGN:
      def_use(now->left, DEF | code);
      def_use(now->right, USE | code);
      break;

    case TYPE: /* name in parameter list */
      name_def_use(now, code);
      break;

    case NAME:
      name_def_use(now, code);
      break;

    case RUN:
      name_def_use(now, USE); /* procname - not really needed */
      for (v = now->left; v; v = v->right)
        def_use(v->left, USE); /* params */
      break;

    case 's':
      def_use(now->left, DEREF_DEF | DEREF_USE | USE | code);
      for (v = now->right; v; v = v->right)
        def_use(v->left, USE | code);
      break;

    case 'r':
      def_use(now->left, DEREF_DEF | DEREF_USE | USE | code);
      for (v = now->right; v; v = v->right) {
        if (v->left->node_type == EVAL) {
          if (v->left->node_type == ',') {
            def_use(v->left->left, code); /* will add USE */
          } else {
            def_use(v->left, code); /* will add USE */
          }
        } else if (v->left->node_type != CONST) {
          def_use(v->left, DEF | code);
        }
      }
      break;

    case 'R':
      def_use(now->left, DEREF_USE | USE | code);
      for (v = now->right; v; v = v->right) {
        if (v->left->node_type == EVAL) {
          if (v->left->node_type == ',') {
            def_use(v->left->left, code); /* will add USE */
          } else {
            def_use(v->left, code); /* will add USE */
          }
        }
      }
      break;

    case '?':
      def_use(now->left, USE | code);
      if (now->right) {
        def_use(now->right->left, code);
        def_use(now->right->right, code);
      }
      break;

    case PRINT:
      for (v = now->left; v; v = v->right)
        def_use(v->left, USE | code);
      break;

    case PRINTM:
      def_use(now->left, USE);
      break;

    case CONST:
    case ELSE: /* ? */
    case NONPROGRESS:
    case PC_VAL:
    case 'p':
    case 'q':
      break;

    case '.':
    case GOTO:
    case BREAK:
    case '@':
    case D_STEP:
    case ATOMIC:
    case NON_ATOMIC:
    case IF:
    case DO:
    case UNLESS:
    case TIMEOUT:
    case C_CODE:
    case C_EXPR:
    default:
      break;
    }
}

static int AST_add_alias(models::Lextok *n, int nr) {
  ALIAS *ca;
  int res;

  for (ca = chalcur->alias; ca; ca = ca->nxt)
    if (AST_mutual(ca->cnm, n, 1)) {
      res = (ca->origin & nr);
      ca->origin |= nr;  /* 1, 2, or 4 - run, asgn, or rcv */
      return (res == 0); /* 0 if already there with same origin */
    }

  ca = (ALIAS *)emalloc(sizeof(ALIAS));
  ca->cnm = n;
  ca->origin = nr;
  ca->nxt = chalcur->alias;
  chalcur->alias = ca;
  return 1;
}

static void AST_run_alias(const std::string &s, models::Lextok *t, int parno) {
  models::Lextok *v;
  int cnt;

  if (!t)
    return;

  if (t->node_type == RUN) {
    if (t->symbol->name == s)
      for (v = t->left, cnt = 1; v; v = v->right, cnt++)
        if (cnt == parno) {
          AST_add_alias(v->left, 1); /* RUN */
          break;
        }
  } else {
    AST_run_alias(s, t->left, parno);
    AST_run_alias(s, t->right, parno);
  }
}

static void AST_findrun(std::string &s, int parno) {
  FSM_state *f;
  FSM_trans *t;
  AST *a;

  for (a = ast; a; a = a->nxt)      /* automata       */
    for (f = a->fsm; f; f = f->nxt) /* control states */
      for (t = f->t; t; t = t->nxt) /* transitions    */
      {
        if (t->step)
          AST_run_alias(s, t->step->n, parno);
      }
}

static void AST_par_chans(
    ProcList *p) /* find local chan's init'd to chan passed as param */
{
  models::Ordered *walk;
  models::Symbol *sp;

  for (walk = all_names; walk; walk = walk->next) {
    sp = walk->entry;
    if (sp && sp->context && sp->context->name == p->n->name &&
        sp->id >= 0 /* not itself a param */
        && sp->type == CHAN &&
        sp->init_value->node_type == NAME) /* != CONST and != CHAN */
    {
      models::Lextok *x = nn(ZN, 0, ZN, ZN);
      x->symbol = sp;
      AST_setcur(x);
      AST_add_alias(sp->init_value, 2); /* ASGN */
    }
  }
}

static void AST_para(ProcList *p) {
  models::Lextok *f, *t, *c;
  int cnt = 0;

  AST_par_chans(p);

  for (f = p->p; f; f = f->right) /* list of types */
    for (t = f->left; t; t = t->right) {
      if (t->node_type != ',')
        c = t;
      else
        c = t->left; /* expanded struct */

      cnt++;
      if (Sym_typ(c) == CHAN) {
        ALIAS *na = (ALIAS *)emalloc(sizeof(ALIAS));

        na->cnm = c;
        na->nxt = chalias;
        chalcur = chalias = na;
        AST_findrun(p->n->name, cnt);
      }
    }
}

static void AST_haschan(models::Lextok *c) {
  if (!c)
    return;
  if (Sym_typ(c) == CHAN) {
    AST_add_alias(c, 2); /* ASGN */
  } else {
    AST_haschan(c->right);
    AST_haschan(c->left);
  }
}

static int AST_nrpar(models::Lextok *n) /* 's' or 'r' */
{
  models::Lextok *m;
  int j = 0;

  for (m = n->right; m; m = m->right)
    j++;
  return j;
}

static int AST_ord(models::Lextok *n, models::Lextok *s) {
  models::Lextok *m;
  int j = 0;

  for (m = n->right; m; m = m->right) {
    j++;
    if (s->symbol == m->left->symbol)
      return j;
  }
  return 0;
}

static int AST_mutual(models::Lextok *a, models::Lextok *b, int toplevel) {
  models::Symbol *as, *bs;

  if (!a && !b)
    return 1;

  if (!a || !b)
    return 0;

  as = a->symbol;
  bs = b->symbol;

  if (!as || !bs)
    return 0;

  if (toplevel && as->context != bs->context)
    return 0;

  if (as->type != bs->type)
    return 0;

  if (as->name != bs->name)
    return 0;

  if (as->type == STRUCT && a->right &&
      b->right) /* we know that a and b are not null */
    return AST_mutual(a->right->left, b->right->left, 0);

  return 1;
}

static void AST_setcur(models::Lextok *n) /* set chalcur */
{
  ALIAS *ca;

  for (ca = chalias; ca; ca = ca->nxt)
    if (AST_mutual(ca->cnm, n, 1)) /* if same chan */
    {
      chalcur = ca;
      return;
    }

  ca = (ALIAS *)emalloc(sizeof(ALIAS));
  ca->cnm = n;
  ca->nxt = chalias;
  chalcur = chalias = ca;
}

static void AST_other(AST *a) /* check chan params in asgns and recvs */
{
  FSM_state *f;
  FSM_trans *t;
  FSM_use *u;
  ChanList *cl;

  for (f = a->fsm; f; f = f->nxt)                        /* control states */
    for (t = f->t; t; t = t->nxt)                        /* transitions    */
      for (u = t->Val[0]; u; u = u->nxt)                 /* def/use info   */
        if (Sym_typ(u->n) == CHAN && (u->special & DEF)) /* def of chan-name  */
        {
          AST_setcur(u->n);
          switch (t->step->n->node_type) {
          case ASGN:
            AST_haschan(t->step->n->right);
            break;
          case 'r':
            /* guess sends where name may originate */
            for (cl = chanlist; cl; cl = cl->nxt) /* all sends */
            {
              int aa = AST_nrpar(cl->s);
              int bb = AST_nrpar(t->step->n);
              if (aa != bb) /* matching nrs of params */
                continue;

              aa = AST_ord(cl->s, cl->n);
              bb = AST_ord(t->step->n, u->n);
              if (aa != bb) /* same position in parlist */
                continue;

              AST_add_alias(cl->n, 4); /* RCV assume possible match */
            }
            break;
          default:
            printf("type = %d\n", t->step->n->node_type);
            loger::non_fatal("unexpected chan def type");
            break;
          }
        }
}

static void AST_aliases(void) {
  ALIAS *na, *ca;

  for (na = chalias; na; na = na->nxt) {
    printf("\npossible aliases of ");
    AST_var(na->cnm, na->cnm->symbol, 1);
    printf("\n\t");
    for (ca = na->alias; ca; ca = ca->nxt) {
      if (!ca->cnm->symbol)
        printf("no valid name ");
      else
        AST_var(ca->cnm, ca->cnm->symbol, 1);
      printf("<");
      if (ca->origin & 1)
        printf("RUN ");
      if (ca->origin & 2)
        printf("ASGN ");
      if (ca->origin & 4)
        printf("RCV ");
      printf("[%s]", AST_isini(ca->cnm) ? "Initzd" : "Name");
      printf(">");
      if (ca->nxt)
        printf(", ");
    }
    printf("\n");
  }
  printf("\n");
}

static void AST_indirect(FSM_use *uin, FSM_trans *t, const std::string &cause,
                         const std::string &pn) {
  FSM_use *u;

  /* this is a newly discovered relevant statement */
  /* all vars it uses to contribute to its DEF are new criteria */

  if (!(t->relevant & 1))
    AST_Changes++;

  t->round = AST_Round;
  t->relevant = 1;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  if (verbose_flags.NeedToPrintVerbose() && t->step) {
    printf("\tDR %s [[ ", pn.c_str());
    comment(stdout, t->step->n, 0);
    printf("]]\n\t\tfully relevant %s", cause.c_str());
    if (uin) {
      printf(" due to ");
      AST_var(uin->n, uin->n->symbol, 1);
    }
    printf("\n");
  }
  for (u = t->Val[0]; u; u = u->nxt)
    if (u != uin && (u->special & (USE | DEREF_USE))) {
      if (verbose_flags.NeedToPrintVerbose()) {
        printf("\t\t\tuses(%d): ", u->special);
        AST_var(u->n, u->n->symbol, 1);
        printf("\n");
      }
      name_AST_track(u->n, u->special); /* add to slice criteria */
    }
}

static void def_relevant(const std::string &pn, FSM_trans *t, models::Lextok *n,
                         int ischan) {
  FSM_use *u;
  ALIAS *na, *ca;
  int chanref;

  /* look for all DEF's of n
   *	mark those stmnts relevant
   *	mark all var USEs in those stmnts as criteria
   */

  if (n->node_type != ELSE)
    for (u = t->Val[0]; u; u = u->nxt) {
      chanref = (Sym_typ(u->n) == CHAN);

      if (ischan != chanref                     /* no possible match  */
          || !(u->special & (DEF | DEREF_DEF))) /* not a def */
        continue;

      if (AST_mutual(u->n, n, 1)) {
        AST_indirect(u, t, "(exact match)", pn);
        continue;
      }

      if (chanref)
        for (na = chalias; na; na = na->nxt) {
          if (!AST_mutual(u->n, na->cnm, 1))
            continue;
          for (ca = na->alias; ca; ca = ca->nxt)
            if (AST_mutual(ca->cnm, n, 1) && AST_isini(ca->cnm)) {
              AST_indirect(u, t, "(alias match)", pn);
              break;
            }
          if (ca)
            break;
        }
    }
}

static void AST_relevant(models::Lextok *n) {
  AST *a;
  FSM_state *f;
  FSM_trans *t;
  int ischan;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  /* look for all DEF's of n
   *	mark those stmnts relevant
   *	mark all var USEs in those stmnts as criteria
   */

  if (!n)
    return;
  ischan = (Sym_typ(n) == CHAN);

  if (verbose_flags.NeedToPrintVerbose()) {
    printf("<<ast_relevant (node_type=%d) ", n->node_type);
    AST_var(n, n->symbol, 1);
    printf(">>\n");
  }

  for (t = expl_par; t; t = t->nxt) /* param assignments */
  {
    if (!(t->relevant & 1))
      def_relevant(":params:", t, n, ischan);
  }

  for (t = expl_var; t; t = t->nxt) {
    if (!(t->relevant & 1)) /* var inits */
      def_relevant(":vars:", t, n, ischan);
  }

  for (a = ast; a; a = a->nxt) /* all other stmnts */
  {
    if (a->p->b != models::btypes::N_CLAIM &&
        a->p->b != models::btypes::E_TRACE &&
        a->p->b != models::btypes::N_TRACE)
      for (f = a->fsm; f; f = f->nxt)
        for (t = f->t; t; t = t->nxt) {
          if (!(t->relevant & 1))
            def_relevant(a->p->n->name, t, n, ischan);
        }
  }
}

static int AST_relpar(const std::string &s) {
  FSM_trans *t, *T;
  FSM_use *u;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  for (T = expl_par; T; T = (T == expl_par) ? expl_var : (FSM_trans *)0)
    for (t = T; t; t = t->nxt) {
      if (t->relevant & 1)
        for (u = t->Val[0]; u; u = u->nxt) {
          if (u->n->symbol->type && u->n->symbol->context &&
              u->n->symbol->context->name == s) {
            if (verbose_flags.NeedToPrintVerbose()) {
              printf("proctype %s relevant, due to symbol ", s.c_str());
              AST_var(u->n, u->n->symbol, 1);
              printf("\n");
            }
            return 1;
          }
        }
    }
  return 0;
}

static void AST_dorelevant(void) {
  AST *a;
  RPN *r;

  for (r = rpn; r; r = r->nxt) {
    for (a = ast; a; a = a->nxt)
      if (a->p->n->name == r->rn->name) {
        a->relevant |= 1;
        break;
      }
    if (!a)
      loger::fatal("cannot find proctype %s", r->rn->name);
  }
}

static void AST_procisrelevant(models::Symbol *s) {
  RPN *r;
  for (r = rpn; r; r = r->nxt)
    if (r->rn->name == s->name)
      return;
  r = (RPN *)emalloc(sizeof(RPN));
  r->rn = s;
  r->nxt = rpn;
  rpn = r;
}

static int AST_proc_isrel(const std::string &s) {
  AST *a;

  for (a = ast; a; a = a->nxt)
    if (a->p->n->name == s)
      return (a->relevant & 1);
  loger::non_fatal("cannot happen, missing proc in ast");
  return 0;
}

static int AST_scoutrun(models::Lextok *t) {
  if (!t)
    return 0;

  if (t->node_type == RUN)
    return AST_proc_isrel(t->symbol->name);
  return (AST_scoutrun(t->left) || AST_scoutrun(t->right));
}

static void AST_tagruns(void) {
  AST *a;
  FSM_state *f;
  FSM_trans *t;

  /* if any stmnt inside a proctype is relevant
   * or any parameter passed in a run
   * then so are all the run statements on that proctype
   */

  for (a = ast; a; a = a->nxt) {
    if (a->p->b == models::btypes::N_CLAIM ||
        a->p->b == models::btypes::I_PROC ||
        a->p->b == models::btypes::E_TRACE ||
        a->p->b == models::btypes::N_TRACE) {
      a->relevant |= 1; /* the proctype is relevant */
      continue;
    }
    if (AST_relpar(a->p->n->name))
      a->relevant |= 1;
    else {
      for (f = a->fsm; f; f = f->nxt)
        for (t = f->t; t; t = t->nxt)
          if (t->relevant)
            goto yes;
    yes:
      if (f)
        a->relevant |= 1;
    }
  }

  for (a = ast; a; a = a->nxt)
    for (f = a->fsm; f; f = f->nxt)
      for (t = f->t; t; t = t->nxt)
        if (t->step && AST_scoutrun(t->step->n)) {
          AST_indirect((FSM_use *)0, t, ":run:", a->p->n->name);
          /* BUT, not all actual params are relevant */
        }
}

static void AST_report(AST *a, Element *e) /* ALSO deduce irrelevant vars */
{
  if (!(a->relevant & 2)) {
    a->relevant |= 2;
    printf("spin: redundant in proctype %s (for given property):\n",
           a->p->n->name.c_str());
  }
  printf("      %s:%d (state %d)", e->n ? e->n->file_name->name.c_str() : "-",
         e->n ? e->n->line_number : -1, e->seqno);
  printf("	[");
  comment(stdout, e->n, 0);
  printf("]\n");
}

static int AST_always(models::Lextok *n) {
  if (!n)
    return 0;

  if (n->node_type == '@'     /* -end */
      || n->node_type == 'p') /* remote reference */
    return 1;
  return AST_always(n->left) || AST_always(n->right);
}

static void AST_edge_dump(AST *a, FSM_state *f) {
  FSM_trans *t;
  FSM_use *u;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  for (t = f->t; t; t = t->nxt) /* edges */
  {
    if (t->step && AST_always(t->step->n))
      t->relevant |= 1; /* always relevant */

    if (verbose_flags.NeedToPrintVerbose()) {
      switch (t->relevant) {
      case 0:
        printf("     ");
        break;
      case 1:
        printf("*%3d ", t->round);
        break;
      case 2:
        printf("+%3d ", t->round);
        break;
      case 3:
        printf("#%3d ", t->round);
        break;
      default:
        printf("? ");
        break;
      }

      printf("%d\t->\t%d\t", f->from, t->to);
      if (t->step)
        comment(stdout, t->step->n, 0);
      else
        printf("Unless");

      for (u = t->Val[0]; u; u = u->nxt) {
        printf(" <");
        AST_var(u->n, u->n->symbol, 1);
        printf(":%d>", u->special);
      }
      printf("\n");
    } else {
      if (t->relevant)
        continue;

      if (t->step)
        switch (t->step->n->node_type) {
        case ASGN:
        case 's':
        case 'r':
        case 'c':
          if (t->step->n->left->node_type != CONST)
            AST_report(a, t->step);
          break;

        case PRINT: /* don't report */
        case PRINTM:
        case ASSERT:
        case C_CODE:
        case C_EXPR:
        default:
          break;
        }
    }
  }
}

static void AST_dfs(AST *a, int s, int vis) {
  FSM_state *f;
  FSM_trans *t;

  f = fsm_tbl[s];
  if (f->seen)
    return;

  f->seen = 1;
  if (vis)
    AST_edge_dump(a, f);

  for (t = f->t; t; t = t->nxt)
    AST_dfs(a, t->to, vis);
}

static void AST_dump(AST *a) {
  FSM_state *f;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  for (f = a->fsm; f; f = f->nxt) {
    f->seen = 0;
    fsm_tbl[f->from] = f;
  }

  if (verbose_flags.NeedToPrintVerbose())
    printf("AST_START %s from %d\n", a->p->n->name.c_str(), a->i_st);

  AST_dfs(a, a->i_st, 1);
}

static void AST_sends(AST *a) {
  FSM_state *f;
  FSM_trans *t;
  FSM_use *u;
  ChanList *cl;

  for (f = a->fsm; f; f = f->nxt) /* control states */
    for (t = f->t; t; t = t->nxt) /* transitions    */
    {
      if (t->step && t->step->n && t->step->n->node_type == 's')
        for (u = t->Val[0]; u; u = u->nxt) {
          if (Sym_typ(u->n) == CHAN &&
              ((u->special & USE) && !(u->special & DEREF_USE))) {
            cl = (ChanList *)emalloc(sizeof(ChanList));
            cl->s = t->step->n;
            cl->n = u->n;
            cl->nxt = chanlist;
            chanlist = cl;
          }
        }
    }
}

static ALIAS *AST_alfind(models::Lextok *n) {
  ALIAS *na;

  for (na = chalias; na; na = na->nxt)
    if (AST_mutual(na->cnm, n, 1))
      return na;
  return (ALIAS *)0;
}

static void AST_trans(void) {
  ALIAS *na, *ca, *da, *ea;
  int nchanges;

  do {
    nchanges = 0;
    for (na = chalias; na; na = na->nxt) {
      chalcur = na;
      for (ca = na->alias; ca; ca = ca->nxt) {
        da = AST_alfind(ca->cnm);
        if (da)
          for (ea = da->alias; ea; ea = ea->nxt) {
            nchanges += AST_add_alias(ea->cnm, ea->origin | ca->origin);
          }
      }
    }
  } while (nchanges > 0);

  chalcur = (ALIAS *)0;
}

static void AST_def_use(AST *a) {
  FSM_state *f;
  FSM_trans *t;

  for (f = a->fsm; f; f = f->nxt) /* control states */
    for (t = f->t; t; t = t->nxt) /* all edges */
    {
      cur_t = t;
      rel_use(t->Val[0]); /* redo Val; doesn't cover structs */
      rel_use(t->Val[1]);
      t->Val[0] = t->Val[1] = (FSM_use *)0;

      if (!t->step)
        continue;

      def_use(t->step->n, 0); /* def/use info, including structs */
    }
  cur_t = (FSM_trans *)0;
}

static void name_AST_track(models::Lextok *n, int code) {
  extern int nr_errs;
  if (in_recv && (code & DEF) && (code & USE)) {
    printf("spin: %s:%d, error: DEF and USE of same var in rcv stmnt: ",
           n->file_name->name.c_str(), n->line_number);
    AST_var(n, n->symbol, 1);
    printf(" -- %d\n", code);
    nr_errs++;
  }
  check_slice(n, code);
}

void AST_track(models::Lextok *now, int code) /* called from main.c */
{
  models::Lextok *v;

  extern LaunchSettings launch_settings;
  if (!launch_settings.need_export_ast)
    return;

  if (now)
    switch (now->node_type) {
    case LEN:
    case FULL:
    case EMPTY:
    case NFULL:
    case NEMPTY:
      AST_track(now->left, DEREF_USE | USE | code);
      break;

    case '/':
    case '*':
    case '-':
    case '+':
    case '%':
    case '&':
    case '^':
    case '|':
    case LE:
    case GE:
    case GT:
    case LT:
    case NE:
    case EQ:
    case OR:
    case AND:
    case LSHIFT:
    case RSHIFT:
      AST_track(now->right, USE | code);
      /* fall through */
    case '!':
    case UMIN:
    case '~':
    case 'c':
    case ENABLED:
    case SET_P:
    case GET_P:
    case ASSERT:
      AST_track(now->left, USE | code);
      break;

    case EVAL:
      if (now->left->node_type == ',') {
        AST_track(now->left->left, USE | (code & (~DEF)));
      } else {
        AST_track(now->left, USE | (code & (~DEF)));
      }
      break;

    case NAME:
      name_AST_track(now, code);
      if (now->symbol->value_type > 1 || now->symbol->is_array)
        AST_track(now->left, USE); /* index, was USE|code */
      break;

    case 'R':
      AST_track(now->left, DEREF_USE | USE | code);
      for (v = now->right; v; v = v->right)
        AST_track(v->left, code); /* a deeper eval can add USE */
      break;

    case '?':
      AST_track(now->left, USE | code);
      if (now->right) {
        AST_track(now->right->left, code);
        AST_track(now->right->right, code);
      }
      break;

      /* added for control deps: */
    case TYPE:
      name_AST_track(now, code);
      break;
    case ASGN:
      AST_track(now->left, DEF | code);
      AST_track(now->right, USE | code);
      break;
    case RUN:
      name_AST_track(now, USE);
      for (v = now->left; v; v = v->right)
        AST_track(v->left, USE | code);
      break;
    case 's':
      AST_track(now->left, DEREF_DEF | DEREF_USE | USE | code);
      for (v = now->right; v; v = v->right)
        AST_track(v->left, USE | code);
      break;
    case 'r':
      AST_track(now->left, DEREF_DEF | DEREF_USE | USE | code);
      for (v = now->right; v; v = v->right) {
        in_recv++;
        AST_track(v->left, DEF | code);
        in_recv--;
      }
      break;
    case PRINT:
      for (v = now->left; v; v = v->right)
        AST_track(v->left, USE | code);
      break;
    case PRINTM:
      AST_track(now->left, USE);
      break;
      /* end add */
    case 'p':
#if 0
			   'p' -sym-> _p
			   /
			 '?' -sym-> a (proctype)
			 /
			b (pid expr)
#endif
      AST_track(now->left->left, USE | code);
      AST_procisrelevant(now->left->symbol);
      break;

    case CONST:
    case ELSE:
    case NONPROGRESS:
    case PC_VAL:
    case 'q':
      break;

    case '.':
    case GOTO:
    case BREAK:
    case '@':
    case D_STEP:
    case ATOMIC:
    case NON_ATOMIC:
    case IF:
    case DO:
    case UNLESS:
    case TIMEOUT:
    case C_CODE:
    case C_EXPR:
      break;

    default:
      printf("AST_track, NOT EXPECTED node_type: %d\n", now->node_type);
      break;
    }
}
static int AST_dump_rel(void) {
  models::Ordered *walk;
  std::string buf;
  int banner = 0;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  if (verbose_flags.NeedToPrintVerbose()) {
    printf("Relevant variables:\n");
    for (auto rv = rel_vars; rv; rv = rv->next) {
      printf("\t");
      AST_var(rv->slice_criterion, rv->slice_criterion->symbol, 1);
      printf("\n");
    }
    return 1;
  }
  for (auto rv = rel_vars; rv; rv = rv->next)
    rv->slice_criterion->symbol->last_depth = 1; /* mark it */

  for (walk = all_names; walk; walk = walk->next) {
    models::Symbol *s;
    s = walk->entry;
    if (!s->last_depth &&
        (s->type != MTYPE || s->init_value->node_type != CONST) &&
        s->type != STRUCT /* report only fields */
        && s->type != PROCTYPE && !s->owner_name && sputtype(buf, s->type)) {
      if (!banner) {
        banner = 1;
        printf("spin: redundant vars (for given property):\n");
      }
      printf("\t");
      symvar(s);
    }
  }
  return banner;
}

static void AST_suggestions(void) {
  models::Symbol *s;
  models::Ordered *walk;
  FSM_state *f;
  FSM_trans *t;
  AST *a;
  int banner = 0;
  int talked = 0;

  for (walk = all_names; walk; walk = walk->next) {
    s = walk->entry;
    if (s->color_number == 2 /* only used in conditionals */
        && (s->type == BYTE || s->type == SHORT || s->type == INT ||
            s->type == MTYPE)) {
      if (!banner) {
        banner = 1;
        printf("spin: consider using predicate");
        printf(" abstraction to replace:\n");
      }
      printf("\t");
      symvar(s);
    }
  }

  /* look for source and sink processes */

  for (a = ast; a; a = a->nxt) /* automata       */
  {
    banner = 0;
    for (f = a->fsm; f; f = f->nxt) /* control states */
      for (t = f->t; t; t = t->nxt) /* transitions    */
      {
        if (t->step)
          switch (t->step->n->node_type) {
          case 's':
            banner |= 1;
            break;
          case 'r':
            banner |= 2;
            break;
          case '.':
          case D_STEP:
          case ATOMIC:
          case NON_ATOMIC:
          case IF:
          case DO:
          case UNLESS:
          case '@':
          case GOTO:
          case BREAK:
          case PRINT:
          case PRINTM:
          case ASSERT:
          case C_CODE:
          case C_EXPR:
            break;
          default:
            banner |= 4;
            goto no_good;
          }
      }
  no_good:
    if (banner == 1 || banner == 2) {
      printf("spin: proctype %s defines a %s process\n", a->p->n->name.c_str(),
             banner == 1 ? "source" : "sink");
      talked |= banner;
    } else if (banner == 3) {
      printf("spin: proctype %s mimics a buffer\n", a->p->n->name.c_str());
      talked |= 4;
    }
  }
  if (talked & 1) {
    printf("\tto reduce complexity, consider merging the code of\n");
    printf("\teach source process into the code of its target\n");
  }
  if (talked & 2) {
    printf("\tto reduce complexity, consider merging the code of\n");
    printf("\teach sink process into the code of its source\n");
  }
  if (talked & 4)
    printf("\tto reduce complexity, avoid buffer processes\n");
}

static void AST_preserve(void) {
  models::Slicer *sc, *nx, *rv;

  for (sc = slicer; sc; sc = nx) {
    if (!sc->used)
      break; /* done */

    nx = sc->next;

    for (rv = rel_vars; rv; rv = rv->next)
      if (AST_mutual(sc->slice_criterion, rv->slice_criterion, 1))
        break;

    if (!rv) /* not already there */
    {
      sc->next = rel_vars;
      rel_vars = sc;
    }
  }
  slicer = sc;
}

static void check_slice(models::Lextok *n, int code) {
  models::Slicer *sc;

  for (sc = slicer; sc; sc = sc->next)
    if (AST_mutual(sc->slice_criterion, n, 1) && sc->code == code)
      return; /* already there */

  sc = (models::Slicer *)emalloc(sizeof(models::Slicer));
  sc->slice_criterion = n;

  sc->code = code;
  sc->used = 0;
  sc->next = slicer;
  slicer = sc;
}

static void AST_data_dep(void) {
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  /* mark all def-relevant transitions */
  for (auto sc = slicer; sc; sc = sc->next) {
    sc->used = 1;
    if (verbose_flags.NeedToPrintVerbose()) {
      printf("spin: slice criterion ");
      AST_var(sc->slice_criterion, sc->slice_criterion->symbol, 1);
      printf(" type=%d\n", Sym_typ(sc->slice_criterion));
    }
    AST_relevant(sc->slice_criterion);
  }
  AST_tagruns(); /* mark 'run's relevant if target proctype is relevant */
}

static int AST_blockable(AST *a, int s) {
  FSM_state *f;
  FSM_trans *t;

  f = fsm_tbl[s];

  for (t = f->t; t; t = t->nxt) {
    if (t->relevant & 2)
      return 1;

    if (t->step && t->step->n)
      switch (t->step->n->node_type) {
      case IF:
      case DO:
      case ATOMIC:
      case NON_ATOMIC:
      case D_STEP:
        if (AST_blockable(a, t->to)) {
          t->round = AST_Round;
          t->relevant |= 2;
          return 1;
        }
        /* else fall through */
      default:
        break;
      }
    else if (AST_blockable(a, t->to)) /* Unless */
    {
      t->round = AST_Round;
      t->relevant |= 2;
      return 1;
    }
  }
  return 0;
}

static void AST_spread(AST *a, int s) {
  FSM_state *f;
  FSM_trans *t;

  f = fsm_tbl[s];

  for (t = f->t; t; t = t->nxt) {
    if (t->relevant & 2)
      continue;

    if (t->step && t->step->n)
      switch (t->step->n->node_type) {
      case IF:
      case DO:
      case ATOMIC:
      case NON_ATOMIC:
      case D_STEP:
        AST_spread(a, t->to);
        /* fall thru */
      default:
        t->round = AST_Round;
        t->relevant |= 2;
        break;
      }
    else /* Unless */
    {
      AST_spread(a, t->to);
      t->round = AST_Round;
      t->relevant |= 2;
    }
  }
}

static int AST_notrelevant(models::Lextok *n) {
  for (auto s = rel_vars; s; s = s->next)
    if (AST_mutual(s->slice_criterion, n, 1))
      return 0;
  for (auto s = slicer; s; s = s->next)
    if (AST_mutual(s->slice_criterion, n, 1))
      return 0;
  return 1;
}

static int AST_withchan(models::Lextok *n) {
  if (!n)
    return 0;
  if (Sym_typ(n) == CHAN)
    return 1;
  return AST_withchan(n->left) || AST_withchan(n->right);
}

static int AST_suspect(FSM_trans *t) {
  FSM_use *u;
  /* check for possible overkill */
  if (!t || !t->step || !AST_withchan(t->step->n))
    return 0;
  for (u = t->Val[0]; u; u = u->nxt)
    if (AST_notrelevant(u->n))
      return 1;
  return 0;
}

static void AST_shouldconsider(AST *a, int s) {
  FSM_state *f;
  FSM_trans *t;

  f = fsm_tbl[s];
  for (t = f->t; t; t = t->nxt) {
    if (t->step && t->step->n)
      switch (t->step->n->node_type) {
      case IF:
      case DO:
      case ATOMIC:
      case NON_ATOMIC:
      case D_STEP:
        AST_shouldconsider(a, t->to);
        break;
      default:
        AST_track(t->step->n, 0);
        /*
                AST_track is called here for a blockable stmnt from which
                a relevant stmnmt was shown to be reachable
                for a condition this makes all USEs relevant
                but for a channel operation it only makes the executability
                relevant -- in those cases, parameters that aren't already
                relevant may be replaceable with arbitrary tokens
         */
        if (AST_suspect(t)) {
          printf("spin: possibly redundant parameters in: ");
          comment(stdout, t->step->n, 0);
          printf("\n");
        }
        break;
      }
    else /* an Unless */
      AST_shouldconsider(a, t->to);
  }
}

static int FSM_critical(AST *a, int s) {
  FSM_state *f;
  FSM_trans *t;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  /* is a 1-relevant stmnt reachable from this state? */

  f = fsm_tbl[s];
  if (f->seen)
    goto done;
  f->seen = 1;
  f->cr = 0;
  for (t = f->t; t; t = t->nxt)
    if ((t->relevant & 1) || FSM_critical(a, t->to)) {
      f->cr = 1;

      if (verbose_flags.NeedToPrintVerbose()) {
        printf("\t\t\t\tcritical(%d) ", t->relevant);
        comment(stdout, t->step->n, 0);
        printf("\n");
      }
      break;
    }
done:
  return f->cr;
}

static void AST_ctrl(AST *a) {
  FSM_state *f;
  FSM_trans *t;
  int hit;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  /* add all blockable transitions
   * from which relevant transitions can be reached
   */
  if (verbose_flags.NeedToPrintVerbose())
    printf("CTL -- %s\n", a->p->n->name.c_str());

  /* 1 : mark all blockable edges */
  for (f = a->fsm; f; f = f->nxt) {
    if (!(f->scratch & 2)) /* not part of irrelevant subgraph */
      for (t = f->t; t; t = t->nxt) {
        if (t->step && t->step->n)
          switch (t->step->n->node_type) {
          case 'r':
          case 's':
          case 'c':
          case ELSE:
            t->round = AST_Round;
            t->relevant |= 2; /* mark for next phases */
            if (verbose_flags.NeedToPrintVerbose()) {
              printf("\tpremark ");
              comment(stdout, t->step->n, 0);
              printf("\n");
            }
            break;
          default:
            break;
          }
      }
  }

  /* 2: keep only 2-marked stmnts from which 1-marked stmnts can be reached */
  for (f = a->fsm; f; f = f->nxt) {
    fsm_tbl[f->from] = f;
    f->seen = 0; /* used in dfs from FSM_critical */
  }
  for (f = a->fsm; f; f = f->nxt) {
    if (!FSM_critical(a, f->from))
      for (t = f->t; t; t = t->nxt)
        if (t->relevant & 2) {
          t->relevant &= ~2; /* clear mark */
          if (verbose_flags.NeedToPrintVerbose()) {
            printf("\t\tnomark ");
            if (t->step && t->step->n)
              comment(stdout, t->step->n, 0);
            printf("\n");
          }
        }
  }

  /* 3 : lift marks across IF/DO etc. */
  for (f = a->fsm; f; f = f->nxt) {
    hit = 0;
    for (t = f->t; t; t = t->nxt) {
      if (t->step && t->step->n)
        switch (t->step->n->node_type) {
        case IF:
        case DO:
        case ATOMIC:
        case NON_ATOMIC:
        case D_STEP:
          if (AST_blockable(a, t->to))
            hit = 1;
          break;
        default:
          break;
        }
      else if (AST_blockable(a, t->to)) /* Unless */
        hit = 1;

      if (hit)
        break;
    }
    if (hit) /* at least one outgoing trans can block */
      for (t = f->t; t; t = t->nxt) {
        t->round = AST_Round;
        t->relevant |= 2; /* lift */
        if (verbose_flags.NeedToPrintVerbose()) {
          printf("\t\t\tliftmark ");
          if (t->step && t->step->n)
            comment(stdout, t->step->n, 0);
          printf("\n");
        }
        AST_spread(a, t->to); /* and spread to all guards */
      }
  }

  /* 4: nodes with 2-marked out-edges contribute new slice criteria */
  for (f = a->fsm; f; f = f->nxt)
    for (t = f->t; t; t = t->nxt)
      if (t->relevant & 2) {
        AST_shouldconsider(a, f->from);
        break; /* inner loop */
      }
}

static void AST_control_dep(void) {
  AST *a;

  for (a = ast; a; a = a->nxt) {
    if (a->p->b != models::btypes::N_CLAIM &&
        a->p->b != models::btypes::E_TRACE &&
        a->p->b != models::btypes::N_TRACE) {
      AST_ctrl(a);
    }
  }
}

static void AST_prelabel(void) {
  AST *a;
  FSM_state *f;
  FSM_trans *t;

  for (a = ast; a; a = a->nxt) {
    if (a->p->b != models::btypes::N_CLAIM &&
        a->p->b != models::btypes::E_TRACE &&
        a->p->b != models::btypes::N_TRACE)
      for (f = a->fsm; f; f = f->nxt)
        for (t = f->t; t; t = t->nxt) {
          if (t->step && t->step->n && t->step->n->node_type == ASSERT) {
            t->relevant |= 1;
          }
        }
  }
}

static void
AST_criteria(void) { /*
                      * remote labels are handled separately -- by making
                      * sure they are not pruned away during optimization
                      */
  auto &verbose_flags = utils::verbose::Flags::getInstance();
  AST_Changes = 1; /* to get started */
  for (AST_Round = 1; slicer && AST_Changes; AST_Round++) {
    AST_Changes = 0;
    AST_data_dep();
    AST_preserve();    /* moves processed vars from slicer to rel_vars */
    AST_dominant();    /* mark data-irrelevant subgraphs */
    AST_control_dep(); /* can add data deps, which add control deps */

    if (verbose_flags.NeedToPrintVerbose())
      printf("\n\nROUND %d -- changes %d\n", AST_Round, AST_Changes);
  }
}

static void AST_alias_analysis(void) /* aliasing of promela channels */
{
  AST *a;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  for (a = ast; a; a = a->nxt)
    AST_sends(a); /* collect chan-names that are send across chans */

  for (a = ast; a; a = a->nxt)
    AST_para(a->p); /* aliasing of chans thru proctype parameters */

  for (a = ast; a; a = a->nxt)
    AST_other(a); /* chan params in asgns and recvs */

  AST_trans(); /* transitive closure of alias table */

  if (verbose_flags.NeedToPrintVerbose())
    AST_aliases(); /* show channel aliasing info */
}

void AST_slice(void) {
  AST *a;
  int spurious = 0;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  if (!slicer) {
    printf("spin: warning: no slice criteria found (no assertions and no "
           "claim)\n");
    spurious = 1;
  }
  AST_dorelevant(); /* mark procs refered to in remote refs */

  for (a = ast; a; a = a->nxt)
    AST_def_use(a); /* compute standard def/use information */

  AST_hidden(); /* parameter passing and local var inits */

  AST_alias_analysis(); /* channel alias analysis */

  AST_prelabel(); /* mark all 'assert(...)' stmnts as relevant */
  AST_criteria(); /* process the slice criteria from
                   * asserts and from the never claim
                   */
  if (!spurious || verbose_flags.NeedToPrintVerbose()) {
    spurious = 1;
    for (a = ast; a; a = a->nxt) {
      AST_dump(a);         /* marked up result */
      if (a->relevant & 2) /* it printed something */
        spurious = 0;
    }
    if (!AST_dump_rel() /* relevant variables */
        && spurious)
      printf("spin: no redundancies found (for given property)\n");
  }
  AST_suggestions();

  if (verbose_flags.NeedToPrintVerbose())
    show_expl();
}

void AST_store(ProcList *p, int start_state) {
  AST *n_ast;

  if (p->b != models::btypes::N_CLAIM && p->b != models::btypes::E_TRACE &&
      p->b != models::btypes::N_TRACE) {
    n_ast = (AST *)emalloc(sizeof(AST));
    n_ast->p = p;
    n_ast->i_st = start_state;
    n_ast->relevant = 0;
    n_ast->fsm = fsmx;
    n_ast->nxt = ast;
    ast = n_ast;
  }
  fsmx = (FSM_state *)0; /* hide it from FSM_DEL */
}

static void AST_add_explicit(models::Lextok *d, models::Lextok *u) {
  FSM_trans *e = (FSM_trans *)emalloc(sizeof(FSM_trans));

  e->to = 0;              /* or start_state ? */
  e->relevant = 0;        /* to be determined */
  e->step = (Element *)0; /* left blank */
  e->Val[0] = e->Val[1] = (FSM_use *)0;

  cur_t = e;

  def_use(u, USE);
  def_use(d, DEF);

  cur_t = (FSM_trans *)0;

  e->nxt = explicit_;
  explicit_ = e;
}

static void AST_fp1(const std::string &s, models::Lextok *t, models::Lextok *f,
                    int parno) {
  models::Lextok *v;
  int cnt;

  if (!t)
    return;

  if (t->node_type == RUN) {
    if (t->symbol->name == s) {
      for (v = t->left, cnt = 1; v; v = v->right, cnt++)
        if (cnt == parno) {
          AST_add_explicit(f, v->left);
          break;
        }
    }
  } else {
    AST_fp1(s, t->left, f, parno);
    AST_fp1(s, t->right, f, parno);
  }
}

static void AST_mk1(const std::string &s, models::Lextok *c, int parno) {
  AST *a;
  FSM_state *f;
  FSM_trans *t;

  /* concoct an extra FSM_trans *t with the asgn of
   * formal par c to matching actual pars made explicit
   */

  for (a = ast; a; a = a->nxt)      /* automata       */
    for (f = a->fsm; f; f = f->nxt) /* control states */
      for (t = f->t; t; t = t->nxt) /* transitions    */
      {
        if (t->step)
          AST_fp1(s, t->step->n, c, parno);
      }
}

static void AST_par_init() /* parameter passing -- hidden_flags assignments */
{
  AST *a;
  models::Lextok *f, *t, *c;
  int cnt;

  for (a = ast; a; a = a->nxt) {
    if (a->p->b == models::btypes::N_CLAIM ||
        a->p->b == models::btypes::I_PROC ||
        a->p->b == models::btypes::E_TRACE ||
        a->p->b == models::btypes::N_TRACE) {
      continue; /* has no params */
    }
    cnt = 0;
    for (f = a->p->p; f; f = f->right)   /* types */
      for (t = f->left; t; t = t->right) /* formals */
      {
        cnt++;                                   /* formal par count */
        c = (t->node_type != ',') ? t : t->left; /* the formal parameter */
        AST_mk1(a->p->n->name, c, cnt); /* all matching run statements */
      }
  }
}

static void
AST_var_init(void) /* initialized vars (not chans) - hidden_flags assignments */
{
  models::Ordered *walk;
  models::Lextok *x;
  models::Symbol *sp;
  AST *a;

  for (walk = all_names; walk; walk = walk->next) {
    sp = walk->entry;
    if (sp && !sp->context /* globals */
        && sp->type != PROCTYPE && sp->init_value &&
        (sp->type != MTYPE ||
         sp->init_value->node_type != CONST) /* not mtype defs */
        && sp->init_value->node_type != CHAN) {
      x = nn(ZN, TYPE, ZN, ZN);
      x->symbol = sp;
      AST_add_explicit(x, sp->init_value);
    }
  }

  for (a = ast; a; a = a->nxt) {
    if (a->p->b != models::btypes::N_CLAIM &&
        a->p->b != models::btypes::E_TRACE &&
        a->p->b != models::btypes::N_TRACE) /* has no locals */
      for (walk = all_names; walk; walk = walk->next) {
        sp = walk->entry;
        if (sp && sp->context && sp->context->name == a->p->n->name &&
            sp->id >= 0 /* not a param */
            && sp->type != LABEL && sp->init_value &&
            sp->init_value->node_type != CHAN) {
          x = nn(ZN, TYPE, ZN, ZN);
          x->symbol = sp;
          AST_add_explicit(x, sp->init_value);
        }
      }
  }
}

static void show_expl(void) {
  FSM_trans *t, *T;
  FSM_use *u;

  printf("\nExplicit List:\n");
  for (T = expl_par; T; T = (T == expl_par) ? expl_var : (FSM_trans *)0) {
    for (t = T; t; t = t->nxt) {
      if (!t->Val[0])
        continue;
      printf("%s", t->relevant ? "*" : " ");
      printf("%3d", t->round);
      for (u = t->Val[0]; u; u = u->nxt) {
        printf("\t<");
        AST_var(u->n, u->n->symbol, 1);
        printf(":%d>, ", u->special);
      }
      printf("\n");
    }
    printf("==\n");
  }
  printf("End\n");
}

static void AST_hidden(void) /* reveal all hidden_flags assignments */
{
  AST_par_init();
  expl_par = explicit_;
  explicit_ = (FSM_trans *)0;

  AST_var_init();
  expl_var = explicit_;
  explicit_ = (FSM_trans *)0;
}

#define BPW (8 * sizeof(ulong)) /* bits per word */

static int bad_scratch(FSM_state *f, int upto) {
  FSM_trans *t;
  auto &verbose_flags = utils::verbose::Flags::getInstance();
#if 0
	1. all internal branch-points have else-s
	2. all non-branchpoints have non-blocking out-edge
	3. all internal edges are non-relevant
	subgraphs like this need NOT contribute control-dependencies
#endif

  if (!f->seen || (f->scratch & 4))
    return 0;

  if (f->scratch & 8)
    return 1;

  f->scratch |= 4;

  if (verbose_flags.NeedToPrintVerbose())
    printf("X[%d:%d:%d] ", f->from, upto, f->scratch);

  if (f->scratch & 1) {
    if (verbose_flags.NeedToPrintVerbose())
      printf("\tbad scratch: %d\n", f->from);
  bad:
    f->scratch &= ~4;
    /*	f->scratch |=  8;	 wrong */
    return 1;
  }

  if (f->from != upto)
    for (t = f->t; t; t = t->nxt)
      if (bad_scratch(fsm_tbl[t->to], upto))
        goto bad;

  return 0;
}

static void mark_subgraph(FSM_state *f, int upto) {
  FSM_trans *t;

  if (f->from == upto || !f->seen || (f->scratch & 2))
    return;

  f->scratch |= 2;

  for (t = f->t; t; t = t->nxt)
    mark_subgraph(fsm_tbl[t->to], upto);
}

static void AST_pair(AST *a, FSM_state *h, int y) {
  Pair *p;

  for (p = a->pairs; p; p = p->nxt)
    if (p->h == h && p->b == y)
      return;

  p = (Pair *)emalloc(sizeof(Pair));
  p->h = h;
  p->b = y;
  p->nxt = a->pairs;
  a->pairs = p;
}

static void AST_checkpairs(AST *a) {
  Pair *p;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  for (p = a->pairs; p; p = p->nxt) {
    if (verbose_flags.NeedToPrintVerbose())
      printf("	inspect pair %d %d\n", p->b, p->h->from);
    if (!bad_scratch(p->h, p->b)) /* subgraph is clean */
    {
      if (verbose_flags.NeedToPrintVerbose())
        printf("subgraph: %d .. %d\n", p->b, p->h->from);
      mark_subgraph(p->h, p->b);
    }
  }
}

static void subgraph(AST *a, FSM_state *f, int out) {
  FSM_state *h;
  int i, j;
  ulong *g;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

#if 0
	reverse dominance suggests that this is a possible
	entry and exit node for a proper subgraph
#endif
  h = fsm_tbl[out];

  i = f->from / BPW;
  j = f->from % BPW; /* assert(j <= 32); else lshift undefined? */
  g = h->mod;

  if (verbose_flags.NeedToPrintVerbose())
    printf("possible pair %d %d -- %d\n", f->from, h->from,
           (g[i] & (1 << j)) ? 1 : 0);

  if (g[i] & (1 << j))       /* also a forward dominance pair */
    AST_pair(a, h, f->from); /* record this pair */
}

static void act_dom(AST *a) {
  FSM_state *f;
  FSM_trans *t;
  int i, j, cnt;

  for (f = a->fsm; f; f = f->nxt) {
    if (!f->seen)
      continue;
#if 0
		f->from is the exit-node of a proper subgraph, with
		the dominator its entry-node, if:
		a. this node has more than 1 reachable predecessor
		b. the dominator has more than 1 reachable successor
		   (need reachability - in case of reverse dominance)
		d. the dominator is reachable, and not equal to this node
#endif
    for (t = f->p, i = 0; t; t = t->nxt) {
      i += fsm_tbl[t->to]->seen;
    }
    if (i <= 1) {
      continue; /* a. */
    }
    for (cnt = 1; cnt < a->nstates; cnt++) /* 0 is endstate */
    {
      if (cnt == f->from || !fsm_tbl[cnt]->seen) {
        continue; /* c. */
      }
      i = cnt / BPW;
      j = cnt % BPW; /* assert(j <= 32); */
      if (!(f->dom[i] & (1 << j))) {
        continue;
      }
      for (t = fsm_tbl[cnt]->t, i = 0; t; t = t->nxt) {
        i += fsm_tbl[t->to]->seen;
      }
      if (i <= 1) {
        continue; /* b. */
      }
      if (f->mod) /* final check in 2nd phase */
      {
        subgraph(a, f, cnt); /* possible entry-exit pair */
      }
    }
  }
}

static void reachability(AST *a) {
  FSM_state *f;

  for (f = a->fsm; f; f = f->nxt)
    f->seen = 0;          /* clear */
  AST_dfs(a, a->i_st, 0); /* mark 'seen' */
}

static int see_else(FSM_state *f) {
  FSM_trans *t;

  for (t = f->t; t; t = t->nxt) {
    if (t->step && t->step->n)
      switch (t->step->n->node_type) {
      case ELSE:
        return 1;
      case IF:
      case DO:
      case ATOMIC:
      case NON_ATOMIC:
      case D_STEP:
        if (see_else(fsm_tbl[t->to]))
          return 1;
      default:
        break;
      }
  }
  return 0;
}

static int is_guard(FSM_state *f) {
  FSM_state *g;
  FSM_trans *t;

  for (t = f->p; t; t = t->nxt) {
    g = fsm_tbl[t->to];
    if (!g->seen)
      continue;

    if (t->step && t->step->n)
      switch (t->step->n->node_type) {
      case IF:
      case DO:
        return 1;
      case ATOMIC:
      case NON_ATOMIC:
      case D_STEP:
        if (is_guard(g))
          return 1;
      default:
        break;
      }
  }
  return 0;
}

static void curtail(AST *a) {
  FSM_state *f, *g;
  FSM_trans *t;
  int i, haselse, isrel, blocking;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

#if 0
	mark nodes that do not satisfy these requirements:
	1. all internal branch-points have else-s
	2. all non-branchpoints have non-blocking out-edge
	3. all internal edges are non-data-relevant
#endif
  if (verbose_flags.NeedToPrintVerbose())
    printf("Curtail %s:\n", a->p->n->name.c_str());

  for (f = a->fsm; f; f = f->nxt) {
    if (!f->seen || (f->scratch & (1 | 2)))
      continue;

    isrel = haselse = i = blocking = 0;

    for (t = f->t; t; t = t->nxt) {
      g = fsm_tbl[t->to];

      isrel |= (t->relevant & 1); /* data relevant */
      i += g->seen;

      if (t->step && t->step->n) {
        switch (t->step->n->node_type) {
        case IF:
        case DO:
          haselse |= see_else(g);
          break;
        case 'c':
        case 's':
        case 'r':
          blocking = 1;
          break;
        }
      }
    }
    if (isrel                   /* 3. */
        || (i == 1 && blocking) /* 2. */
        || (i > 1 && !haselse)) /* 1. */
    {
      if (!is_guard(f)) {
        f->scratch |= 1;
        if (verbose_flags.NeedToPrintVerbose())
          printf("scratch %d -- %d %d %d %d\n", f->from, i, isrel, blocking,
                 haselse);
      }
    }
  }
}

static void init_dom(AST *a) {
  FSM_state *f;
  int i, j, cnt;
#if 0
	(1)  D(s0) = {s0}
	(2)  for s in S - {s0} do D(s) = S
#endif

  for (f = a->fsm; f; f = f->nxt) {
    if (!f->seen)
      continue;

    f->dom = (ulong *)emalloc(a->nwords * sizeof(ulong));

    if (f->from == a->i_st) {
      i = a->i_st / BPW;
      j = a->i_st % BPW;    /* assert(j <= 32); */
      f->dom[i] = (1 << j); /* (1) */
    } else                  /* (2) */
    {
      for (i = 0; i < a->nwords; i++) {
        f->dom[i] = (ulong)~0; /* all 1's */
      }
      if (a->nstates % BPW)
        for (i = (a->nstates % BPW); i < (int)BPW; i++) {
          f->dom[a->nwords - 1] &= ~(1 << ((ulong)i)); /* clear tail */
        }
      for (cnt = 0; cnt < a->nstates; cnt++) {
        if (!fsm_tbl[cnt]->seen) {
          i = cnt / BPW;
          j = cnt % BPW; /* assert(j <= 32); */
          f->dom[i] &= ~(1 << ((ulong)j));
        }
      }
    }
  }
}

static int dom_perculate(AST *a, FSM_state *f) {
  static ulong *ndom = (ulong *)0;
  static int on = 0;
  int i, j, cnt = 0;
  FSM_state *g;
  FSM_trans *t;

  if (on < a->nwords) {
    on = a->nwords;
    ndom = (ulong *)emalloc(on * sizeof(ulong));
  }

  for (i = 0; i < a->nwords; i++)
    ndom[i] = (ulong)~0;

  for (t = f->p; t; t = t->nxt) /* all reachable predecessors */
  {
    g = fsm_tbl[t->to];
    if (g->seen)
      for (i = 0; i < a->nwords; i++)
        ndom[i] &= g->dom[i]; /* (5b) */
  }

  i = f->from / BPW;
  j = f->from % BPW;   /* assert(j <= 32); */
  ndom[i] |= (1 << j); /* (5a) */

  for (i = 0; i < a->nwords; i++)
    if (f->dom[i] != ndom[i]) {
      cnt++;
      f->dom[i] = ndom[i];
    }

  return cnt;
}

static void dom_forward(AST *a) {
  FSM_state *f;
  int cnt;

  init_dom(a); /* (1,2) */
  do {
    cnt = 0;
    for (f = a->fsm; f; f = f->nxt) {
      if (f->seen && f->from != a->i_st) /* (4) */
        cnt += dom_perculate(a, f);      /* (5) */
    }
  } while (cnt); /* (3) */
  dom_perculate(a, fsm_tbl[a->i_st]);
}

static void AST_dominant(void) {
  FSM_state *f;
  FSM_trans *t;
  AST *a;
  int oi;
  static FSM_state no_state;
  auto &verbose_flags = utils::verbose::Flags::getInstance();
#if 0
	find dominators
	Aho, Sethi, & Ullman, Compilers - principles, techniques, and tools
	Addison-Wesley, 1986, p.671.

	(1)  D(s0) = {s0}
	(2)  for s in S - {s0} do D(s) = S

	(3)  while any D(s) changes do
	(4)    for s in S - {s0} do
	(5)	D(s) = {s} union  with intersection of all D(p)
		where p are the immediate predecessors of s

	the purpose is to find proper subgraphs
	(one entry node, one exit node)
#endif
  if (AST_Round == 1) /* computed once, reused in every round */
    for (a = ast; a; a = a->nxt) {
      a->nstates = 0;
      for (f = a->fsm; f; f = f->nxt) {
        a->nstates++;         /* count */
        fsm_tbl[f->from] = f; /* fast lookup */
        f->scratch = 0;       /* clear scratch marks */
      }
      for (oi = 0; oi < a->nstates; oi++)
        if (!fsm_tbl[oi])
          fsm_tbl[oi] = &no_state;

      a->nwords = (a->nstates + BPW - 1) / BPW; /* round up */

      if (verbose_flags.NeedToPrintVerbose()) {
        printf("%s (%d): ", a->p->n->name.c_str(), a->i_st);
        printf("states=%d (max %d), words = %d, bpw %d, overflow %d\n",
               a->nstates, o_max, a->nwords, (int)BPW, (int)(a->nstates % BPW));
      }

      reachability(a);
      dom_forward(a); /* forward dominance relation */

      curtail(a); /* mark ineligible edges */
      for (f = a->fsm; f; f = f->nxt) {
        t = f->p;
        f->p = f->t;
        f->t = t; /* invert edges */

        f->mod = f->dom;
        f->dom = (ulong *)0;
      }
      oi = a->i_st;
      if (fsm_tbl[0]->seen) /* end-state reachable - else leave it */
        a->i_st = 0;        /* becomes initial state */

      dom_forward(a);    /* reverse dominance -- don't redo reachability! */
      act_dom(a);        /* mark proper subgraphs, if any */
      AST_checkpairs(a); /* selectively place 2 scratch-marks */

      for (f = a->fsm; f; f = f->nxt) {
        t = f->p;
        f->p = f->t;
        f->t = t; /* restore */
      }
      a->i_st = oi; /* restore */
    }
  else
    for (a = ast; a; a = a->nxt) {
      for (f = a->fsm; f; f = f->nxt) {
        fsm_tbl[f->from] = f;
        f->scratch &= 1; /* preserve 1-marks */
      }
      for (oi = 0; oi < a->nstates; oi++)
        if (!fsm_tbl[oi])
          fsm_tbl[oi] = &no_state;

      curtail(a); /* mark ineligible edges */

      for (f = a->fsm; f; f = f->nxt) {
        t = f->p;
        f->p = f->t;
        f->t = t; /* invert edges */
      }

      AST_checkpairs(a); /* recompute 2-marks */

      for (f = a->fsm; f; f = f->nxt) {
        t = f->p;
        f->p = f->t;
        f->t = t; /* restore */
      }
    }
}
