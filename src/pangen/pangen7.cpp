/***** spin: pangen7.c *****/

#include "../fatal/fatal.hpp"
#include "../spin.hpp"
#include "../utils/verbose/verbose.hpp"
#include "y.tab.h"
#include <assert.h>
#include <stdlib.h>
#ifndef PC
#include <unistd.h>
#endif
#include "../main/launch_settings.hpp"
#include <iostream>

extern LaunchSettings launch_settings;

extern models::ProcList *ready;
extern models::Element *Al_El;
extern int nclaims, verbose;
extern short has_accept;

struct Succ_List;
struct SQueue;
struct OneState;
struct State_Stack;
struct Guard;

struct Succ_List {
  SQueue *s;
  Succ_List *next;
};

struct OneState {
  int *combo;      /* the combination of claim states */
  Succ_List *succ; /* list of ptrs to immediate successor states */
};

struct SQueue {
  OneState state;
  SQueue *next;
};

struct State_Stack {
  int *n;
  State_Stack *next;
};

struct Guard {
  models::Lextok *t;
  Guard *next;
};

static SQueue *sq, *sd,
    *render; /* states move from sq to sd to render to holding */
static SQueue *holding, *lasthold;
static State_Stack *dsts;

static int nst;       /* max nr of states in claims */
static int *Ist;      /* initial states */
static int *Nacc;     /* number of accept states in claim */
static int *Nst;      /* next states */
static int **reached; /* n claims x states */
static int unfolding; /* to make sure all accept states are reached */
static int
    is_accept; /* remember if the current state is accepting in any claim */
static int not_printing; /* set during explore_product */

static models::Element ****matrix; /* n x two-dimensional arrays state x state */
static models::Element **Selfs;    /* self-loop states at end of claims */

static void get_seq(int, models::Sequence *);
static void set_el(int n, models::Element *e);
static void gen_product(void);
static void print_state_nm(char *, int *, char *);
static SQueue *find_state(int *);
static SQueue *retrieve_state(int *);

static int same_state(int *a, int *b) {
  int i;

  for (i = 0; i < nclaims; i++) {
    if (a[i] != b[i]) {
      return 0;
    }
  }
  return 1;
}

static int in_stack(SQueue *s, SQueue *in) {
  SQueue *q;

  for (q = in; q; q = q->next) {
    if (same_state(q->state.combo, s->state.combo)) {
      return 1;
    }
  }
  return 0;
}

static void to_render(SQueue *s) {
  auto &verbose_flags = utils::verbose::Flags::getInstance();
  SQueue *a, *q,
      *last; /* find in sd/sq and move to render, if not already there */
  int n;

  for (n = 0; n < nclaims; n++) {
    reached[n][s->state.combo[n]] |= 2;
  }

  for (q = render; q; q = q->next) {
    if (same_state(q->state.combo, s->state.combo)) {
      return;
    }
  }
  for (q = holding; q; q = q->next) {
    if (same_state(q->state.combo, s->state.combo)) {
      return;
    }
  }

  a = sd;
more:
  for (q = a, last = 0; q; last = q, q = q->next) {
    if (same_state(q->state.combo, s->state.combo)) {
      if (!last) {
        if (a == sd) {
          sd = q->next;
        } else if (a == sq) {
          sq = q->next;
        } else {
          holding = q->next;
        }
      } else {
        last->next = q->next;
      }
      q->next = render;
      render = q;
      return;
    }
  }
  if (verbose_flags.Active()) {
    print_state_nm("looking for: ", s->state.combo, "\n");
  }
  (void)find_state(s->state.combo); /* creates it in sq */
  if (a != sq) {
    a = sq;
    goto more;
  }
  loger::fatal("cannot happen, to_render");
}

static void wrap_text(char *pre, models::Lextok *t, char *post) {
  std::cout << pre;
  comment(stdout, t, 0);
  std::cout << post;
}

static State_Stack *push_dsts(int *n) {
  State_Stack *s;
  int i;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  for (s = dsts; s; s = s->next) {
    if (same_state(s->n, n)) {
      if (verbose_flags.NeedToPrintVeryVerbose()) {
        printf("\n");
        for (s = dsts; s; s = s->next) {
          print_state_nm("\t", s->n, "\n");
        }
        print_state_nm("\t", n, "\n");
      }
      return s;
    }
  }

  s = (State_Stack *)emalloc(sizeof(State_Stack));
  s->n = (int *)emalloc(nclaims * sizeof(int));
  for (i = 0; i < nclaims; i++)
    s->n[i] = n[i];
  s->next = dsts;
  dsts = s;
  return 0;
}

static void pop_dsts(void) {
  assert(dsts != NULL);
  dsts = dsts->next;
}

static void complete_transition(Succ_List *sl, Guard *g) {
  Guard *w;
  int cnt = 0;

  printf("	:: ");
  for (w = g; w; w = w->next) {
    if (w->t->node_type == CONST && w->t->value == 1) {
      continue;
    } else if (w->t->node_type == 'c' && w->t->left->node_type == CONST &&
               w->t->left->value == 1) {
      continue; /* 'true' */
    }

    if (cnt > 0) {
      printf(" && ");
    }
    wrap_text("", w->t, "");
    cnt++;
  }
  if (cnt == 0) {
    printf("true");
  }
  print_state_nm(" -> goto ", sl->s->state.combo, "");

  if (is_accept > 0) {
    printf("_U%d\n", (unfolding + 1) % nclaims);
  } else {
    printf("_U%d\n", unfolding);
  }
}

static void state_body(OneState *s, Guard *guard) {
  Succ_List *sl;
  State_Stack *y;
  Guard *g;
  int i, once;

  for (sl = s->succ; sl; sl = sl->next) {
    once = 0;

    for (i = 0; i < nclaims; i++) {
      models::Element *e;
      e = matrix[i][s->combo[i]][sl->s->state.combo[i]];

      /* if one of the claims has a DO or IF move
         then pull its target state forward, once
       */

      if (!e || e->n->node_type == NON_ATOMIC || e->n->node_type == DO ||
          e->n->node_type == IF) {
        s = &(sl->s->state);
        y = push_dsts(s->combo);
        if (!y) {
          if (once++ == 0) {
            assert(s->succ != NULL);
            state_body(s, guard);
          }
          pop_dsts();
        } else if (!y->next) /* self-loop transition */
        {
          if (!not_printing)
            printf(" /* self-loop */\n");
        } else { /* loger::non_fatal("loop in state body", 0); ** maybe ok */
        }
        continue;
      } else {
        g = (Guard *)emalloc(sizeof(Guard));
        g->t = e->n;
        g->next = guard;
        guard = g;
      }
    }

    if (guard && !once) {
      if (!not_printing)
        complete_transition(sl, guard);
      to_render(sl->s);
    }
  }
}

static struct X_tbl {
  char *s;
  int n;
} spl[] = {
    {"end", 3},
    {"accept", 6},
    {0, 0},
};

static int slcnt;
extern models::Label *labtab;

static models::ProcList *locate_claim(int n) {
  models::ProcList *p;
  int i;

  for (p = ready, i = 0; p; p = p->next, i++) /* find claim name */
  {
    if (i == n) {
      break;
    }
  }
  assert(p && p->b == models::btypes::N_CLAIM);

  return p;
}

static void elim_lab(models::Element *e) {
  models::Label *l, *lst;

  for (l = labtab, lst = NULL; l; lst = l, l = l->next) {
    if (l->e == e) {
      if (lst) {
        lst->next = l->next;
      } else {
        labtab = l->next;
      }
      break;
    }
  }
}

static int claim_has_accept(models::ProcList *p) {
  models::Label *l;

  for (l = labtab; l; l = l->next) {
    if (l->c->name == p->n->name && l->s->name.substr(0, 6) == "accept") {
      return 1;
    }
  }
  return 0;
}

static void prune_accept(void) {
  int n;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  for (n = 0; n < nclaims; n++) {
    if ((reached[n][Selfs[n]->seqno] & 2) == 0) {
      if (verbose_flags.Active()) {
        printf("claim %d: selfloop not reachable\n", n);
      }
      elim_lab(Selfs[n]);
      Nacc[n] = claim_has_accept(locate_claim(n));
    }
  }
}

static void mk_accepting(int n, models::Element *e) {
  models::ProcList *p;
  models::Label *l;
  int i;

  assert(!Selfs[n]);
  Selfs[n] = e;

  l = (models::Label *)emalloc(sizeof(models::Label));
  l->s = (models::Symbol *)emalloc(sizeof(models::Symbol));
  l->s->name = "accept00";
  l->c = (models::Symbol *)emalloc(sizeof(models::Symbol));
  l->opt_inline_id = 0; /* this is not in an inline */

  for (p = ready, i = 0; p; p = p->next, i++) /* find claim name */
  {
    if (i == n) {
      l->c->name = p->n->name;
      break;
    }
  }
  assert(p && p->b == models::btypes::N_CLAIM);
  Nacc[n] = 1;
  has_accept = 1;

  l->e = e;
  l->next = labtab;
  labtab = l;
}

static void check_special(int *nrs) {
  models::ProcList *p;
  models::Label *l;
  int i, j, nmatches;
  int any_accepts = 0;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  for (i = 0; i < nclaims; i++) {
    any_accepts += Nacc[i];
  }

  is_accept = 0;
  for (j = 0; spl[j].n; j++) /* 2 special label prefixes */
  {
    nmatches = 0;
    for (p = ready, i = 0; p; p = p->next, i++) /* check each claim */
    {
      if (p->b != models::btypes::N_CLAIM) {
        continue;
      }
      /* claim i in state nrs[i], type p->tn, name p->n->name
       * either the state has an accept label, or the claim has none,
       * so that all its states should be considered accepting
       * --- but only if other claims do have accept states!
       */
      if (!launch_settings.need_use_strict_lang_intersection && j == 1 &&
          Nacc[i] == 0 && any_accepts > 0) {
        if (verbose_flags.NeedToPrintVerbose() && i == unfolding) {
          printf("	/* claim %d pseudo-accept */\n", i);
        }
        goto is_accepting;
      }
      for (l = labtab; l; l = l->next) /* check its labels */
      {
        if (l->c->name == p->n->name /* right claim */
            && l->e->seqno == nrs[i] /* right state */
            && l->s->name.substr(0, spl[j].n) == std::string(spl[j].s)) {
          if (j == 1) /* accept state */
          {
          is_accepting:
            std::string buf;
            if (strchr(p->n->name.c_str(), ':')) {
              buf = "N" + std::to_string(i);
            } else {
              assert(p->n->name.length() < 32);
              buf = p->n->name;
            }

            if (unfolding == 0 && i == 0) {
              if (!not_printing)
                printf("%s_%s_%d:\n", /* true accept */
                       spl[j].s, buf.c_str(), slcnt++);
            } else if (verbose_flags.NeedToPrintVerbose()) {
              if (!not_printing)
                printf("%s_%s%d:\n", buf.c_str(), spl[j].s, slcnt++);
            }
            if (i == unfolding) {
              is_accept++; /* move to next unfolding */
            }
          } else {
            nmatches++;
          }
          break;
        }
      }
    }
    if (j == 0 && nmatches == nclaims) /* end-state */
    {
      if (!not_printing) {
        printf("%s%d:\n", spl[j].s, slcnt++);
      }
    }
  }
}

static int render_state(SQueue *q) {
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  if (!q || !q->state.succ) {
    if (verbose_flags.NeedToPrintVeryVerbose()) {
      printf("	no exit\n");
    }
    return 0;
  }

  check_special(q->state.combo); /* accept or end-state labels */

  dsts = (State_Stack *)0;
  push_dsts(q->state.combo); /* to detect loops */

  if (!not_printing) {
    print_state_nm("", q->state.combo, ""); /* the name */
    printf("_U%d:\n\tdo\n", unfolding);
  }

  state_body(&(q->state), (Guard *)0);

  if (!not_printing) {
    printf("\tod;\n");
  }
  pop_dsts();
  return 1;
}

static void explore_product(void) {
  SQueue *q;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  /* all states are in the sd queue */

  q = retrieve_state(Ist); /* retrieve from the sd q */
  q->next = render;         /* put in render q */
  render = q;
  do {
    q = render;
    render = render->next;
    q->next = 0; /* remove from render q */

    if (verbose_flags.NeedToPrintVeryVerbose()) {
      print_state_nm("explore: ", q->state.combo, "\n");
    }

    not_printing = 1;
    render_state(q); /* may add new states */
    not_printing = 0;

    if (lasthold) {
      lasthold->next = q;
      lasthold = q;
    } else {
      holding = lasthold = q;
    }
  } while (render);
  assert(!dsts);
}

static void print_product(void) {
  SQueue *q;
  int cnt;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  if (unfolding == 0) {
    printf("never Product {\n"); /* name expected by iSpin */
    q = find_state(Ist);         /* should find it in the holding q */
    assert(q != NULL);
    q->next = holding; /* put it at the front */
    holding = q;
  }
  render = holding;
  holding = lasthold = 0;

  printf("/* ============= U%d ============= */\n", unfolding);
  cnt = 0;
  do {
    q = render;
    render = render->next;
    q->next = 0;
    if (verbose_flags.NeedToPrintVeryVerbose()) {
      print_state_nm("print: ", q->state.combo, "\n");
    }
    cnt += render_state(q);

    if (lasthold) {
      lasthold->next = q;
      lasthold = q;
    } else {
      holding = lasthold = q;
    }
  } while (render);
  assert(!dsts);

  if (cnt == 0) {
    printf("	0;\n");
  }

  if (unfolding == nclaims - 1) {
    printf("}\n");
  }
}

static void prune_dead(void) {
  Succ_List *sl, *last;
  SQueue *q;
  int cnt;

  do {
    cnt = 0;
    for (q = sd; q;
         q = q->next) { /* if successor is deadend, remove it
                        * unless it's a move to the end-state of the claim
                        */
      last = (Succ_List *)0;
      for (sl = q->state.succ; sl; last = sl, sl = sl->next) {
        if (!sl->s->state.succ) /* no successor */
        {
          if (!last) {
            q->state.succ = sl->next;
          } else {
            last->next = sl->next;
          }
          cnt++;
        }
      }
    }
  } while (cnt > 0);
}

static void print_raw(void) {
  int i, j, n;

  printf("#if 0\n");
  for (n = 0; n < nclaims; n++) {
    printf("C%d:\n", n);
    for (i = 0; i < nst; i++) {
      if (reached[n][i])
        for (j = 0; j < nst; j++) {
          if (matrix[n][i][j]) {
            if (reached[n][i] & 2)
              printf("+");
            if (i == Ist[n])
              printf("*");
            printf("\t%d", i);
            wrap_text(" -[", matrix[n][i][j]->n, "]->\t");
            printf("%d\n", j);
          }
        }
    }
  }
  printf("#endif\n\n");
  fflush(stdout);
}

void sync_product(void) {
  models::ProcList *p;
  models::Element *e;
  int n, i;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  if (nclaims <= 1)
    return;

  (void)unlink("pan.pre");

  Ist = (int *)emalloc(sizeof(int) * nclaims);
  Nacc = (int *)emalloc(sizeof(int) * nclaims);
  Nst = (int *)emalloc(sizeof(int) * nclaims);
  reached = (int **)emalloc(sizeof(int *) * nclaims);
  Selfs = (models::Element **)emalloc(sizeof(models::Element *) * nclaims);
  matrix = (models::Element ****)emalloc(sizeof(models::Element ***) * nclaims); /* claims */

  for (p = ready, i = 0; p; p = p->next, i++) {
    if (p->b == models::btypes::N_CLAIM) {
      nst = max(p->s->maxel, nst);
      Nacc[i] = claim_has_accept(p);
    }
  }

  for (n = 0; n < nclaims; n++) {
    reached[n] = (int *)emalloc(sizeof(int) * nst);
    matrix[n] = (models::Element ***)emalloc(sizeof(models::Element **) * nst); /* rows */
    for (i = 0; i < nst; i++)                                   /* cols */
    {
      matrix[n][i] = (models::Element **)emalloc(sizeof(models::Element *) * nst);
    }
  }

  for (e = Al_El; e; e = e->Nxt) {
    e->status &= ~DONE;
  }

  for (p = ready, n = 0; p; p = p->next, n++) {
    if (p->b == models::btypes::N_CLAIM) { /* fill in matrix[n] */
      e = p->s->frst;
      Ist[n] = huntele(e, e->status, -1)->seqno;

      reached[n][Ist[n]] = 1 | 2;
      get_seq(n, p->s);
    }
  }

  if (verbose_flags.Active()) /* show only the input automata */
  {
    print_raw();
  }

  gen_product(); /* create product automaton */
}

static int nxt_trans(int n, int cs, int frst) {
  int j;

  for (j = frst; j < nst; j++) {
    if (reached[n][cs] && matrix[n][cs][j]) {
      return j;
    }
  }
  return -1;
}

static void print_state_nm(char *p, int *s, char *a) {
  int i;
  printf("%sP", p);
  for (i = 0; i < nclaims; i++) {
    printf("_%d", s[i]);
  }
  printf("%s", a);
}

static void create_transition(OneState *s, SQueue *it) {
  int n, from, upto;
  int *F = s->combo;
  int *T = it->state.combo;
  Succ_List *sl;
  models::Lextok *t;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  if (verbose_flags.NeedToPrintVeryVerbose()) {
    print_state_nm("", F, " ");
    print_state_nm("-> ", T, "\t");
  }

  /* check if any of the claims is blocked */
  /* which makes the state a dead-end */
  for (n = 0; n < nclaims; n++) {
    from = F[n];
    upto = T[n];
    t = matrix[n][from][upto]->n;
    if (verbose_flags.NeedToPrintVeryVerbose()) {
      wrap_text("", t, " ");
    }
    if (t->node_type == 'c' && t->left->node_type == CONST) {
      if (t->left->value == 0) /* i.e., false */
      {
        goto done;
      }
    }
  }

  sl = (Succ_List *)emalloc(sizeof(Succ_List));
  sl->s = it;
  sl->next = s->succ;
  s->succ = sl;
done:
  if (verbose_flags.NeedToPrintVeryVerbose()) {
    printf("\n");
  }
}

static SQueue *find_state(int *cs) {
  SQueue *nq, *a = sq;
  int i;

again: /* check in nq, sq, and then in the render q */
  for (nq = a; nq; nq = nq->next) {
    if (same_state(nq->state.combo, cs)) {
      return nq; /* found */
    }
  }
  if (a == sq && sd) {
    a = sd;
    goto again; /* check the other stack too */
  } else if (a == sd && render) {
    a = render;
    goto again;
  }

  nq = (SQueue *)emalloc(sizeof(SQueue));
  nq->state.combo = (int *)emalloc(nclaims * sizeof(int));
  for (i = 0; i < nclaims; i++) {
    nq->state.combo[i] = cs[i];
  }
  nq->next = sq; /* add to sq stack */
  sq = nq;

  return nq;
}

static SQueue *retrieve_state(int *s) {
  SQueue *nq, *last = NULL;

  for (nq = sd; nq; last = nq, nq = nq->next) {
    if (same_state(nq->state.combo, s)) {
      if (last) {
        last->next = nq->next;
      } else {
        sd = nq->next; /* 6.4.0: was sd = nq */
      }
      return nq; /* found */
    }
  }

  loger::fatal("cannot happen: retrieve_state");
  return (SQueue *)0;
}

static void all_successors(int n, OneState *cur) {
  int i, j = 0;

  if (n >= nclaims) {
    create_transition(cur, find_state(Nst));
  } else {
    i = cur->combo[n];
    for (;;) {
      j = nxt_trans(n, i, j);
      if (j < 0)
        break;
      Nst[n] = j;
      all_successors(n + 1, cur);
      j++;
    }
  }
}

static void gen_product(void) {
  OneState *cur_st;
  SQueue *q;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  find_state(Ist); /* create initial state */

  while (sq) {
    if (in_stack(sq, sd)) {
      sq = sq->next;
      continue;
    }
    cur_st = &(sq->state);

    q = sq;
    sq = sq->next; /* delete from sq stack */
    q->next = sd;  /* and move to done stack */
    sd = q;

    all_successors(0, cur_st);
  }
  /* all states are in the sd queue now */
  prune_dead();
  explore_product(); /* check if added accept-self-loops are reachable */
  prune_accept();

  if (verbose_flags.Active()) {
    print_raw();
  }

  /* PM: merge states with identical successor lists */

  /* all outgoing transitions from accept-states
     from claim n in copy n connect to states in copy (n+1)%nclaims
     only accept states from claim 0 in copy 0 are true accept states
     in the product

     PM: what about claims that have no accept states (e.g., restrictions)
  */

  for (unfolding = 0; unfolding < nclaims; unfolding++) {
    print_product();
  }
}

static void t_record(int n, models::Element *e, models::Element *g) {
  int from = e->seqno, upto = g ? g->seqno : 0;

  assert(from >= 0 && from < nst);
  assert(upto >= 0 && upto < nst);

  matrix[n][from][upto] = e;
  reached[n][upto] |= 1;
}

static void get_sub(int n, models::Element *e) {
  if (e->n->node_type == D_STEP || e->n->node_type == ATOMIC) {
    loger::fatal("atomic or d_step in never claim product");
  }
  /* NON_ATOMIC */
  e->n->seq_list->this_sequence->last->next = e->next;
  get_seq(n, e->n->seq_list->this_sequence);

  t_record(n, e, e->n->seq_list->this_sequence->frst);
}

static void set_el(int n, models::Element *e) {
  models::Element *g;

  if (e->n->node_type == '@') /* change to self-loop */
  {
    e->n->node_type = CONST;
    e->n->value = 1; /* true */
    e->next = e;
    g = e;
    mk_accepting(n, e);
  } else

      if (e->n->node_type == GOTO) {
    g = get_lab(e->n, 1);
    g = huntele(g, e->status, -1);
  } else if (e->next) {
    g = huntele(e->next, e->status, -1);
  } else {
    g = NULL;
  }

  t_record(n, e, g);
}

static void get_seq(int n, models::Sequence *s) {
  models::SeqList *h;
  models::Element *e;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  e = huntele(s->frst, s->frst->status, -1);
  for (; e; e = e->next) {
    if (e->status & DONE) {
      goto checklast;
    }
    e->status |= DONE;

    if (e->n->node_type == UNLESS) {
      loger::fatal("unless stmnt in never claim product");
    }

    if (e->sub) /* IF or DO */
    {
      models::Lextok *x = NULL;
      models::Lextok *y = NULL;
      models::Lextok *haselse = NULL;

      for (h = e->sub; h; h = h->next) {
        models::Lextok *t = h->this_sequence->frst->n;
        if (t->node_type == ELSE) {
          if (verbose_flags.NeedToPrintVeryVerbose())
            printf("else at line %d\n", t->line_number);
          haselse = t;
          continue;
        }
        if (t->node_type != 'c') {
          loger::fatal("product, 'else' combined with non-condition");
        }

        if (t->left->node_type == CONST /* true */
            && t->left->value == 1 && y == NULL) {
          y = nn(ZN, CONST, ZN, ZN);
          y->value = 0;
        } else {
          if (!x)
            x = t;
          else
            x = nn(ZN, OR, x, t);
          if (verbose_flags.NeedToPrintVeryVerbose()) {
            wrap_text(" [", x, "]\n");
          }
        }
      }
      if (haselse) {
        if (!y) {
          y = nn(ZN, '!', x, ZN);
        }
        if (verbose_flags.NeedToPrintVeryVerbose()) {
          wrap_text(" [else: ", y, "]\n");
        }
        haselse->node_type = 'c'; /* replace else */
        haselse->left = y;
      }

      for (h = e->sub; h; h = h->next) {
        t_record(n, e, h->this_sequence->frst);
        get_seq(n, h->this_sequence);
      }
    } else {
      if (e->n->node_type == ATOMIC || e->n->node_type == D_STEP ||
          e->n->node_type == NON_ATOMIC) {
        get_sub(n, e);
      } else {
        set_el(n, e);
      }
    }
  checklast:
    if (e == s->last)
      break;
  }
}
