/***** spin: mesg.c *****/

#include "fatal/fatal.hpp"
#include "main/launch_settings.hpp"
#include "spin.hpp"
#include "utils/verbose/verbose.hpp"
#include "y.tab.h"
#include <assert.h>
#include <fmt/core.h>
#include <stdlib.h>

#ifndef MAXQ
#define MAXQ 2500 /* default max # queues  */
#endif

extern RunList *X_lst;
extern models::Symbol *Fname;
extern int TstOnly;
extern int lineno, depth;
extern int nproc, nstop;
extern short Have_claim;

extern LaunchSettings launch_settings;

QH *qh_lst;
Queue *qtab = (Queue *)0; /* linked list of queues */
Queue *ltab[MAXQ];        /* linear list of queues */
int nrqs = 0, firstrow = 1, has_stdin = 0;
char GBuf[4096];

static Lextok *n_rem = (Lextok *)0;
static Queue *q_rem = (Queue *)0;

static int a_rcv(Queue *, Lextok *, int);
static int a_snd(Queue *, Lextok *);
static int sa_snd(Queue *, Lextok *);
static int s_snd(Queue *, Lextok *);
extern Lextok **find_mtype_list(const std::string &);
extern std::string which_mtype(const std::string &);
extern void sr_buf(int, int, const std::string &);
extern void sr_mesg(FILE *, int, int, const std::string &);
extern void putarrow(int, int);
static void sr_talk(Lextok *, int, char *, char *, int, Queue *);

int cnt_mpars(Lextok *n) {
  Lextok *m;
  int i = 0;

  for (m = n; m; m = m->rgt)
    i += Cnt_flds(m);
  return i;
}

int qmake(models::Symbol *s) {
  Lextok *m;
  Queue *q;
  int i, j;

  if (!s->init_value)
    return 0;

  if (nrqs >= MAXQ) {
    lineno = s->init_value->ln;
    Fname = s->init_value->fn;
    loger::fatal("too many queues (%s)", s->name);
  }
  if (launch_settings.need_to_analyze && nrqs >= 255) {
    loger::fatal("too many channel types");
  }

  if (s->init_value->ntyp != CHAN)
    return eval(s->init_value);

  q = (Queue *)emalloc(sizeof(Queue));
  q->qid = (short)++nrqs;
  q->nslots = s->init_value->val;
  q->nflds = cnt_mpars(s->init_value->rgt);
  q->setat = depth;

  i = max(1, q->nslots); /* 0-slot qs get 1 slot minimum */
  j = q->nflds * i;

  q->contents = (int *)emalloc(j * sizeof(int));
  q->fld_width = (int *)emalloc(q->nflds * sizeof(int));
  q->mtp = (char **)emalloc(q->nflds * sizeof(char *));
  q->stepnr = (int *)emalloc(i * sizeof(int));

  for (m = s->init_value->rgt, i = 0; m; m = m->rgt) {
    if (m->sym && m->ntyp == STRUCT) {
      i = Width_set(q->fld_width, i, getuname(m->sym));
    } else {
      if (m->sym) {
        q->mtp[i] = m->sym->name.data();
      }
      q->fld_width[i++] = m->ntyp;
    }
  }
  q->nxt = qtab;
  qtab = q;
  ltab[q->qid - 1] = q;

  return q->qid;
}

int qfull(Lextok *n) {
  int whichq = eval(n->lft) - 1;

  if (whichq < MAXQ && whichq >= 0 && ltab[whichq])
    return (ltab[whichq]->qlen >= ltab[whichq]->nslots);
  return 0;
}

int qlen(Lextok *n) {
  int whichq = eval(n->lft) - 1;

  if (whichq < MAXQ && whichq >= 0 && ltab[whichq])
    return ltab[whichq]->qlen;
  return 0;
}

int q_is_sync(Lextok *n) {
  int whichq = eval(n->lft) - 1;

  if (whichq < MAXQ && whichq >= 0 && ltab[whichq])
    return (ltab[whichq]->nslots == 0);
  return 0;
}

int qsend(Lextok *n) {
  int whichq = eval(n->lft) - 1;

  if (whichq == -1) {
    printf("Error: sending to an uninitialized chan\n");
    /* whichq = 0; */
    return 0;
  }
  if (whichq < MAXQ && whichq >= 0 && ltab[whichq]) {
    ltab[whichq]->setat = depth;
    if (ltab[whichq]->nslots > 0) {
      return a_snd(ltab[whichq], n);
      ;
    } else {
      return s_snd(ltab[whichq], n);
    }
  }
  return 0;
}

#ifndef PC
#include <termios.h>
static struct termios initial_settings, new_settings;

void peek_ch_init(void) {
  tcgetattr(0, &initial_settings);

  new_settings = initial_settings;
  new_settings.c_lflag &= ~ICANON;
  new_settings.c_lflag &= ~ECHO;
  new_settings.c_lflag &= ~ISIG;
  new_settings.c_cc[VMIN] = 0;
  new_settings.c_cc[VTIME] = 0;
}

int peek_ch(void) {
  int n;

  has_stdin = 1;

  tcsetattr(0, TCSANOW, &new_settings);
  n = getchar();
  tcsetattr(0, TCSANOW, &initial_settings);

  return n;
}
#endif

int qrecv(Lextok *n, int full) {
  int whichq = eval(n->lft) - 1;

  if (whichq == -1) {
    if (n->sym && n->sym->name != "STDIN") {
      Lextok *m;
#ifndef PC
      static int did_once = 0;
      if (!did_once) /* 6.2.4 */
      {
        peek_ch_init();
        did_once = 1;
      }
#endif
      if (TstOnly)
        return 1;

      for (m = n->rgt; m; m = m->rgt)
        if (m->lft->ntyp != CONST && m->lft->ntyp != EVAL) {
#ifdef PC
          int c = getchar();
#else
          int c = peek_ch(); /* 6.2.4, was getchar(); */
#endif
          if (c == 27 || c == 3) /* escape or control-c */
          {
            printf("quit\n");
            exit(0);
          } /* else: non-blocking */
          if (c == EOF)
            return 0; /* no char available */
          (void)setval(m->lft, c);
        } else {
          loger::fatal("invalid use of STDIN");
        }
      return 1;
    }
    printf("Error: receiving from an uninitialized chan %s\n",
           n->sym ? n->sym->name.c_str() : "");
    /* whichq = 0; */
    return 0;
  }
  if (whichq < MAXQ && whichq >= 0 && ltab[whichq]) {
    ltab[whichq]->setat = depth;
    return a_rcv(ltab[whichq], n, full);
  }
  return 0;
}

static int sa_snd(Queue *q, Lextok *n) /* sorted asynchronous */
{
  Lextok *m;
  int i, j, k;
  int New, Old;

  for (i = 0; i < q->qlen; i++)
    for (j = 0, m = n->rgt; m && j < q->nflds; m = m->rgt, j++) {
      New = cast_val(q->fld_width[j], eval(m->lft), 0);
      Old = q->contents[i * q->nflds + j];
      if (New == Old)
        continue;
      if (New > Old)
        break;    /* inner loop */
      goto found; /* New < Old */
    }
found:
  for (j = q->qlen - 1; j >= i; j--)
    for (k = 0; k < q->nflds; k++) {
      q->contents[(j + 1) * q->nflds + k] =
          q->contents[j * q->nflds + k]; /* shift up */
      if (k == 0)
        q->stepnr[j + 1] = q->stepnr[j];
    }
  return i * q->nflds; /* new q offset */
}

void typ_ck(int ft, int at, const std::string &s) {
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  if (verbose_flags.NeedToPrintVerbose() && ft != at &&
      (ft == CHAN || at == CHAN) && (at != PREDEF || s != "recv")) {
    std::string buf, tag1, tag2;
    sputtype(tag1, ft);
    sputtype(tag2, at);
    buf = "type-clash in " + s + ", (" + tag1 + "<-> " + tag2 + ")";
    loger::non_fatal("%s", buf.c_str());
  }
}

static void mtype_ck(const std::string &p, Lextok *arg) {
  std::string t;
  std::string s = p;
  if (p.empty()) {
    s = "_unnamed_";
  }
  if (!arg || !arg->sym) {
    return;
  }

  switch (arg->ntyp) {
  case NAME:
    if (arg->sym->mtype_name) {
      t = arg->sym->mtype_name->name.data();
    } else {
      t = "_unnamed_";
    }
    break;
  case CONST:
    t = which_mtype(arg->sym->name);
    break;
  default:
    t = "expression";
    break;
  }

  if (s != t) {
    printf("spin: %s:%d, Error: '%s' is type '%s', but ",
           arg->fn ? arg->fn->name.c_str() : "", arg->ln,
           arg->sym->name.c_str(), t.c_str());
    printf("should be type '%s'\n", s.c_str());
    loger::non_fatal("incorrect type of '%s'", arg->sym->name.c_str());
  }
}

static int a_snd(Queue *q, Lextok *n) {
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  Lextok *m;
  int i = q->qlen * q->nflds; /* q offset */
  int j = 0;                  /* q field# */

  if (q->nslots > 0 && q->qlen >= q->nslots) {
    return launch_settings.need_lose_msgs_sent_to_full_queues; /* q is full */
  }

  if (TstOnly) {
    return 1;
  }
  if (n->val)
    i = sa_snd(q, n); /* sorted insert */

  q->stepnr[i / q->nflds] = depth;

  for (m = n->rgt; m && j < q->nflds; m = m->rgt, j++) {
    int New = eval(m->lft);
    q->contents[i + j] = cast_val(q->fld_width[j], New, 0);

    if (q->fld_width[i + j] == MTYPE) {
      mtype_ck(q->mtp[i + j], m->lft); /* 6.4.8 */
    }
    if (verbose_flags.NeedToPrintSends() &&
        depth >= launch_settings.count_of_skipping_steps) {
      sr_talk(n, New, "Send ", "->", j, q); /* XXX j was i+j in 6.4.8 */
    }
    typ_ck(q->fld_width[i + j], Sym_typ(m->lft), "send");
  }

  if (verbose_flags.NeedToPrintSends() &&
      depth >= launch_settings.count_of_skipping_steps) {
    for (i = j; i < q->nflds; i++) {
      sr_talk(n, 0, "Send ", "->", i, q);
    }
    if (j < q->nflds) {
      printf("%3d: warning: missing params in send\n", depth);
    }
    if (m) {
      printf("%3d: warning: too many params in send\n", depth);
    }
  }
  q->qlen++;
  return 1;
}

static int a_rcv(Queue *q, Lextok *n, int full) {
  auto &verbose_flags = utils::verbose::Flags::getInstance();
  Lextok *m;
  int i = 0, oi, j, k;
  extern int Rvous;

  if (q->qlen == 0) {
    return 0; /* q is empty */
  }
try_slot:
  /* test executability */
  for (m = n->rgt, j = 0; m && j < q->nflds; m = m->rgt, j++) {
    if (q->fld_width[i * q->nflds + j] == MTYPE) {
      mtype_ck(q->mtp[i * q->nflds + j], m->lft); /* 6.4.8 */
    }

    if (m->lft->ntyp == CONST && q->contents[i * q->nflds + j] != m->lft->val) {
      if (n->val == 0        /* fifo recv */
          || n->val == 2     /* fifo poll */
          || ++i >= q->qlen) /* last slot */
      {
        return 0; /* no match  */
      }
      goto try_slot; /* random recv */
    }

    if (m->lft->ntyp == EVAL) {
      Lextok *fix = m->lft->lft;

      if (fix->ntyp == ',') /* new, usertype7 */
      {
        do {
          assert(j < q->nflds);
          if (q->contents[i * q->nflds + j] != eval(fix->lft)) {
            if (n->val == 0 || n->val == 2 || ++i >= q->qlen) {
              return 0;
            }
            goto try_slot; /* random recv */
          }
          j++;
          fix = fix->rgt;
        } while (fix && fix->ntyp == ',');
        j--;
      } else {
        if (q->contents[i * q->nflds + j] != eval(fix)) {
          if (n->val == 0        /* fifo recv */
              || n->val == 2     /* fifo poll */
              || ++i >= q->qlen) /* last slot */
          {
            return 0; /* no match  */
          }
          goto try_slot; /* random recv */
        }
      }
    }
  }

  if (TstOnly)
    return 1;

  if (verbose_flags.NeedToPrintReceives()) {
    if (j < q->nflds) {
      printf("%3d: warning: missing params in next recv\n", depth);
    } else if (m) {
      printf("%3d: warning: too many params in next recv\n", depth);
    }
  }

  /* set the fields */
  if (Rvous) {
    n_rem = n;
    q_rem = q;
  }

  oi = q->stepnr[i];
  for (m = n->rgt, j = 0; m && j < q->nflds; m = m->rgt, j++) {
    if (launch_settings.need_columnated_output && !full)
      continue;
    if (verbose_flags.NeedToPrintReceives() && !Rvous &&
        depth >= launch_settings.count_of_skipping_steps) {
      char *Recv = "Recv ";
      char *notRecv = "Recv ";
      sr_talk(n, q->contents[i * q->nflds + j],
              (full && n->val < 2) ? Recv : notRecv, "<-", j, q);
    }
    if (!full)
      continue; /* test */
    if (m && m->lft->ntyp != CONST && m->lft->ntyp != EVAL) {
      (void)setval(m->lft, q->contents[i * q->nflds + j]);
      typ_ck(q->fld_width[j], Sym_typ(m->lft), "recv");
    }
    if (n->val < 2) /* not a poll */
      for (k = i; k < q->qlen - 1; k++) {
        q->contents[k * q->nflds + j] = q->contents[(k + 1) * q->nflds + j];
        if (j == 0)
          q->stepnr[k] = q->stepnr[k + 1];
      }
  }

  if ((!launch_settings.need_columnated_output || full) &&
      verbose_flags.NeedToPrintReceives() && !Rvous &&
      depth >= launch_settings.count_of_skipping_steps)
    for (i = j; i < q->nflds; i++) {
      char *Recv = "Recv ";
      char *notRecv = "Recv ";
      sr_talk(n, 0, (full && n->val < 2) ? Recv : notRecv, "<-", i, q);
    }
  if (launch_settings.need_generate_mas_flow_tcl_tk && full && !Rvous &&
      depth >= launch_settings.count_of_skipping_steps) {
    putarrow(oi, depth);
  }
  if (full && n->val < 2)
    q->qlen--;
  return 1;
}

static int s_snd(Queue *q, Lextok *n) {
  auto &verbose_flags = utils::verbose::Flags::getInstance();
  Lextok *m;
  RunList *rX, *sX = X_lst; /* rX=recvr, sX=sendr */
  int i, j = 0;             /* q field# */

  for (m = n->rgt; m && j < q->nflds; m = m->rgt, j++) {
    q->contents[j] = cast_val(q->fld_width[j], eval(m->lft), 0);
    typ_ck(q->fld_width[j], Sym_typ(m->lft), "rv-send");

    if (q->fld_width[j] == MTYPE) {
      mtype_ck(q->mtp[j], m->lft); /* 6.4.8 */
    }
  }

  q->qlen = 1;
  if (!complete_rendez()) {
    q->qlen = 0;
    return 0;
  }
  if (TstOnly) {
    q->qlen = 0;
    return 1;
  }
  q->stepnr[0] = depth;
  if (verbose_flags.NeedToPrintSends() &&
      depth >= launch_settings.count_of_skipping_steps) {
    m = n->rgt;
    rX = X_lst;
    X_lst = sX;

    for (j = 0; m && j < q->nflds; m = m->rgt, j++) {
      sr_talk(n, eval(m->lft), "Sent ", "->", j, q);
    }

    for (i = j; i < q->nflds; i++) {
      sr_talk(n, 0, "Sent ", "->", i, q);
    }

    if (j < q->nflds) {
      printf("%3d: warning: missing params in rv-send\n", depth);
    } else if (m) {
      printf("%3d: warning: too many params in rv-send\n", depth);
    }

    X_lst = rX; /* restore receiver's context */
    if (!launch_settings.need_save_trail) {
      if (!n_rem || !q_rem)
        loger::fatal("cannot happen, s_snd");
      m = n_rem->rgt;
      for (j = 0; m && j < q->nflds; m = m->rgt, j++) {
        if (q->fld_width[j] == MTYPE) {
          mtype_ck(q->mtp[j], m->lft); /* 6.4.8 */
        }

        if (m->lft->ntyp != NAME || m->lft->sym->name != "_") {
          i = eval(m->lft);
        } else {
          i = 0;
        }
        if (verbose_flags.NeedToPrintReceives()) {
          sr_talk(n_rem, i, "Recv ", "<-", j, q_rem);
        }
      }
      if (verbose_flags.NeedToPrintReceives()) {
        for (i = j; i < q->nflds; i++) {
          sr_talk(n_rem, 0, "Recv ", "<-", j, q_rem);
        }
      }
      if (launch_settings.need_generate_mas_flow_tcl_tk) {
        putarrow(depth, depth);
      }
    }
    n_rem = (Lextok *)0;
    q_rem = (Queue *)0;
  }
  return 1;
}

static void channm(Lextok *n) {
  std::string lbuf;

  if (n->sym->type == models::SymbolType::kChan) {
    strcat(GBuf, n->sym->name.c_str());
  } else if (n->sym->type == models::SymbolType::kName) {
    strcat(GBuf, lookup(n->sym->name)->name.c_str());
  } else if (n->sym->type == models::SymbolType::kStruct) {
    models::Symbol *r = n->sym;
    if (r->context) {
      r = findloc(r);
      if (!r) {
        strcat(GBuf, "*?*");
        return;
      }
    }
    ini_struct(r);
    printf("%s", r->name.c_str());
    lbuf = "";
    struct_name(n->lft, r, 1, lbuf);
    strcat(GBuf, lbuf.c_str());
  } else {
    strcat(GBuf, "-");
  }
  if (n->lft->lft) {
    lbuf = fmt::format("[{}]", eval(n->lft->lft));
    strcat(GBuf, lbuf.c_str());
  }
}

static void difcolumns(Lextok *n, char *tr, int v, int j, Queue *q) {
  extern int prno;

  if (j == 0) {
    GBuf[0] = '\0';
    channm(n);
    strcat(GBuf, (strncmp(tr, "Sen", 3)) ? "?" : "!");
  } else
    strcat(GBuf, ",");
  if (tr[0] == '[')
    strcat(GBuf, "[");
  sr_buf(v, q->fld_width[j] == MTYPE, q->mtp[j]);
  if (j == q->nflds - 1) {
    int cnr;
    if (launch_settings.need_save_trail) {
      cnr = prno - Have_claim;
    } else {
      cnr = X_lst ? X_lst->pid - Have_claim : 0;
    }
    if (tr[0] == '[')
      strcat(GBuf, "]");
    pstext(cnr, GBuf);
  }
}

static void docolumns(Lextok *n, char *tr, int v, int j, Queue *q) {
  int i;

  if (firstrow) {
    printf("q\\p");
    for (i = 0; i < nproc - nstop - Have_claim; i++)
      printf(" %3d", i);
    printf("\n");
    firstrow = 0;
  }
  if (j == 0) {
    printf("%3d", q->qid);
    if (X_lst)
      for (i = 0; i < X_lst->pid - Have_claim; i++)
        printf("   .");
    printf("   ");
    GBuf[0] = '\0';
    channm(n);
    printf("%s%c", GBuf, (strncmp(tr, "Sen", 3)) ? '?' : '!');
  } else
    printf(",");
  if (tr[0] == '[')
    printf("[");
  sr_mesg(stdout, v, q->fld_width[j] == MTYPE, q->mtp[j]);
  if (j == q->nflds - 1) {
    if (tr[0] == '[')
      printf("]");
    printf("\n");
  }
}

void qhide(int q) {
  QH *p = (QH *)emalloc(sizeof(QH));
  p->n = q;
  p->nxt = qh_lst;
  qh_lst = p;
}

int qishidden(int q) {
  QH *p;
  for (p = qh_lst; p; p = p->nxt)
    if (p->n == q)
      return 1;
  return 0;
}

static void sr_talk(Lextok *n, int v, char *tr, char *a, int j, Queue *q) {
  char s[128];
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  if (qishidden(eval(n->lft)))
    return;

  if (launch_settings.need_columnated_output) {
    if (launch_settings.need_generate_mas_flow_tcl_tk) {
      difcolumns(n, tr, v, j, q);
    } else {
      docolumns(n, tr, v, j, q);
    }
    return;
  }

  strcpy(s, tr);

  if (j == 0) {
    char snm[128];
    whoruns(1);
    {
      const char *ptr = n->fn->name.c_str();
      char *qtr = snm;
      while (*ptr != '\0') {
        if (*ptr != '\"') {
          *qtr++ = *ptr;
        }
        ptr++;
      }
      *qtr = '\0';
      printf("%s:%d %s", snm, n->ln, s);
    }
  } else {
    printf(",");
  }
  sr_mesg(stdout, v, q->fld_width[j] == MTYPE, q->mtp[j]);

  if (j == q->nflds - 1) {
    printf("\t%s queue %d (", a, eval(n->lft));
    GBuf[0] = '\0';
    channm(n);
    printf("%s)\n", GBuf);
  }
  fflush(stdout);
}

void sr_buf(int v, int j, const std::string &s) {
  int cnt = 1;
  Lextok *n;
  char lbuf[512];
  Lextok *Mtype = ZN;

  if (j) {
    Mtype = *find_mtype_list(!s.empty() ? s : "_unnamed_");
  }
  for (n = Mtype; n && j; n = n->rgt, cnt++) {
    if (cnt == v) {
      if (n->lft->sym->name.length() >= sizeof(lbuf)) {
        loger::non_fatal("mtype name %s too long", n->lft->sym->name);
        break;
      }
      sprintf(lbuf, "%s", n->lft->sym->name.c_str());
      strcat(GBuf, lbuf);
      return;
    }
  }
  sprintf(lbuf, "%d", v);
  strcat(GBuf, lbuf);
}

void sr_mesg(FILE *fd, int v, int j, const std::string &s) {
  GBuf[0] = '\0';

  sr_buf(v, j, s);
  fprintf(fd, GBuf, (char *)0); /* prevent compiler warning */
}

void doq(models::Symbol *s, int n, RunList *r) {
  Queue *q;
  int j, k;

  if (!s->value.empty()) /* uninitialized queue */
    return;
  for (q = qtab; q; q = q->nxt)
    if (q->qid == s->value[n]) {
      if (q->nslots == 0) {
        continue; /* rv q always empty */
      }

      printf("\t\tqueue %d (", q->qid);
      if (r) {
        printf("%s(%d):", r->n->name.c_str(), r->pid - Have_claim);
      }

      if (s->value_type > 1 || s->is_array) {
        printf("%s[%d]): ", s->name.c_str(), n);
      } else {
        printf("%s): ", s->name.c_str());
      }

      for (k = 0; k < q->qlen; k++) {
        printf("[");
        for (j = 0; j < q->nflds; j++) {
          if (j > 0)
            printf(",");
          sr_mesg(stdout, q->contents[k * q->nflds + j],
                  q->fld_width[j] == MTYPE, q->mtp[j]);
        }
        printf("]");
      }
      printf("\n");
      break;
    }
}

void nochan_manip(Lextok *p, Lextok *n, int d) /* p=lhs n=rhs */
{
  int e = 1;

  if (!n || !p || !p->sym ||
      p->sym->type == STRUCT) { /* if a struct, assignments to structure fields
                                   arent checked yet */
    return;
  }

  if (d == 0 && p->sym && p->sym->type == CHAN) {
    setaccess(p->sym, ZS, 0, 'L');

    if (n && n->ntyp == CONST)
      loger::fatal("invalid asgn to chan");

    if (n && n->sym && n->sym->type == CHAN) {
      setaccess(n->sym, ZS, 0, 'V');
      return;
    }
  }

  if (!d && n && n->ismtyp) /* rhs is an mtype value (a constant) */
  {
    std::string lhs = "_unnamed_", rhs = "_unnamed_";

    if (p->sym) {
      std::string unnamed = "_unnamed";
      lhs = p->sym->mtype_name ? p->sym->mtype_name->name : unnamed;
    }
    if (n->sym) {
      rhs = which_mtype(n->sym->name); /* only for constants */
    }

    if (p->sym && !p->sym->mtype_name && n->sym) {
      p->sym->mtype_name = (models::Symbol *)emalloc(sizeof(models::Symbol));
      p->sym->mtype_name->name = rhs;
    } else if (lhs != rhs) {
      fprintf(stderr,
              "spin: %s:%d, Error: '%s' is type '%s' but '%s' is type '%s'\n",
              p->fn->name.c_str(), p->ln, p->sym ? p->sym->name.c_str() : "?",
              lhs.c_str(), n->sym ? n->sym->name.c_str() : "?", rhs.c_str());
      loger::non_fatal("type error");
    }
  }

  /* ok on the rhs of an assignment: */
  if (!n || n->ntyp == LEN || n->ntyp == RUN || n->ntyp == FULL ||
      n->ntyp == NFULL || n->ntyp == EMPTY || n->ntyp == NEMPTY ||
      n->ntyp == 'R')
    return;

  if (n->sym && n->sym->type == CHAN) {
    if (d == 1)
      loger::fatal("invalid use of chan name");
    else
      setaccess(n->sym, ZS, 0, 'V');
  }

  if (n->ntyp == NAME || n->ntyp == '.') {
    e = 0; /* array index or struct element */
  }

  nochan_manip(p, n->lft, e);
  nochan_manip(p, n->rgt, 1);
}

struct BaseName {
  char *str;
  int cnt;
  struct BaseName *nxt;
};

static BaseName *bsn;

void newbasename(const std::string &s) {
  BaseName *b;

  /*	printf("+++++++++%s\n", s);	*/
  for (b = bsn; b; b = b->nxt)
    if (strcmp(b->str, s.c_str()) == 0) {
      b->cnt++;
      return;
    }
  b = (BaseName *)emalloc(sizeof(BaseName));
  b->str = emalloc(s.length() + 1);
  b->cnt = 1;
  strcpy(b->str, s.c_str());
  b->nxt = bsn;
  bsn = b;
}

void delbasename(const std::string &s) {
  BaseName *b, *prv = (BaseName *)0;

  for (b = bsn; b; prv = b, b = b->nxt) {
    if (strcmp(b->str, s.c_str()) == 0) {
      b->cnt--;
      if (b->cnt == 0) {
        if (prv) {
          prv->nxt = b->nxt;
        } else {
          bsn = b->nxt;
        }
      }
      /*	printf("---------%s\n", s);	*/
      break;
    }
  }
}

void checkindex(std::string &s, std::string &t) {
  BaseName *b;

  /*	printf("xxx Check %s (%s)\n", s, t);	*/
  for (b = bsn; b; b = b->nxt) {
    /*		printf("	%s\n", b->str);	*/
    if (strcmp(b->str, s.c_str()) == 0) {
      loger::non_fatal("do not index an array with itself (%s)", t.c_str());
      break;
    }
  }
}

void scan_tree(Lextok *t, std::string &mn, std::string &mx) {
  std::string sv;
  std::string tmp;
  int oln = lineno;

  if (!t)
    return;

  lineno = t->ln;

  if (t->ntyp == NAME) {
    if (t->sym->name.length() + mn.length() > 256) // conservative
    {
      loger::fatal("name too long", t->sym->name);
    }
    mn += t->sym->name;
    mx += t->sym->name;
    if (t->lft) /* array index */
    {
      mn += "[]";
      newbasename(mn);
      sv += mn;
      mn += "";
      mx += "[";
      scan_tree(t->lft, mn, mx); /* index */
      mx += "]";
      checkindex(mn, mx); /* match against basenames */
      mn = sv;            /*restore*/
      delbasename(mn);
    }
    if (t->rgt) /* structure element */
    {
      scan_tree(t->rgt, mn, mx);
    }
  } else if (t->ntyp == CONST) {
    mn += "1";
    tmp += t->val;
    mx += tmp;
  } else if (t->ntyp == '.') {
    mn += ".";
    mx += ".";
    scan_tree(t->lft, mn, mx);
  } else {
    mn += "??";
    mx += "??";
  }
  lineno = oln;
}

void no_nested_array_refs(
    Lextok *n) /* a [ a[1] ] with a[1] = 1, causes trouble in pan.b */
{
  std::string mn;
  std::string mx;

  bsn = (BaseName *)0; /* start new list */

  scan_tree(n, mn, mx);
  /*	printf("==> %s\n", mn);	*/
}

void no_internals(Lextok *n) {
  std::string sp;

  if (!n->sym || n->sym->name.empty())
    return;

  sp = n->sym->name;

  if ((sp.length() == strlen("_nr_pr") && sp.compare("_nr_pr") == 0) ||
      (sp.length() == strlen("_pid") && sp.compare("_pid") == 0) ||
      (sp.length() == strlen("_p") && sp.compare("_p") == 0)) {
    loger::fatal("invalid assignment to %s", sp.c_str());
  }

  no_nested_array_refs(n);
}