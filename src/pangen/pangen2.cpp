#include "pangen2.hpp"
#include "../fatal/fatal.hpp"
#include "../lexer/lexer.hpp"
#include "../spin.hpp"
#include "../utils/verbose/verbose.hpp"
#include "../version/version.hpp"
#include "pangen4.hpp"
#include "pangen5.hpp"
#include "pangen7.hpp"
#include "y.tab.h"
#include <fmt/core.h>
#include <iostream>

#include "../main/launch_settings.hpp"
extern LaunchSettings launch_settings;

#define DELTA 500 /* sets an upperbound on nr of chan names */

#define blurb(fd, e)                                                           \
  {                                                                            \
    fprintf(fd, "\n");                                                         \
    if (!launch_settings.need_statemate_merging)                               \
      fprintf(fd, "\t\t/* %s:%d */\n", e->n->fn->name.c_str(), e->n->ln);      \
  }
#define tr_map(m, e)                                                           \
  {                                                                            \
    if (!launch_settings.need_statemate_merging)                               \
      fprintf(fd_tt, "\t\ttr_2_src(%d, \"%s\", %d);\n", m,                     \
              e->n->fn->name.c_str(), e->n->ln);                               \
  }

extern ProcList *ready;
extern RunList *run_lst;
extern Lextok *runstmnts;
extern models::Symbol *Fname, *oFname, *context;
extern char *claimproc, *eventmap;
extern int lineno, Npars, Mpars, nclaims;
extern int has_remote, has_remvar, rvopt;
extern int Ntimeouts, Etimeouts;
extern int u_sync, u_async, nrRdy, Unique;
extern int GenCode, IsGuard, Level, TestOnly;
extern int globmin, globmax, dont_simplify;

extern short has_stack;
extern std::string NextLab[64]; /* must match value in dstep.c:18 */

FILE *fd_tc, *fd_th, *fd_tt, *fd_tb;
static FILE *fd_tm;

int OkBreak = -1, has_hidden = 0; /* has_hidden set in sym.c and structs.c */
short nocast = 0;                 /* to turn off casts in lvalues */
short terse = 0;                  /* terse printing of varnames */
short no_arrays = 0;
short has_badelse = 0;  /* spec contains else combined with chan refs */
short has_enabled = 0;  /* spec contains enabled() */
short has_pcvalue = 0;  /* spec contains pc_value() */
short has_np = 0;       /* spec contains np_ */
short has_sorted = 0;   /* spec contains `!!' (sorted-send) operator */
short has_random = 0;   /* spec contains `??' (random-recv) operator */
short has_xu = 0;       /* spec contains xr or xs assertions */
short has_unless = 0;   /* spec contains unless statements */
extern lexer::Lexer lexer_;
int mstp = 0;        /* max nr of state/process */
int claimnr = -1;    /* claim process, if any */
int eventmapnr = -1; /* event trace, if any */
int Pid_nr;          /* proc currently processed */
int multi_oval;      /* set in merges, used also in pangen4.c */
int in_settr;        /* avoid quotes inside quotes */

#define MAXMERGE 256 /* max nr of bups per merge sequence */

static short CnT[MAXMERGE];
static Lextok XZ, YZ[MAXMERGE];
static int didcase, YZmax, YZcnt;

static Lextok *Nn[2];
static int Det; /* set if deterministic */
static int T_sum, T_mus, t_cyc;
static int TPE[2], EPT[2];
static int uniq = 1;
static int multi_needed, multi_undo;
static short AllGlobal = 0;    /* set if process has provided clause */
static short withprocname = 0; /* prefix local varnames with procname */
static short _isok = 0;        /* checks usage of predefined variable _ */
static short evalindex = 0;    /* evaluate index of var names */

extern int has_global(Lextok *);
extern void check_mtypes(Lextok *, Lextok *);
extern void walk2_struct(const std::string &, models::Symbol *);
extern int find_min(Sequence *);
extern int find_max(Sequence *);

static int getweight(Lextok *);
static int scan_seq(Sequence *);
static void genconditionals(void);
static void mark_seq(Sequence *);
static void patch_atomic(Sequence *);
static void put_seq(Sequence *, int, int);
static void putproc(ProcList *);
static void Tpe(Lextok *);
extern void spit_recvs(FILE *, FILE *);

static L_List *keep_track;

void keep_track_off(Lextok *n) {
  L_List *p;

  p = (L_List *)emalloc(sizeof(L_List));
  p->n = n;
  p->nxt = keep_track;
  keep_track = p;
}

int check_track(Lextok *n) {
  L_List *p;

  for (p = keep_track; p; p = p->nxt) {
    if (p->n == n) {
      return n->sym ? n->sym->type : 0;
    }
  }
  return 0;
}

static int fproc(const std::string &s) {
  ProcList *p;

  for (p = ready; p; p = p->nxt)
    if (p->n->name.c_str() == s)
      return p->tn;

  loger::fatal("proctype %s not found", s);
  return -1;
}

int pid_is_claim(int p) /* Pid_nr (p->tn) to type (p->b) */
{
  ProcList *r;

  for (r = ready; r; r = r->nxt) {
    if (r->tn == p)
      return (r->b == N_CLAIM);
  }
  printf("spin: error, cannot find pid %d\n", p);
  return 0;
}

static void reverse_procs(RunList *q) {
  if (!q)
    return;
  reverse_procs(q->nxt);
  fprintf(fd_tc, "		Addproc(%d, %d);\n", q->tn,
          q->priority < 1 ? 1 : q->priority);
}

static void forward_procs(RunList *q) {
  if (!q)
    return;
  fprintf(fd_tc, "		Addproc(%d, %d);\n", q->tn,
          q->priority < 1 ? 1 : q->priority);
  forward_procs(q->nxt);
}

static void tm_predef_np(void) {
  fprintf(fd_th, "#define _T5	%d\n", uniq++);
  fprintf(fd_th, "#define _T2	%d\n", uniq++);

  fprintf(fd_tm, "\tcase  _T5:\t/* np_ */\n");

  if (launch_settings.separate_version == 2) {
    fprintf(fd_tm, "\t\tif (!((!(o_pm&4) && !(tau&128))))\n");
  } else {
    fprintf(fd_tm, "\t\tif (!((!(trpt->o_pm&4) && !(trpt->tau&128))))\n");
  }
  fprintf(fd_tm, "\t\t\tcontinue;\n");
  fprintf(fd_tm, "\t\t/* else fall through */\n");
  fprintf(fd_tm, "\tcase  _T2:\t/* true */\n");
  fprintf(fd_tm, "\t\t_m = 3; goto P999;\n");
}

static void tt_predef_np(void) {
  fprintf(fd_tt, "\t/* np_ demon: */\n");
  fprintf(fd_tt, "\ttrans[_NP_] = ");
  fprintf(fd_tt, "(Trans **) emalloc(3*sizeof(Trans *));\n");
  fprintf(fd_tt, "\tT = trans[_NP_][0] = ");
  fprintf(fd_tt, "settr(9997,0,1,_T5,0,\"(np_)\", 1,2,0);\n");
  fprintf(fd_tt, "\t    T->nxt	  = ");
  fprintf(fd_tt, "settr(9998,0,0,_T2,0,\"(1)\",   0,2,0);\n");
  fprintf(fd_tt, "\tT = trans[_NP_][1] = ");
  fprintf(fd_tt, "settr(9999,0,1,_T5,0,\"(np_)\", 1,2,0);\n");
}

static struct {
  char *nm[3];
} Cfile[] = {{{"pan.c", "pan_s.c", "pan_t.c"}},
             {{"pan.h", "pan_s.h", "pan_t.h"}},
             {{"pan.t", "pan_s.t", "pan_t.t"}},
             {{"pan.m", "pan_s.m", "pan_t.m"}},
             {{"pan.b", "pan_s.b", "pan_t.b"}}};

void gensrc(void) {
  ProcList *p;
  int i;

  disambiguate(); /* avoid name-clashes between scopes */

  if (!(fd_tc = fopen(Cfile[0].nm[launch_settings.separate_version], MFLAGS))    /* main routines */
      || !(fd_th = fopen(Cfile[1].nm[launch_settings.separate_version], MFLAGS)) /* header file   */
      || !(fd_tt = fopen(Cfile[2].nm[launch_settings.separate_version], MFLAGS)) /* transition matrix */
      || !(fd_tm = fopen(Cfile[3].nm[launch_settings.separate_version], MFLAGS)) /* forward  moves */
      || !(fd_tb = fopen(Cfile[4].nm[launch_settings.separate_version], MFLAGS))) /* backward moves */
  {
    printf("spin: cannot create pan.[chtmfb]\n");
    alldone(1);
  }

  fprintf(fd_th, "#ifndef PAN_H\n");
  fprintf(fd_th, "#define PAN_H\n\n");

  fprintf(fd_th, "#define SpinVersion	\"%s\"\n", SpinVersion);
  fprintf(fd_th, "#define PanSource	\"");
  for (i = 0; oFname->name[i] != '\0'; i++) {
    char c = oFname->name[i];
    if (c == '\\') /* Windows path */
    {
      fprintf(fd_th, "\\");
    }
    fprintf(fd_th, "%c", c);
  }
  fprintf(fd_th, "\"\n\n");

  fprintf(fd_th, "#define G_long	%d\n", (int)sizeof(long));
  fprintf(fd_th, "#define G_int	%d\n\n", (int)sizeof(int));
  fprintf(fd_th, "#define ulong	unsigned long\n");
  fprintf(fd_th, "#define ushort	unsigned short\n");

  fprintf(fd_th, "#ifdef WIN64\n");
  fprintf(fd_th, "	#define ONE_L	(1L)\n");
  fprintf(fd_th, "/*	#define long	long long */\n");
  fprintf(fd_th, "#else\n");
  fprintf(fd_th, "	#define ONE_L	(1L)\n");
  fprintf(fd_th, "#endif\n\n");

  fprintf(fd_th, "#ifdef BFS_PAR\n");
  fprintf(fd_th, "	#define NRUNS	%d\n", (runstmnts) ? 1 : 0);
  fprintf(fd_th, "	#ifndef BFS\n");
  fprintf(fd_th, "		#define BFS\n");
  fprintf(fd_th, "	#endif\n");
  fprintf(fd_th, "	#ifndef PUTPID\n");
  fprintf(fd_th, "		#define PUTPID\n");
  fprintf(fd_th, "	#endif\n\n");
  fprintf(fd_th, "	#if !defined(USE_TDH) && !defined(NO_TDH)\n");
  fprintf(fd_th, "		#define USE_TDH\n");
  fprintf(fd_th, "	#endif\n");
  fprintf(fd_th, "	#if defined(USE_TDH) && !defined(NO_HC)\n");
  fprintf(fd_th, "		#define HC /* default for USE_TDH */\n");
  fprintf(fd_th, "	#endif\n");
  fprintf(fd_th, "	#ifndef BFS_MAXPROCS\n");
  fprintf(fd_th, "		#define BFS_MAXPROCS	64	/* max nr of "
                 "cores to use */\n");
  fprintf(fd_th, "	#endif\n");

  fprintf(fd_th, "	#define BFS_GLOB	0	/* global lock */\n");
  fprintf(fd_th,
          "	#define BFS_ORD		1	/* used with -DCOLLAPSE */\n");
  fprintf(
      fd_th,
      "	#define BFS_MEM		2	/* malloc from shared heap */\n");
  fprintf(fd_th,
          "	#define BFS_PRINT	3	/* protect printfs */\n");
  fprintf(fd_th, "	#define BFS_STATE	4	/* hashtable */\n\n");
  fprintf(fd_th,
          "	#define BFS_INQ 	2	/* state is in q */\n\n");

  fprintf(fd_th, "	#ifdef BFS_FIFO\n"); /* queue access */
  fprintf(
      fd_th,
      "	  #define BFS_ID(a,b)	(BFS_STATE + (int) ((a)*BFS_MAXPROCS+(b)))\n");
  fprintf(
      fd_th,
      "	  #define BFS_MAXLOCKS	(BFS_STATE + (BFS_MAXPROCS*BFS_MAXPROCS))\n");
  fprintf(fd_th, "	#else\n"); /* h_store access (not needed for o_store) */
  fprintf(fd_th, "	  #ifndef BFS_W\n");
  fprintf(fd_th, "		#define BFS_W	10\n"); /* 1<<BFS_W locks */
  fprintf(fd_th, "	  #endif\n");
  fprintf(fd_th, "	  #define BFS_MASK	((1<<BFS_W) - 1)\n");
  fprintf(
      fd_th,
      "	  #define BFS_ID	(BFS_STATE + (int) (j1_spin & (BFS_MASK)))\n");
  fprintf(
      fd_th,
      "	  #define BFS_MAXLOCKS	(BFS_STATE + (1<<BFS_W))\n"); /* 4+1024 */
  fprintf(fd_th, "	#endif\n");

  fprintf(fd_th, "	#undef NCORE\n");
  fprintf(fd_th, "	extern int Cores, who_am_i;\n");
  fprintf(fd_th, "	#ifndef SAFETY\n");
  fprintf(fd_th, "	  #if !defined(BFS_STAGGER) && !defined(BFS_DISK)\n");
  fprintf(fd_th,
          "		#define BFS_STAGGER	64 /* randomizer, was 16 */\n");
  fprintf(fd_th, "	  #endif\n");
  fprintf(fd_th, "	  #ifndef L_BOUND\n");
  fprintf(fd_th, "		#define L_BOUND 	10 /* default */\n");
  fprintf(fd_th, "	  #endif\n");
  fprintf(fd_th, "	  extern int L_bound;\n");
  fprintf(fd_th, "	#endif\n");
  fprintf(fd_th, "	#if defined(BFS_DISK) && defined(BFS_STAGGER)\n");
  fprintf(fd_th,
          "		#error BFS_DISK and BFS_STAGGER are not compatible\n");
  fprintf(fd_th, "	#endif\n");
  fprintf(fd_th, "#endif\n\n");

  fprintf(fd_th, "#if defined(BFS)\n");
  fprintf(fd_th, "	#ifndef SAFETY\n");
  fprintf(fd_th, "		#define SAFETY\n");
  fprintf(fd_th, "	#endif\n");
  fprintf(fd_th, "	#ifndef XUSAFE\n");
  fprintf(fd_th, "		#define XUSAFE\n");
  fprintf(fd_th, "	#endif\n");
  fprintf(fd_th, "#endif\n");

  fprintf(fd_th, "#ifndef uchar\n");
  fprintf(fd_th, "	#define uchar	unsigned char\n");
  fprintf(fd_th, "#endif\n");
  fprintf(fd_th, "#ifndef uint\n");
  fprintf(fd_th, "	#define uint	unsigned int\n");
  fprintf(fd_th, "#endif\n");

  if (launch_settings.separate_version == 1 && !claimproc) {
    models::Symbol *n = (models::Symbol *)emalloc(sizeof(models::Symbol));
    Sequence *s = (Sequence *)emalloc(sizeof(Sequence));
    s->minel = -1;
    claimproc = "_:never_template:_";
    n->name = "_:never_template:_";
    mk_rdy(n, ZN, s, 0, ZN, N_CLAIM);
  }
  if (launch_settings.separate_version == 2) {
    if (has_remote) {
      printf("spin: warning, make sure that the S1 model\n");
      printf("      includes the same remote references\n");
    }
    fprintf(fd_th, "#ifndef NFAIR\n");
    fprintf(fd_th, "#define NFAIR	2	/* must be >= 2 */\n");
    fprintf(fd_th, "#endif\n");
    if (lexer_.GetHasLast())
      fprintf(fd_th, "#define HAS_LAST	%d\n", lexer_.GetHasLast());
    if (lexer_.GetHasPriority() && !launch_settings.need_revert_old_rultes_for_priority)
      fprintf(fd_th, "#define HAS_PRIORITY	%d\n", lexer_.GetHasPriority());
    goto doless;
  }

  fprintf(fd_th, "#define DELTA	%d\n", DELTA);
  fprintf(fd_th, "#ifdef MA\n");
  fprintf(fd_th, "	#if NCORE>1 && !defined(SEP_STATE)\n");
  fprintf(fd_th, "		#define SEP_STATE\n");
  fprintf(fd_th, "	#endif\n");
  fprintf(fd_th, "	#if MA==1\n"); /* user typed -DMA without size */
  fprintf(fd_th, "		#undef MA\n");
  fprintf(fd_th, "		#define MA	100\n");
  fprintf(fd_th, "	#endif\n");
  fprintf(fd_th, "#endif\n");
  fprintf(fd_th, "#ifdef W_XPT\n");
  fprintf(fd_th, "	#if W_XPT==1\n"); /* user typed -DW_XPT without size */
  fprintf(fd_th, "		#undef W_XPT\n");
  fprintf(fd_th, "		#define W_XPT 1000000\n");
  fprintf(fd_th, "	#endif\n");
  fprintf(fd_th, "#endif\n");
  fprintf(fd_th, "#ifndef NFAIR\n");
  fprintf(fd_th, "	#define NFAIR	2	/* must be >= 2 */\n");
  fprintf(fd_th, "#endif\n");
  if (Ntimeouts)
    fprintf(fd_th, "#define NTIM	%d\n", Ntimeouts);
  if (Etimeouts)
    fprintf(fd_th, "#define ETIM	%d\n", Etimeouts);
  if (has_remvar)
    fprintf(fd_th, "#define REM_VARS	1\n");
  if (has_remote)
    fprintf(fd_th, "#define REM_REFS	%d\n", has_remote); /* not yet used */
  if (has_hidden) {
    fprintf(fd_th, "#define HAS_HIDDEN	%d\n", has_hidden);
    fprintf(fd_th, "#if defined(BFS_PAR) || defined(BFS)\n");
    fprintf(fd_th, "	#error cannot use BFS on models with variables "
                   "declared hidden_flags\n");
    fprintf(fd_th, "#endif\n");
  }
  if (lexer_.GetHasLast())
    fprintf(fd_th, "#define HAS_LAST	%d\n", lexer_.GetHasLast());
  if (lexer_.GetHasPriority() && !launch_settings.need_revert_old_rultes_for_priority)
    fprintf(fd_th, "#define HAS_PRIORITY	%d\n", lexer_.GetHasPriority());
  if (has_sorted)
    fprintf(fd_th, "#define HAS_SORTED	%d\n", has_sorted);
  if (launch_settings.need_lose_msgs_sent_to_full_queues)
    fprintf(fd_th, "#define M_LOSS\n");
  if (has_random)
    fprintf(fd_th, "#define HAS_RANDOM	%d\n", has_random);
  if (lexer_.HasLtl())
    fprintf(fd_th, "#define HAS_LTL	1\n");
  fprintf(fd_th,
          "#define HAS_CODE	1\n"); /* could also be set to has_code */
  /* always defining it doesn't seem to cause measurable overhead though */
  /* and allows for pan -r etc to work for non-embedded code as well */
  fprintf(fd_th, "#if defined(RANDSTORE) && !defined(RANDSTOR)\n");
  fprintf(
      fd_th,
      "	#define RANDSTOR	RANDSTORE\n"); /* xspin uses RANDSTORE... */
  fprintf(fd_th, "#endif\n");
  if (has_stack)
    fprintf(fd_th, "#define HAS_STACK	%d\n", has_stack);
  if (has_enabled || (lexer_.GetHasPriority() && !launch_settings.need_revert_old_rultes_for_priority))
    fprintf(fd_th, "#define HAS_ENABLED	1\n");
  if (has_unless)
    fprintf(fd_th, "#define HAS_UNLESS	%d\n", has_unless);
  if (launch_settings.has_provided)
    fprintf(fd_th, "#define HAS_PROVIDED	%d\n", launch_settings.has_provided);
  if (has_pcvalue)
    fprintf(fd_th, "#define HAS_PCVALUE	%d\n", has_pcvalue);
  if (has_badelse)
    fprintf(fd_th, "#define HAS_BADELSE	%d\n", has_badelse);
  if (has_enabled || (lexer_.GetHasPriority() && !launch_settings.need_revert_old_rultes_for_priority) ||
      has_pcvalue || has_badelse || lexer_.GetHasLast()) {
    fprintf(fd_th, "#ifndef NOREDUCE\n");
    fprintf(fd_th, "	#define NOREDUCE	1\n");
    fprintf(fd_th, "#endif\n");
  }
  if (has_np)
    fprintf(fd_th, "#define HAS_NP	%d\n", has_np);
  if (launch_settings.need_statemate_merging)
    fprintf(fd_th, "#define MERGED	1\n");

doless:
  fprintf(fd_th, "#if !defined(HAS_LAST) && defined(BCS)\n");
  fprintf(fd_th, "	#define HAS_LAST	1 /* use it, but */\n");
  fprintf(fd_th, "	#ifndef STORE_LAST\n"); /* unless the user insists */
  fprintf(fd_th, "		#define NO_LAST	1 /* don't store it */\n");
  fprintf(fd_th, "	#endif\n");
  fprintf(fd_th, "#endif\n");

  fprintf(fd_th, "#if defined(BCS) && defined(BITSTATE)\n");
  fprintf(fd_th, "	#ifndef NO_CTX\n");
  fprintf(fd_th, "		#define STORE_CTX	1\n");
  fprintf(fd_th, "	#endif\n");
  fprintf(fd_th, "#endif\n");

  fprintf(fd_th, "#ifdef NP\n");
  if (!has_np)
    fprintf(fd_th, "	#define HAS_NP	2\n");
  fprintf(fd_th, "	#define VERI	%d	/* np_ */\n", nrRdy);
  fprintf(fd_th, "#endif\n");

  fprintf(fd_th, "#if defined(NOCLAIM) && defined(NP)\n");
  fprintf(fd_th, "	#undef NOCLAIM\n");
  fprintf(fd_th, "#endif\n");
  if (claimproc) {
    claimnr = fproc(claimproc); /* the default claim */
    fprintf(fd_th, "#ifndef NOCLAIM\n");
    fprintf(fd_th, "	#define NCLAIMS	%d\n", nclaims);
    fprintf(fd_th, "	#ifndef NP\n");
    fprintf(fd_th, "		#define VERI	%d\n", claimnr);
    fprintf(fd_th, "	#endif\n");
    fprintf(fd_th, "#endif\n");
  }
  if (eventmap) {
    eventmapnr = fproc(eventmap);
    fprintf(fd_th, "#define EVENT_TRACE	%d\n", eventmapnr);
    fprintf(fd_th, "#define endevent	_endstate%d\n", eventmapnr);
    if (eventmap[2] == 'o') /* ":notrace:" */
      fprintf(fd_th, "#define NEGATED_TRACE	1\n");
  }

  fprintf(fd_th, "\nstruct S_F_MAP {\n");
  fprintf(fd_th, "	char *fnm;\n\tint from;\n\tint upto;\n");
  fprintf(fd_th, "} S_F_MAP;\n");

  fprintf(fd_tc, "/*** Generated by %s ***/\n", SpinVersion);
  fprintf(fd_tc, "/*** From source: %s ***/\n\n", oFname->name.c_str());

  ntimes(fd_tc, 0, 1, Pre0);

  plunk_c_decls(fd_tc); /* types can be refered to in State */

  switch (launch_settings.separate_version) {
  case 0:
    fprintf(fd_tc, "#include \"pan.h\"\n");
    break;
  case 1:
    fprintf(fd_tc, "#include \"pan_s.h\"\n");
    break;
  case 2:
    fprintf(fd_tc, "#include \"pan_t.h\"\n");
    break;
  }

  if (launch_settings.separate_version != 2) {
    fprintf(fd_tc, "char *TrailFile = PanSource; /* default */\n");
    fprintf(fd_tc, "char *trailfilename;\n");
  }

  fprintf(fd_tc, "#ifdef LOOPSTATE\n");
  fprintf(fd_tc, "double cnt_loops;\n");
  fprintf(fd_tc, "#endif\n");

  fprintf(fd_tc, "State	A_Root;	/* seed-state for cycles */\n");
  fprintf(fd_tc, "State	now;	/* the full state-vector */\n");
  fprintf(fd_tc, "#if NQS > 0\n");
  fprintf(fd_tc, "short q_flds[NQS+1];\n");
  fprintf(fd_tc, "short q_max[NQS+1];\n");
  fprintf(fd_tc, "#endif\n");

  fprintf(fd_tc, "#ifndef XUSAFE\n");
  fprintf(fd_tc, "	uchar q_claim[MAXQ+1];\n");
  fprintf(fd_tc, "	char *q_name[MAXQ+1];\n");
  fprintf(fd_tc, "	char *p_name[MAXPROC+1];\n");
  fprintf(fd_tc, "#endif\n");

  plunk_c_fcts(fd_tc); /* State can be used in fcts */

  if (launch_settings.separate_version != 2) {
    ntimes(fd_tc, 0, 1, Preamble);
    ntimes(fd_tc, 0, 1, Separate); /* things that moved out of pan.h */
  } else {
    fprintf(fd_tc, "extern int verbose;\n");
    fprintf(fd_tc, "extern long depth, depthfound;\n");
  }

  fprintf(fd_tc, "#ifndef NOBOUNDCHECK\n");
  fprintf(fd_tc, "	#define Index(x, y)\tBoundcheck(x, y, II, tt, t)\n");
  fprintf(fd_tc, "#else\n");
  fprintf(fd_tc, "	#define Index(x, y)\tx\n");
  fprintf(fd_tc, "#endif\n");

  c_preview(); /* sets hastrack */

  for (p = ready; p; p = p->nxt)
    mstp = max(p->s->maxel, mstp);

  if (launch_settings.separate_version != 2) {
    fprintf(fd_tt, "#ifdef PEG\n");
    fprintf(fd_tt, "struct T_SRC {\n");
    fprintf(fd_tt, "	char *fl; int ln;\n");
    fprintf(fd_tt, "} T_SRC[NTRANS];\n\n");
    fprintf(fd_tt, "void\ntr_2_src(int m, char *file, int ln)\n");
    fprintf(fd_tt, "{	T_SRC[m].fl = file;\n");
    fprintf(fd_tt, "	T_SRC[m].ln = ln;\n");
    fprintf(fd_tt, "}\n\n");
    fprintf(fd_tt, "void\nputpeg(int n, int m)\n");
    fprintf(fd_tt, "{	printf(\"%%5d\ttrans %%4d \", m, n);\n");
    fprintf(fd_tt, "	printf(\"%%s:%%d\\n\",\n");
    fprintf(fd_tt, "		T_SRC[n].fl, T_SRC[n].ln);\n");
    fprintf(fd_tt, "}\n");
    if (!launch_settings.need_statemate_merging) {
      fprintf(fd_tt, "#else\n");
      fprintf(fd_tt, "#define tr_2_src(m,f,l)\n");
    }
    fprintf(fd_tt, "#endif\n\n");
    fprintf(fd_tt, "void\nsettable(void)\n{\tTrans *T;\n");
    fprintf(fd_tt, "\tTrans *settr(int, int, int, int, int,");
    fprintf(fd_tt, " char *, int, int, int);\n\n");
    fprintf(fd_tt, "\ttrans = (Trans ***) ");
    fprintf(fd_tt, "emalloc(%d*sizeof(Trans **));\n", nrRdy + 1);
    /* +1 for np_ automaton */

    if (launch_settings.separate_version == 1) {
      fprintf(fd_tm, "	if (II == 0)\n");
      fprintf(fd_tm, "	{ _m = step_claim(trpt->o_pm, trpt->tau, tt, ot, "
                     "t);\n");
      fprintf(fd_tm, "	  if (_m) goto P999; else continue;\n");
      fprintf(fd_tm, "	} else\n");
    }

    fprintf(fd_tm, "#define rand	pan_rand\n");
    fprintf(fd_tm, "#define pthread_equal(a,b)	((a)==(b))\n");
    fprintf(fd_tm, "#if defined(HAS_CODE) && defined(VERBOSE)\n");
    fprintf(fd_tm, "	#ifdef BFS_PAR\n");
    fprintf(fd_tm,
            "		bfs_printf(\"Pr: %%d Tr: %%d\\n\", II, t->forw);\n");
    fprintf(fd_tm, "	#else\n");
    fprintf(fd_tm,
            "		cpu_printf(\"Pr: %%d Tr: %%d\\n\", II, t->forw);\n");
    fprintf(fd_tm, "	#endif\n");
    fprintf(fd_tm, "#endif\n");
    fprintf(fd_tm, "	switch (t->forw) {\n");
  } else {
    fprintf(fd_tt, "#ifndef PEG\n");
    fprintf(fd_tt, "	#define tr_2_src(m,f,l)\n");
    fprintf(fd_tt, "#endif\n");
    fprintf(fd_tt, "void\nset_claim(void)\n{\tTrans *T;\n");
    fprintf(fd_tt, "\textern Trans ***trans;\n");
    fprintf(fd_tt, "\textern Trans *settr(int, int, int, int, int,");
    fprintf(fd_tt, " char *, int, int, int);\n\n");

    fprintf(fd_tm, "#define rand	pan_rand\n");
    fprintf(fd_tm, "#define pthread_equal(a,b)	((a)==(b))\n");
    fprintf(fd_tm, "#if defined(HAS_CODE) && defined(VERBOSE)\n");
    fprintf(fd_tm, "	cpu_printf(\"Pr: %%d Tr: %%d\\n\", II, forw);\n");
    fprintf(fd_tm, "#endif\n");
    fprintf(fd_tm, "	switch (forw) {\n");
  }

  fprintf(fd_tm, "	default: Uerror(\"bad forward move\");\n");
  fprintf(fd_tm, "	case 0:	/* if without executable clauses */\n");
  fprintf(fd_tm, "		continue;\n");
  fprintf(fd_tm, "	case 1: /* generic 'goto' or 'skip' */\n");
  if (launch_settings.separate_version != 2)
    fprintf(fd_tm, "		IfNotBlocked\n");
  fprintf(fd_tm, "		_m = 3; goto P999;\n");
  fprintf(fd_tm, "	case 2: /* generic 'else' */\n");
  if (launch_settings.separate_version == 2)
    fprintf(fd_tm, "		if (o_pm&1) continue;\n");
  else {
    fprintf(fd_tm, "		IfNotBlocked\n");
    fprintf(fd_tm, "		if (trpt->o_pm&1) continue;\n");
  }
  fprintf(fd_tm, "		_m = 3; goto P999;\n");
  uniq = 3;

  if (launch_settings.separate_version == 1)
    fprintf(fd_tb, "	if (II == 0) goto R999;\n");

  fprintf(fd_tb, "	switch (t->back) {\n");
  fprintf(fd_tb, "	default: Uerror(\"bad return move\");\n");
  fprintf(fd_tb, "	case  0: goto R999; /* nothing to undo */\n");

  for (p = ready; p; p = p->nxt) {
    putproc(p);
  }

  if (launch_settings.separate_version != 2) {
    fprintf(fd_th, "\n");
    for (p = ready; p; p = p->nxt)
      fprintf(fd_th, "extern short src_ln%d[];\n", p->tn);
    for (p = ready; p; p = p->nxt)
      fprintf(fd_th, "extern S_F_MAP src_file%d[];\n", p->tn);
    fprintf(fd_th, "\n");

    fprintf(fd_tc, "uchar reached%d[3];  /* np_ */\n", nrRdy);
    fprintf(fd_tc, "uchar *loopstate%d;  /* np_ */\n", nrRdy);

    fprintf(fd_tc, "struct {\n");
    fprintf(fd_tc, "	int tp; short *src;\n");
    fprintf(fd_tc, "} src_all[] = {\n");
    for (p = ready; p; p = p->nxt)
      fprintf(fd_tc, "	{ %d, &src_ln%d[0] },\n", p->tn, p->tn);
    fprintf(fd_tc, "	{ 0, (short *) 0 }\n");
    fprintf(fd_tc, "};\n");

    fprintf(fd_tc, "S_F_MAP *flref[] = {\n"); /* 5.3.0 */
    for (p = ready; p; p = p->nxt) {
      fprintf(fd_tc, "	src_file%d%c\n", p->tn, p->nxt ? ',' : ' ');
    }
    fprintf(fd_tc, "};\n\n");
  } else {
    fprintf(fd_tc, "extern uchar reached%d[3];  /* np_ */\n", nrRdy);
  }

  gencodetable(fd_tc); /* was th */

  if (Unique < (1 << (8 * sizeof(unsigned char)))) /* was uniq before */
  {
    fprintf(fd_th, "#define T_ID	unsigned char\n");
  } else if (Unique < (1 << (8 * sizeof(unsigned short)))) {
    fprintf(fd_th, "#define T_ID	unsigned short\n");
  } else {
    fprintf(fd_th, "#define T_ID	unsigned int\n");
  }

  if (launch_settings.separate_version != 1) {
    tm_predef_np();
    tt_predef_np();
  }
  fprintf(fd_tt, "}\n\n"); /* end of settable() */

  fprintf(fd_tm, "#undef rand\n");
  fprintf(fd_tm, "	}\n\n");
  fprintf(fd_tb, "	}\n\n");

  if (launch_settings.separate_version != 2) {
    ntimes(fd_tt, 0, 1, Tail);
    genheader();
    if (launch_settings.separate_version == 1) {
      fprintf(fd_th, "#define FORWARD_MOVES\t\"pan_s.m\"\n");
      fprintf(fd_th, "#define BACKWARD_MOVES\t\"pan_s.b\"\n");
      fprintf(fd_th, "#define SEPARATE\n");
      fprintf(fd_th, "#define TRANSITIONS\t\"pan_s.t\"\n");
      fprintf(fd_th, "extern void ini_claim(int, int);\n");
    } else {
      fprintf(fd_th, "#define FORWARD_MOVES\t\"pan.m\"\n");
      fprintf(fd_th, "#define BACKWARD_MOVES\t\"pan.b\"\n");
      fprintf(fd_th, "#define TRANSITIONS\t\"pan.t\"\n");
    }
    genaddproc();
    genother();
    genaddqueue();
    genunio();
    genconditionals();
    gensvmap();
    if (!run_lst)
      loger::fatal("no runable process");
    fprintf(fd_tc, "void\n");
    fprintf(fd_tc, "active_procs(void)\n{\n");

    fprintf(fd_tc, "	if (reversing == 0) {\n");
    reverse_procs(run_lst);
    fprintf(fd_tc, "	} else {\n");
    forward_procs(run_lst);
    fprintf(fd_tc, "	}\n");

    fprintf(fd_tc, "}\n");
    ntimes(fd_tc, 0, 1, Dfa);
    ntimes(fd_tc, 0, 1, Xpt);

    fprintf(fd_th, "#define NTRANS	%d\n", uniq);
    if (u_sync && !u_async) {
      spit_recvs(fd_th, fd_tc);
    }
  } else {
    genheader();
    fprintf(fd_th, "#define FORWARD_MOVES\t\"pan_t.m\"\n");
    fprintf(fd_th, "#define BACKWARD_MOVES\t\"pan_t.b\"\n");
    fprintf(fd_th, "#define TRANSITIONS\t\"pan_t.t\"\n");
    fprintf(fd_tc, "extern int Maxbody;\n");
    fprintf(fd_tc, "#if VECTORSZ>32000\n");
    fprintf(fd_tc, "	extern int *proc_offset;\n");
    fprintf(fd_tc, "#else\n");
    fprintf(fd_tc, "	extern short *proc_offset;\n");
    fprintf(fd_tc, "#endif\n");
    fprintf(fd_tc, "extern uchar *proc_skip;\n");
    fprintf(fd_tc, "extern uchar *reached[];\n");
    fprintf(fd_tc, "extern uchar *accpstate[];\n");
    fprintf(fd_tc, "extern uchar *progstate[];\n");
    fprintf(fd_tc, "extern uchar *loopstate[];\n");
    fprintf(fd_tc, "extern uchar *stopstate[];\n");
    fprintf(fd_tc, "extern uchar *visstate[];\n\n");
    fprintf(fd_tc, "extern short *mapstate[];\n");

    fprintf(fd_tc, "void\nini_claim(int n, int h)\n{");
    fprintf(fd_tc, "\textern State now;\n");
    fprintf(fd_tc, "\textern void set_claim(void);\n\n");
    fprintf(fd_tc, "#ifdef PROV\n");
    fprintf(fd_tc, "	#include PROV\n");
    fprintf(fd_tc, "#endif\n");
    fprintf(fd_tc, "\tset_claim();\n");
    genother();
    fprintf(fd_tc, "\n\tswitch (n) {\n");
    genaddproc();
    fprintf(fd_tc, "\t}\n");
    fprintf(fd_tc, "\n}\n");
    fprintf(fd_tc,
            "int\nstep_claim(int o_pm, int tau, int tt, int ot, Trans *t)\n");
    fprintf(fd_tc, "{	int forw = t->forw; int _m = 0; extern char *noptr; "
                   "int II=0;\n");
    fprintf(fd_tc, "	extern State now;\n");
    fprintf(fd_tc, "#define continue	return 0\n");
    fprintf(fd_tc, "#include \"pan_t.m\"\n");
    fprintf(fd_tc, "P999:\n\treturn _m;\n}\n");
    fprintf(fd_tc, "#undef continue\n");
    fprintf(fd_tc, "int\nrev_claim(int backw)\n{ return 0; }\n");
    fprintf(fd_tc, "#include TRANSITIONS\n");
  }

  if (launch_settings.separate_version != 2) {
    c_wrapper(fd_tc);
    c_chandump(fd_tc);
  }

  fprintf(fd_th, "#if defined(BFS_PAR) || NCORE>1\n");
  fprintf(fd_th, "	void e_critical(int);\n");
  fprintf(fd_th, "	void x_critical(int);\n");
  fprintf(fd_th, "	#ifdef BFS_PAR\n");
  fprintf(fd_th, "		void bfs_main(int, int);\n");
  fprintf(fd_th, "		void bfs_report_mem(void);\n");
  fprintf(fd_th, "	#endif\n");
  fprintf(fd_th, "#endif\n");

  fprintf(fd_th, "\n\n/* end of PAN_H */\n#endif\n");
  fclose(fd_th);
  fclose(fd_tt);
  fclose(fd_tm);
  fclose(fd_tb);

  if (!(fd_th = fopen("pan.p", MFLAGS))) {
    printf("spin: cannot create pan.p for -DBFS_PAR\n");
    return; /* we're done anyway */
  }

  ntimes(fd_th, 0, 1, pan_par); /* BFS_PAR */
  fclose(fd_th);

  fprintf(fd_tc, "\nTrans *t_id_lkup[%d];\n\n", globmax + 1);

  if (launch_settings.separate_version != 2) {
    fprintf(fd_tc, "\n#ifdef BFS_PAR\n\t#include \"pan.p\"\n#endif\n");
  }
  fprintf(fd_tc, "\n/* end of pan.c */\n");
  fclose(fd_tc);
}

static int find_id(models::Symbol *s) {
  ProcList *p;

  if (s)
    for (p = ready; p; p = p->nxt)
      if (s == p->n)
        return p->tn;
  return 0;
}

static void dolen(models::Symbol *s, char *pre, int pid, int ai, int qln) {
  if (ai > 0)
    fprintf(fd_tc, "\n\t\t\t ||    ");
  fprintf(fd_tc, "%s(", pre);
  if (!(s->hidden_flags & 1)) {
    if (s->context)
      fprintf(fd_tc, "(int) ( ((P%d *)_this)->", pid);
    else
      fprintf(fd_tc, "(int) ( now.");
  }
  fprintf(fd_tc, "%s", s->name.c_str());
  if (qln > 1 || s->is_array)
    fprintf(fd_tc, "[%d]", ai);
  fprintf(fd_tc, ") )");
}

struct AA {
  char TT[9];
  char CC[8];
};

static struct AA BB[4] = {{"Q_FULL_F", " q_full"},
                          {"Q_FULL_T", "!q_full"},
                          {"Q_EMPT_F", " !q_len"},
                          {"Q_EMPT_T", "  q_len"}};

static struct AA DD[4] = {{"Q_FULL_F", " q_e_f"}, /* empty or full */
                          {"Q_FULL_T", "!q_full"},
                          {"Q_EMPT_F", " q_e_f"},
                          {"Q_EMPT_T", " q_len"}};
/* this reduces the number of cases where 's' and 'r'
   are considered conditionally safe under the
   partial order reduction rules;  as a price for
   this simple implementation, it also affects the
   cases where nfull and nempty can be considered
   safe -- since these are labeled the same way as
   's' and 'r' respectively
   it only affects reduction, not functionality
 */

void bb_or_dd(int j, int which) {
  if (which) {
    if (has_unless)
      fprintf(fd_tc, "%s", DD[j].CC);
    else
      fprintf(fd_tc, "%s", BB[j].CC);
  } else {
    if (has_unless)
      fprintf(fd_tc, "%s", DD[j].TT);
    else
      fprintf(fd_tc, "%s", BB[j].TT);
  }
}

void Done_case(const std::string &nm, models::Symbol *z) {
  int j, k;
  int nid = z->id;
  int qln = z->value_type;

  fprintf(fd_tc, "\t\tcase %d: if (", nid);
  for (j = 0; j < 4; j++) {
    fprintf(fd_tc, "\t(t->ty[i] == ");
    bb_or_dd(j, 0);
    fprintf(fd_tc, " && (");
    for (k = 0; k < qln; k++) {
      if (k > 0)
        fprintf(fd_tc, "\n\t\t\t ||    ");
      bb_or_dd(j, 1);
      fprintf(fd_tc, "(%s%s", nm.c_str(), z->name.c_str());
      if (qln > 1)
        fprintf(fd_tc, "[%d]", k);
      fprintf(fd_tc, ")");
    }
    fprintf(fd_tc, "))\n\t\t\t ");
    if (j < 3)
      fprintf(fd_tc, "|| ");
    else
      fprintf(fd_tc, "   ");
  }
  fprintf(fd_tc, ") return 0; break;\n");
}

static void Docase(models::Symbol *s, int pid, int nid) {
  int i, j;

  fprintf(fd_tc, "\t\tcase %d: if (", nid);
  for (j = 0; j < 4; j++) {
    fprintf(fd_tc, "\t(t->ty[i] == ");
    bb_or_dd(j, 0);
    fprintf(fd_tc, " && (");
    if (has_unless) {
      for (i = 0; i < s->value_type; i++)
        dolen(s, DD[j].CC, pid, i, s->value_type);
    } else {
      for (i = 0; i < s->value_type; i++)
        dolen(s, BB[j].CC, pid, i, s->value_type);
    }
    fprintf(fd_tc, "))\n\t\t\t ");
    if (j < 3)
      fprintf(fd_tc, "|| ");
    else
      fprintf(fd_tc, "   ");
  }
  fprintf(fd_tc, ") return 0; break;\n");
}

static void genconditionals(void) {
  models::Symbol *s;
  int last = 0, j;
  extern Ordered *all_names;
  Ordered *walk;

  fprintf(fd_th, "#define LOCAL	1\n");
  fprintf(fd_th, "#define Q_FULL_F	2\n");
  fprintf(fd_th, "#define Q_EMPT_F	3\n");
  fprintf(fd_th, "#define Q_EMPT_T	4\n");
  fprintf(fd_th, "#define Q_FULL_T	5\n");
  fprintf(fd_th, "#define TIMEOUT_F	6\n");
  fprintf(fd_th, "#define GLOBAL	7\n");
  fprintf(fd_th, "#define BAD	8\n");
  fprintf(fd_th, "#define ALPHA_F	9\n");

  fprintf(fd_tc, "int\n");
  fprintf(fd_tc, "q_cond(short II, Trans *t)\n");
  fprintf(fd_tc, "{	int i = 0;\n");
  fprintf(fd_tc, "	for (i = 0; i < 6; i++)\n");
  fprintf(fd_tc, "	{	if (t->ty[i] == TIMEOUT_F) return %s;\n",
          (Etimeouts) ? "(!(trpt->tau&1))" : "1");
  fprintf(fd_tc, "		if (t->ty[i] == ALPHA_F)\n");
  fprintf(fd_tc, "#ifdef GLOB_ALPHA\n");
  fprintf(fd_tc, "			return 0;\n");
  fprintf(fd_tc, "#else\n\t\t\treturn ");
  fprintf(fd_tc, "(II+1 == (short) now._nr_pr && II+1 < MAXPROC);\n");
  fprintf(fd_tc, "#endif\n");

  /* we switch on the chan name from the spec (as identified by
   * the corresponding Nid number) rather than the actual qid
   * because we cannot predict at compile time which specific qid
   * will be accessed by the statement at runtime.  that is:
   * we do not know which qid to pass to q_cond at runtime
   * but we do know which name is used.  if it's a chan array, we
   * must check all elements of the array for compliance (bummer)
   */
  fprintf(fd_tc, "		switch (t->qu[i]) {\n");
  fprintf(fd_tc, "		case 0: break;\n");

  for (walk = all_names; walk; walk = walk->next) {
    s = walk->entry;
    if (s->owner_name)
      continue;
    j = find_id(s->context);
    if (s->type == CHAN) {
      if (last == s->id)
        continue; /* chan array */
      last = s->id;
      Docase(s, j, last);
    } else if (s->type == STRUCT) { /* struct may contain a chan */
      char pregat[128];
      strcpy(pregat, "");
      if (!(s->hidden_flags & 1)) {
        if (s->context)
          sprintf(pregat, "((P%d *)_this)->", j);
        else
          sprintf(pregat, "now.");
      }
      walk2_struct(pregat, s);
    }
  }
  fprintf(fd_tc, "	\tdefault: Uerror(\"unknown qid - q_cond\");\n");
  fprintf(fd_tc, "	\t\t\treturn 0;\n");
  fprintf(fd_tc, "	\t}\n");
  fprintf(fd_tc, "	}\n");
  fprintf(fd_tc, "	return 1;\n");
  fprintf(fd_tc, "}\n");
}

static void putproc(ProcList *p) {
  Pid_nr = p->tn;
  Det = p->det;

  if (pid_is_claim(Pid_nr) && launch_settings.separate_version == 1) {
    fprintf(fd_th, "extern uchar reached%d[];\n", Pid_nr);
    fprintf(fd_th, "\n#define _nstates%d	%d\t/* %s */\n", Pid_nr,
            p->s->maxel, p->n->name.c_str());
    fprintf(fd_th, "extern short src_ln%d[];\n", Pid_nr);
    fprintf(fd_th, "extern uchar *loopstate%d;\n", Pid_nr);
    fprintf(fd_th, "extern S_F_MAP src_file%d[];\n", Pid_nr);
    fprintf(fd_th, "#define _endstate%d	%d\n", Pid_nr,
            p->s->last ? p->s->last->seqno : 0);
    return;
  }
  if (!pid_is_claim(Pid_nr) && launch_settings.separate_version == 2) {
    fprintf(fd_th, "extern short src_ln%d[];\n", Pid_nr);
    fprintf(fd_th, "extern uchar *loopstate%d;\n", Pid_nr);
    return;
  }

  AllGlobal = (p->prov) ? 1 : 0; /* process has provided clause */

  fprintf(fd_th, "\n#define _nstates%d	%d\t/* %s */\n", Pid_nr, p->s->maxel,
          p->n->name.c_str());
  /* new */
  fprintf(fd_th, "#define minseq%d	%d\n", Pid_nr, find_min(p->s));
  fprintf(fd_th, "#define maxseq%d	%d\n", Pid_nr, find_max(p->s));

  /* end */

  if (Pid_nr == eventmapnr)
    fprintf(fd_th, "#define nstates_event	_nstates%d\n", Pid_nr);

  fprintf(fd_th, "#define _endstate%d	%d\n", Pid_nr,
          p->s->last ? p->s->last->seqno : 0);

  if (p->b == N_CLAIM || p->b == E_TRACE || p->b == N_TRACE) {
    fprintf(fd_tm, "\n		 /* CLAIM %s */\n", p->n->name.c_str());
    fprintf(fd_tb, "\n		 /* CLAIM %s */\n", p->n->name.c_str());
  } else {
    fprintf(fd_tm, "\n		 /* PROC %s */\n", p->n->name.c_str());
    fprintf(fd_tb, "\n		 /* PROC %s */\n", p->n->name.c_str());
  }
  fprintf(fd_tt, "\n	/* proctype %d: %s */\n", Pid_nr, p->n->name.c_str());
  fprintf(fd_tt, "\n	trans[%d] = (Trans **)", Pid_nr);
  fprintf(fd_tt, " emalloc(%d*sizeof(Trans *));\n\n", p->s->maxel);

  if (Pid_nr == eventmapnr) {
    fprintf(fd_th, "\n#define in_s_scope(x_y3_)	0");
    fprintf(fd_tc, "\n#define in_r_scope(x_y3_)	0");
  }
  put_seq(p->s, 2, 0);
  if (Pid_nr == eventmapnr) {
    fprintf(fd_th, "\n\n");
    fprintf(fd_tc, "\n\n");
  }
  dumpsrc(p->s->maxel, Pid_nr);
}

static void addTpe(int x) {
  int i;

  if (x <= 2)
    return;

  for (i = 0; i < T_sum; i++)
    if (TPE[i] == x)
      return;
  TPE[(T_sum++) % 2] = x;
}

static void cnt_seq(Sequence *s) {
  Element *f;
  SeqList *h;

  if (s)
    for (f = s->frst; f; f = f->nxt) {
      Tpe(f->n); /* sets EPT */
      addTpe(EPT[0]);
      addTpe(EPT[1]);
      for (h = f->sub; h; h = h->nxt)
        cnt_seq(h->this_sequence);
      if (f == s->last)
        break;
    }
}

static void typ_seq(Sequence *s) {
  T_sum = 0;
  TPE[0] = 2;
  TPE[1] = 0;
  cnt_seq(s);
  if (T_sum > 2) /* more than one type */
  {
    TPE[0] = 5 * DELTA; /* non-mixing */
    TPE[1] = 0;
  }
}

static int hidden_flags(Lextok *n) {
  if (n)
    switch (n->ntyp) {
    case FULL:
    case EMPTY:
    case NFULL:
    case NEMPTY:
    case TIMEOUT:
      Nn[(T_mus++) % 2] = n;
      break;
    case '!':
    case UMIN:
    case '~':
    case ASSERT:
    case 'c':
      (void)hidden_flags(n->lft);
      break;
    case '/':
    case '*':
    case '-':
    case '+':
    case '%':
    case LT:
    case GT:
    case '&':
    case '^':
    case '|':
    case LE:
    case GE:
    case NE:
    case '?':
    case EQ:
    case OR:
    case AND:
    case LSHIFT:
    case RSHIFT:
      (void)hidden_flags(n->lft);
      (void)hidden_flags(n->rgt);
      break;
    }
  return T_mus;
}

static int getNid(Lextok *n) {
  if (n->sym && n->sym->type == STRUCT && n->rgt && n->rgt->lft)
    return getNid(n->rgt->lft);

  if (!n->sym || n->sym->id == 0) {
    char *no_name = "no name";
    loger::fatal("bad channel name '%s'", (n->sym) ? n->sym->name : no_name);
  }
  return n->sym->id;
}

static int valTpe(Lextok *n) {
  int res = 2;
  /*
  2 = local
  2+1	    .. 2+1*DELTA = nfull,  's'	- require q_full==false
  2+1+1*DELTA .. 2+2*DELTA = nempty, 'r'	- require q_len!=0
  2+1+2*DELTA .. 2+3*DELTA = empty	- require q_len==0
  2+1+3*DELTA .. 2+4*DELTA = full		- require q_full==true
  5*DELTA = non-mixing (i.e., always makes the selection global)
  6*DELTA = timeout (conditionally safe)
  7*DELTA = @, process deletion (conditionally safe)
   */
  switch (n->ntyp) { /* a series of fall-thru cases: */
  case FULL:
    res += DELTA; /* add 3*DELTA + chan nr */
  case EMPTY:
    res += DELTA; /* add 2*DELTA + chan nr */
  case 'r':
  case NEMPTY:
    res += DELTA; /* add 1*DELTA + chan nr */
  case 's':
  case NFULL:
    res += getNid(n->lft); /* add channel nr */
    break;

  case TIMEOUT:
    res = 6 * DELTA;
    break;
  case '@':
    res = 7 * DELTA;
    break;
  default:
    break;
  }
  return res;
}

static void Tpe(Lextok *n) /* mixing in selections */
{
  EPT[0] = 2;
  EPT[1] = 0;

  if (!n)
    return;

  T_mus = 0;
  Nn[0] = Nn[1] = ZN;

  if (n->ntyp == 'c') {
    if (hidden_flags(n->lft) > 2) {
      EPT[0] = 5 * DELTA; /* non-mixing */
      EPT[1] = 0;
      return;
    }
  } else
    Nn[0] = n;

  if (Nn[0])
    EPT[0] = valTpe(Nn[0]);
  if (Nn[1])
    EPT[1] = valTpe(Nn[1]);
}

static void put_escp(Element *e) {
  int n;
  SeqList *x;

  if (e->esc /* && e->n->ntyp != GOTO */ && e->n->ntyp != '.') {
    for (x = e->esc, n = 0; x; x = x->nxt, n++) {
      int i = huntele(x->this_sequence->frst, e->status, -1)->seqno;
      fprintf(fd_tt, "\ttrans[%d][%d]->escp[%d] = %d;\n", Pid_nr, e->seqno, n,
              i);
      fprintf(fd_tt, "\treached%d[%d] = 1;\n", Pid_nr, i);
    }
    for (x = e->esc, n = 0; x; x = x->nxt, n++) {
      fprintf(fd_tt, "	/* escape #%d: %d */\n", n,
              huntele(x->this_sequence->frst, e->status, -1)->seqno);
      put_seq(x->this_sequence, 2, 0); /* args?? */
    }
    fprintf(fd_tt, "	/* end-escapes */\n");
  }
}

static void put_sub(Element *e, int Tt0, int Tt1) {
  Sequence *s = e->n->sl->this_sequence;
  Element *g = ZE;
  int a;

  patch_atomic(s);
  putskip(s->frst->seqno);
  g = huntstart(s->frst);
  a = g->seqno;

  if (0)
    printf("put_sub %d -> %d -> %d\n", e->seqno, s->frst->seqno, a);

  if ((e->n->ntyp == ATOMIC || e->n->ntyp == D_STEP) && scan_seq(s))
    mark_seq(s);
  s->last->nxt = e->nxt;

  typ_seq(s); /* sets TPE */

  if (e->n->ntyp == D_STEP) {
    int inherit = (e->status & (ATOM | L_ATOM));
    fprintf(fd_tm, "\tcase %d: ", uniq++);
    fprintf(fd_tm, "// STATE %d - %s:%d - [", e->seqno, e->n->fn->name.c_str(),
            e->n->ln);
    comment(fd_tm, e->n, 0);
    fprintf(fd_tm, "]\n\t\t");

    if (s->last->n->ntyp == BREAK)
      OkBreak = target(huntele(s->last->nxt, s->last->status, -1))->Seqno;
    else
      OkBreak = -1;

    if (!putcode(fd_tm, s, e->nxt, 0, e->n->ln, e->seqno)) {
      fprintf(fd_tm, "\n#if defined(C_States) && (HAS_TRACK==1)\n");
      fprintf(fd_tm, "\t\tc_update((uchar *) &(now.c_state[0]));\n");
      fprintf(fd_tm, "#endif\n");

      fprintf(fd_tm, "\t\t_m = %d", getweight(s->frst->n));
      if (launch_settings.need_lose_msgs_sent_to_full_queues &&
          s->frst->n->ntyp == 's')
        fprintf(fd_tm, "+delta_m; delta_m = 0");
      fprintf(fd_tm, "; goto P999;\n\n");
    }

    fprintf(fd_tb, "\tcase %d: ", uniq - 1);
    fprintf(fd_tb, "// STATE %d\n", e->seqno);
    fprintf(fd_tb, "\t\tsv_restor();\n");
    fprintf(fd_tb, "\t\tgoto R999;\n");
    if (e->nxt)
      a = huntele(e->nxt, e->status, -1)->seqno;
    else
      a = 0;
    tr_map(uniq - 1, e);
    fprintf(fd_tt, "/*->*/\ttrans[%d][%d]\t= ", Pid_nr, e->seqno);
    fprintf(fd_tt, "settr(%d,%d,%d,%d,%d,\"", e->Seqno, D_ATOM | inherit, a,
            uniq - 1, uniq - 1);
    in_settr++;
    comment(fd_tt, e->n, e->seqno);
    in_settr--;
    fprintf(fd_tt, "\", %d, ", (s->frst->status & I_GLOB) ? 1 : 0);
    fprintf(fd_tt, "%d, %d);\n", TPE[0], TPE[1]);
    put_escp(e);
  } else { /* ATOMIC or NON_ATOMIC */
    fprintf(fd_tt, "\tT = trans[ %d][%d] = ", Pid_nr, e->seqno);
    fprintf(fd_tt, "settr(%d,%d,0,0,0,\"", e->Seqno,
            (e->n->ntyp == ATOMIC) ? ATOM : 0);
    in_settr++;
    comment(fd_tt, e->n, e->seqno);
    in_settr--;
    if ((e->status & CHECK2) || (g->status & CHECK2))
      s->frst->status |= I_GLOB;
    fprintf(fd_tt, "\", %d, %d, %d);", (s->frst->status & I_GLOB) ? 1 : 0, Tt0,
            Tt1);
    blurb(fd_tt, e);
    fprintf(fd_tt, "\tT->nxt\t= ");
    fprintf(fd_tt, "settr(%d,%d,%d,0,0,\"", e->Seqno,
            (e->n->ntyp == ATOMIC) ? ATOM : 0, a);
    in_settr++;
    comment(fd_tt, e->n, e->seqno);
    in_settr--;
    fprintf(fd_tt, "\", %d, ", (s->frst->status & I_GLOB) ? 1 : 0);
    if (e->n->ntyp == NON_ATOMIC) {
      fprintf(fd_tt, "%d, %d);", Tt0, Tt1);
      blurb(fd_tt, e);
      put_seq(s, Tt0, Tt1);
    } else {
      fprintf(fd_tt, "%d, %d);", TPE[0], TPE[1]);
      blurb(fd_tt, e);
      put_seq(s, TPE[0], TPE[1]);
    }
  }
}

struct CaseCache {
  int m, b, owner;
  Element *e;
  Lextok *n;
  FSM_use *u;
  struct CaseCache *nxt;
};

static CaseCache *casing[6];

static int identical(Lextok *p, Lextok *q) {
  if ((!p && q) || (p && !q))
    return 0;
  if (!p)
    return 1;

  if (p->ntyp != q->ntyp || p->ismtyp != q->ismtyp || p->val != q->val ||
      p->indstep != q->indstep || p->sym != q->sym || p->sq != q->sq ||
      p->sl != q->sl)
    return 0;

  return identical(p->lft, q->lft) && identical(p->rgt, q->rgt);
}

static int samedeads(FSM_use *a, FSM_use *b) {
  FSM_use *p, *q;

  for (p = a, q = b; p && q; p = p->nxt, q = q->nxt)
    if (p->var != q->var || p->special != q->special)
      return 0;
  return (!p && !q);
}

static Element *findnext(Element *f) {
  Element *g;

  if (f->n->ntyp == GOTO) {
    g = get_lab(f->n, 1);
    return huntele(g, f->status, -1);
  }
  return f->nxt;
}

static Element *advance(Element *e, int stopat) {
  Element *f = e;

  if (stopat)
    while (f && f->seqno != stopat) {
      f = findnext(f);
      if (!f) {
        break;
      }
      switch (f->n->ntyp) {
      case GOTO:
      case '.':
      case PRINT:
      case PRINTM:
        break;
      default:
        return f;
      }
    }
  return (Element *)0;
}

static int equiv_merges(Element *a, Element *b) {
  Element *f, *g;
  int stopat_a, stopat_b;

  if (a->merge_start)
    stopat_a = a->merge_start;
  else
    stopat_a = a->merge;

  if (b->merge_start)
    stopat_b = b->merge_start;
  else
    stopat_b = b->merge;

  if (!stopat_a && !stopat_b)
    return 1;

  f = advance(a, stopat_a);
  g = advance(b, stopat_b);

  if (!f && !g)
    return 1;

  if (f && g)
    return identical(f->n, g->n);

  return 0;
}

static CaseCache *prev_case(Element *e, int owner) {
  int j;
  CaseCache *nc;

  switch (e->n->ntyp) {
  case 'r':
    j = 0;
    break;
  case 's':
    j = 1;
    break;
  case 'c':
    j = 2;
    break;
  case ASGN:
    j = 3;
    break;
  case ASSERT:
    j = 4;
    break;
  default:
    j = 5;
    break;
  }
  for (nc = casing[j]; nc; nc = nc->nxt)
    if (identical(nc->n, e->n) && samedeads(nc->u, e->dead) &&
        equiv_merges(nc->e, e) && nc->owner == owner)
      return nc;

  return (CaseCache *)0;
}

static void new_case(Element *e, int m, int b, int owner) {
  int j;
  CaseCache *nc;

  switch (e->n->ntyp) {
  case 'r':
    j = 0;
    break;
  case 's':
    j = 1;
    break;
  case 'c':
    j = 2;
    break;
  case ASGN:
    j = 3;
    break;
  case ASSERT:
    j = 4;
    break;
  default:
    j = 5;
    break;
  }
  nc = (CaseCache *)emalloc(sizeof(CaseCache));
  nc->owner = owner;
  nc->m = m;
  nc->b = b;
  nc->e = e;
  nc->n = e->n;
  nc->u = e->dead;
  nc->nxt = casing[j];
  casing[j] = nc;
}

static int nr_bup(Element *e) {
  FSM_use *u;
  Lextok *v;
  int nr = 0;

  switch (e->n->ntyp) {
  case ASGN:
    if (check_track(e->n) == STRUCT) {
      break;
    }
    nr++;
    break;
  case 'r':
    if (e->n->val >= 1)
      nr++; /* random recv */
    for (v = e->n->rgt; v; v = v->rgt) {
      if ((v->lft->ntyp == CONST || v->lft->ntyp == EVAL))
        continue;
      nr++;
    }
    break;
  default:
    break;
  }
  for (u = e->dead; u; u = u->nxt) {
    switch (u->special) {
    case 2: /* dead after write */
      if (e->n->ntyp == ASGN && e->n->rgt->ntyp == CONST && e->n->rgt->val == 0)
        break;
      nr++;
      break;
    case 1: /* dead after read */
      nr++;
      break;
    }
  }
  return nr;
}

static int nrhops(Element *e) {
  Element *f = e, *g;
  int cnt = 0;
  int stopat;

  if (e->merge_start)
    stopat = e->merge_start;
  else
    stopat = e->merge;
  do {
    cnt += nr_bup(f);

    if (f->n->ntyp == GOTO) {
      g = get_lab(f->n, 1);
      if (g->seqno == stopat)
        f = g;
      else
        f = huntele(g, f->status, stopat);
    } else {
      f = f->nxt;
    }

    if (f && !f->merge && !f->merge_single && f->seqno != stopat) {
      fprintf(fd_tm, "\n\t\t// bad hop %s:%d -- at %d, <",
              f->n->fn->name.c_str(), f->n->ln, f->seqno);
      comment(fd_tm, f->n, 0);
      fprintf(fd_tm, "> looking for %d -- merge %d:%d:%d ", stopat, f->merge,
              f->merge_start, f->merge_single);
      break;
    }
  } while (f && f->seqno != stopat);

  return cnt;
}

static void check_needed(void) {
  if (multi_needed) {
    fprintf(fd_tm, "(trpt+1)->bup.ovals = grab_ints(%d);\n\t\t", multi_needed);
    multi_undo = multi_needed;
    multi_needed = 0;
  }
}

static void doforward(FILE *tm_fd, Element *e) {
  FSM_use *u;

  putstmnt(tm_fd, e->n, e->seqno);

  if (e->n->ntyp != ELSE && Det) {
    fprintf(tm_fd, ";\n\t\tif (trpt->o_pm&1)\n\t\t");
    fprintf(tm_fd, "\tuerror(\"non-determinism in D_proctype\")");
  }
  if (launch_settings.need_hide_write_only_variables && !lexer_.GetHasCode())
    for (u = e->dead; u; u = u->nxt) {
      fprintf(tm_fd, ";\n\t\t");
      fprintf(tm_fd, "if (TstOnly) return 1; /* TT */\n");
      fprintf(tm_fd, "\t\t/* dead %d: %s */  ", u->special,
              u->var->name.c_str());

      switch (u->special) {
      case 2:                   /* dead after write -- lval already bupped */
        if (e->n->ntyp == ASGN) /* could be recv or asgn */
        {
          if (e->n->rgt->ntyp == CONST && e->n->rgt->val == 0)
            continue; /* already set to 0 */
        }
        if (e->n->ntyp != 'r') {
          XZ.sym = u->var;
          fprintf(tm_fd, "\n#ifdef HAS_CODE\n");
          fprintf(tm_fd, "\t\tif (!readtrail)\n");
          fprintf(tm_fd, "#endif\n\t\t\t");
          putname(tm_fd, "", &XZ, 0, " = 0");
          break;
        }     /* else fall through */
      case 1: /* dead after read -- add asgn of rval -- needs bup */
        YZ[YZmax].sym = u->var; /* store for pan.b */
        CnT[YZcnt]++;           /* this step added bups */
        if (multi_oval) {
          check_needed();
          fprintf(tm_fd, "(trpt+1)->bup.ovals[%d] = ", multi_oval - 1);
          multi_oval++;
        } else
          fprintf(tm_fd, "(trpt+1)->bup.oval = ");
        putname(tm_fd, "", &YZ[YZmax], 0, ";\n");
        fprintf(tm_fd, "#ifdef HAS_CODE\n");
        fprintf(tm_fd, "\t\tif (!readtrail)\n");
        fprintf(tm_fd, "#endif\n\t\t\t");
        putname(tm_fd, "", &YZ[YZmax], 0, " = 0");
        YZmax++;
        break;
      }
    }
  fprintf(tm_fd, ";\n\t\t");
}

static int dobackward(Element *e, int casenr) {
  if (!any_undo(e->n) && CnT[YZcnt] == 0) {
    YZcnt--;
    return 0;
  }

  if (!didcase) {
    fprintf(fd_tb, "\n\tcase %d: ", casenr);
    fprintf(fd_tb, "// STATE %d\n\t\t", e->seqno);
    didcase++;
  }

  _isok++;
  while (CnT[YZcnt] > 0) /* undo dead variable resets */
  {
    CnT[YZcnt]--;
    YZmax--;
    if (YZmax < 0)
      loger::fatal("cannot happen, dobackward");
    fprintf(fd_tb, ";\n\t/* %d */\t", YZmax);
    putname(fd_tb, "", &YZ[YZmax], 0, " = trpt->bup.oval");
    if (multi_oval > 0) {
      multi_oval--;
      fprintf(fd_tb, "s[%d]", multi_oval - 1);
    }
  }

  if (e->n->ntyp != '.') {
    fprintf(fd_tb, ";\n\t\t");
    undostmnt(e->n, e->seqno);
  }
  _isok--;

  YZcnt--;
  return 1;
}

static void lastfirst(int stopat, Element *fin, int casenr) {
  Element *f = fin, *g;

  if (f->n->ntyp == GOTO) {
    g = get_lab(f->n, 1);
    if (g->seqno == stopat)
      f = g;
    else
      f = huntele(g, f->status, stopat);
  } else
    f = f->nxt;

  if (!f || f->seqno == stopat || (!f->merge && !f->merge_single))
    return;
  lastfirst(stopat, f, casenr);
  dobackward(f, casenr);
}

static int modifier;

static void lab_transfer(Element *to, Element *from) {
  models::Symbol *ns, *s = has_lab(from, (1 | 2 | 4));
  models::Symbol *oc;
  int ltp, usedit = 0;

  if (!s)
    return;

  /* "from" could have all three labels -- rename
   * to prevent jumps to the transfered copies
   */
  oc = context;                    /* remember */
  for (ltp = 1; ltp < 8; ltp *= 2) /* 1, 2, and 4 */
    if ((s = has_lab(from, ltp)) != (models::Symbol *)0) {
      ns = (models::Symbol *)emalloc(sizeof(models::Symbol));
      ns->name = (char *)emalloc((int)strlen(s->name.c_str()) + 4);
      ns->name = fmt::format("{}{}", s->name, modifier);

      context = s->context;
      set_lab(ns, to);
      usedit++;
    }
  context = oc; /* restore */
  if (usedit) {
    if (modifier++ > 990)
      loger::fatal("modifier overflow error");
  }
}

static int case_cache(Element *e, int a) {
  int bupcase = 0, casenr = uniq, fromcache = 0;
  CaseCache *Cached = (CaseCache *)0;
  Element *f, *g;
  int j, nrbups, mark, ntarget;

  mark = (e->status & ATOM); /* could lose atomicity in a merge chain */

  if (e->merge_mark > 0 ||
      (launch_settings.need_statemate_merging &&
       e->merge_in ==
           0)) { /* state nominally unreachable (part of merge chains) */
    if (e->n->ntyp != '.' && e->n->ntyp != GOTO) {
      fprintf(fd_tt, "\ttrans[%d][%d]\t= ", Pid_nr, e->seqno);
      fprintf(fd_tt, "settr(0,0,0,0,0,\"");
      in_settr++;
      comment(fd_tt, e->n, e->seqno);
      in_settr--;
      fprintf(fd_tt, "\",0,0,0);\n");
    } else {
      fprintf(fd_tt, "\ttrans[%d][%d]\t= ", Pid_nr, e->seqno);
      casenr = 1; /* mhs example */
      j = a;
      goto haveit; /* pakula's example */
    }

    return -1;
  }

  fprintf(fd_tt, "\ttrans[%d][%d]\t= ", Pid_nr, e->seqno);

  if (launch_settings.need_case_caching && !pid_is_claim(Pid_nr) &&
      Pid_nr != eventmapnr && (Cached = prev_case(e, Pid_nr))) {
    bupcase = Cached->b;
    casenr = Cached->m;
    fromcache = 1;

    fprintf(fd_tm, "// STATE %d - %s:%d - [", e->seqno, e->n->fn->name.c_str(),
            e->n->ln);
    comment(fd_tm, e->n, 0);
    fprintf(fd_tm, "] (%d:%d - %d) same as %d (%d:%d - %d)\n", e->merge_start,
            e->merge, e->merge_in, casenr, Cached->e->merge_start,
            Cached->e->merge, Cached->e->merge_in);

    goto gotit;
  }

  fprintf(fd_tm, "\tcase %d: // STATE %d - %s:%d - [", uniq++, e->seqno,
          e->n->fn->name.c_str(), e->n->ln);
  comment(fd_tm, e->n, 0);
  nrbups = (e->merge || e->merge_start) ? nrhops(e) : nr_bup(e);
  fprintf(fd_tm, "] (%d:%d:%d - %d)\n\t\t", e->merge_start, e->merge, nrbups,
          e->merge_in);

  if (nrbups > MAXMERGE - 1)
    loger::fatal("merge requires more than 256 bups");

  if (e->n->ntyp != 'r' && !pid_is_claim(Pid_nr) && Pid_nr != eventmapnr)
    fprintf(fd_tm, "IfNotBlocked\n\t\t");

  if (multi_needed != 0 || multi_undo != 0)
    loger::fatal("cannot happen, case_cache");

  if (nrbups > 1) {
    multi_oval = 1;
    multi_needed = nrbups; /* allocated after edge condition */
  } else
    multi_oval = 0;

  memset(CnT, 0, sizeof(CnT));
  YZmax = YZcnt = 0;

  /* new 4.2.6, revised 6.0.0 */
  if (pid_is_claim(Pid_nr)) {
    fprintf(fd_tm, "\n#if defined(VERI) && !defined(NP)\n");
    fprintf(fd_tm, "#if NCLAIMS>1\n\t\t");
    fprintf(fd_tm, "{	static int reported%d = 0;\n\t\t", e->seqno);
    fprintf(fd_tm, "	if (verbose && !reported%d)\n\t\t", e->seqno);
    fprintf(fd_tm, "	{	int nn = (int) ((Pclaim *)pptr(0))->_n;\n\t\t");
    fprintf(fd_tm, "		printf(\"depth %%ld: Claim %%s (%%d), state "
                   "%%d (line %%d)\\n\",\n\t\t");
    fprintf(fd_tm, "			depth, procname[spin_c_typ[nn]], nn, ");
    fprintf(fd_tm, "(int) ((Pclaim *)pptr(0))->_p, src_claim[ (int) ((Pclaim "
                   "*)pptr(0))->_p ]);\n\t\t");
    fprintf(fd_tm, "		reported%d = 1;\n\t\t", e->seqno);
    fprintf(fd_tm, "		fflush(stdout);\n\t\t");
    fprintf(fd_tm, "}	}\n");
    fprintf(fd_tm, "#else\n\t\t");
    fprintf(fd_tm, "{	static int reported%d = 0;\n\t\t", e->seqno);
    fprintf(fd_tm, "	if (verbose && !reported%d)\n\t\t", e->seqno);
    fprintf(fd_tm, "	{	printf(\"depth %%d: Claim, state %%d (line "
                   "%%d)\\n\",\n\t\t");
    fprintf(fd_tm,
            "			(int) depth, (int) ((Pclaim *)pptr(0))->_p, ");
    fprintf(fd_tm, "src_claim[ (int) ((Pclaim *)pptr(0))->_p ]);\n\t\t");
    fprintf(fd_tm, "		reported%d = 1;\n\t\t", e->seqno);
    fprintf(fd_tm, "		fflush(stdout);\n\t\t");
    fprintf(fd_tm, "}	}\n");
    fprintf(fd_tm, "#endif\n");
    fprintf(fd_tm, "#endif\n\t\t");
  }
  /* end */

  /* the src xrefs have the numbers in e->seqno builtin */
  fprintf(fd_tm, "reached[%d][%d] = 1;\n\t\t", Pid_nr, e->seqno);

  doforward(fd_tm, e);

  if (e->merge_start)
    ntarget = e->merge_start;
  else
    ntarget = e->merge;

  if (ntarget) {
    f = e;

  more:
    if (f->n->ntyp == GOTO) {
      g = get_lab(f->n, 1);
      if (g->seqno == ntarget)
        f = g;
      else
        f = huntele(g, f->status, ntarget);
    } else
      f = f->nxt;

    if (f && f->seqno != ntarget) {
      if (!f->merge && !f->merge_single) {
        fprintf(fd_tm, "/* stop at bad hop %d, %d */\n\t\t", f->seqno, ntarget);
        goto out;
      }
      fprintf(fd_tm, "/* merge: ");
      comment(fd_tm, f->n, 0);
      fprintf(fd_tm, "(%d, %d, %d) */\n\t\t", f->merge, f->seqno, ntarget);
      fprintf(fd_tm, "reached[%d][%d] = 1;\n\t\t", Pid_nr, f->seqno);
      YZcnt++;
      lab_transfer(e, f);
      mark = f->status & (ATOM | L_ATOM); /* last step wins */
      doforward(fd_tm, f);
      if (f->merge_in == 1)
        f->merge_mark++;

      goto more;
    }
  }
out:
  fprintf(fd_tm, "_m = %d", getweight(e->n));
  if (launch_settings.need_lose_msgs_sent_to_full_queues && e->n->ntyp == 's')
    fprintf(fd_tm, "+delta_m; delta_m = 0");
  fprintf(fd_tm, "; goto P999; /* %d */\n", YZcnt);

  multi_needed = 0;
  didcase = 0;

  if (ntarget)
    lastfirst(ntarget, e, casenr); /* mergesteps only */

  dobackward(e, casenr); /* the original step */

  fprintf(fd_tb, ";\n\t\t");

  if (e->merge || e->merge_start) {
    if (!didcase) {
      fprintf(fd_tb, "\n\tcase %d: ", casenr);
      fprintf(fd_tb, "// STATE %d", e->seqno);
      didcase++;
    } else
      fprintf(fd_tb, ";");
  } else
    fprintf(fd_tb, ";");
  fprintf(fd_tb, "\n\t\t");

  if (multi_undo) {
    fprintf(fd_tb, "ungrab_ints(trpt->bup.ovals, %d);\n\t\t", multi_undo);
    multi_undo = 0;
  }
  if (didcase) {
    fprintf(fd_tb, "goto R999;\n");
    bupcase = casenr;
  }

  if (!e->merge && !e->merge_start)
    new_case(e, casenr, bupcase, Pid_nr);

gotit:
  j = a;
  if (e->merge_start)
    j = e->merge_start;
  else if (e->merge)
    j = e->merge;
haveit:
  fprintf(fd_tt, "%ssettr(%d,%d,%d,%d,%d,\"", fromcache ? "/* c */ " : "",
          e->Seqno, mark, j, casenr, bupcase);

  return (fromcache) ? 0 : casenr;
}

static void put_el(Element *e, int Tt0, int Tt1) {
  int a, casenr, Global_ref;
  Element *g = ZE;

  if (e->n->ntyp == GOTO) {
    g = get_lab(e->n, 1);
    g = huntele(g, e->status, -1);
    cross_dsteps(e->n, g->n);
    a = g->seqno;
  } else if (e->nxt) {
    g = huntele(e->nxt, e->status, -1);
    a = g->seqno;
  } else
    a = 0;
  if (g && ((g->status & CHECK2)      /* entering remotely ref'd state */
            || (e->status & CHECK2))) /* leaving  remotely ref'd state */
    e->status |= I_GLOB;

  /* don't remove dead edges in here, to preserve structure of fsm */
  if (e->merge_start || e->merge)
    goto non_generic;

  /*** avoid duplicate or redundant cases in pan.m ***/
  switch (e->n->ntyp) {
  case ELSE:
    casenr = 2; /* standard else */
    putskip(e->seqno);
    goto generic_case;
    /* break; */
  case '.':
  case GOTO:
  case BREAK:
    putskip(e->seqno);
    casenr = 1; /* standard goto */
  generic_case:
    fprintf(fd_tt, "\ttrans[%d][%d]\t= ", Pid_nr, e->seqno);
    fprintf(fd_tt, "settr(%d,%d,%d,%d,0,\"", e->Seqno, e->status & ATOM, a,
            casenr);
    break;
#ifndef PRINTF
  case PRINT:
    goto non_generic;
  case PRINTM:
    goto non_generic;
#endif
  case 'c':
    if (e->n->lft->ntyp == CONST && e->n->lft->val == 1) /* skip or true */
    {
      casenr = 1;
      putskip(e->seqno);
      goto generic_case;
    }
    goto non_generic;

  default:
  non_generic:
    casenr = case_cache(e, a);
    if (casenr < 0)
      return; /* unreachable state */
    break;
  }
  /* tailend of settr(...); */
  Global_ref = (e->status & I_GLOB) ? 1 : has_global(e->n);
  in_settr++;
  comment(fd_tt, e->n, e->seqno);
  in_settr--;
  fprintf(fd_tt, "\", %d, ", Global_ref);
  if (Tt0 != 2) {
    fprintf(fd_tt, "%d, %d);", Tt0, Tt1);
  } else {
    Tpe(e->n); /* sets EPT */
    fprintf(fd_tt, "%d, %d);", EPT[0], EPT[1]);
  }
  if ((e->merge_start && e->merge_start != a) || (e->merge && e->merge != a)) {
    fprintf(fd_tt, " /* m: %d -> %d,%d */\n", a, e->merge_start, e->merge);
    fprintf(fd_tt, "	reached%d[%d] = 1;", Pid_nr,
            a); /* Sheinman's example */
  }
  fprintf(fd_tt, "\n");

  if (casenr > 2)
    tr_map(casenr, e);
  put_escp(e);
}

static void nested_unless(Element *e, Element *g) {
  struct SeqList *y = e->esc, *z = g->esc;

  for (; y && z; y = y->nxt, z = z->nxt)
    if (z->this_sequence != y->this_sequence)
      break;
  if (!y && !z)
    return;

  if (g->n->ntyp != GOTO && g->n->ntyp != '.' && e->sub->nxt) {
    printf("error: (%s:%d) saw 'unless' on a guard:\n",
           (e->n) ? e->n->fn->name.c_str() : "-", (e->n) ? e->n->ln : 0);
    printf("=====>instead of\n");
    printf("	do (or if)\n");
    printf("	:: ...\n");
    printf("	:: stmnt1 unless stmnt2\n");
    printf("	od (of fi)\n");
    printf("=====>use\n");
    printf("	do (or if)\n");
    printf("	:: ...\n");
    printf("	:: stmnt1\n");
    printf("	od (or fi) unless stmnt2\n");
    printf("=====>or rewrite\n");
  }
}

static void put_seq(Sequence *s, int Tt0, int Tt1) {
  SeqList *h;
  Element *e, *g;
  int a, deadlink;
  auto &verbose_flags = utils::verbose::Flags::getInstance();
  if (0)
    printf("put_seq %d\n", s->frst->seqno);

  for (e = s->frst; e; e = e->nxt) {
    if (0)
      printf("	step %d\n", e->seqno);
    if (e->status & DONE) {
      if (0)
        printf("		done before\n");
      goto checklast;
    }
    e->status |= DONE;

    if (e->n->ln)
      putsrc(e);

    if (e->n->ntyp == UNLESS) {
      if (0)
        printf("		an unless\n");
      put_seq(e->sub->this_sequence, Tt0, Tt1);
    } else if (e->sub) {
      if (0)
        printf("		has sub\n");
      fprintf(fd_tt, "\tT = trans[%d][%d] = ", Pid_nr, e->seqno);
      fprintf(fd_tt, "settr(%d,%d,0,0,0,\"", e->Seqno, e->status & ATOM);
      in_settr++;
      comment(fd_tt, e->n, e->seqno);
      in_settr--;
      if (e->status & CHECK2)
        e->status |= I_GLOB;
      fprintf(fd_tt, "\", %d, %d, %d);", (e->status & I_GLOB) ? 1 : 0, Tt0,
              Tt1);
      blurb(fd_tt, e);
      for (h = e->sub; h; h = h->nxt) {
        putskip(h->this_sequence->frst->seqno);
        g = huntstart(h->this_sequence->frst);
        if (g->esc)
          nested_unless(e, g);
        a = g->seqno;

        if (g->n->ntyp == 'c' && g->n->lft->ntyp == CONST &&
            g->n->lft->val == 0 /* 0 or false */
            && !g->esc) {
          fprintf(fd_tt, "#if 0\n\t/* dead link: */\n");
          deadlink = 1;
          if (verbose_flags.NeedToPrintVerbose())
            printf("spin: %s:%d, warning, condition is always false\n",
                   g->n->fn ? g->n->fn->name.c_str() : "", g->n->ln);
        } else
          deadlink = 0;
        if (0)
          printf("			settr %d %d\n", a, 0);
        if (h->nxt)
          fprintf(fd_tt, "\tT = T->nxt\t= ");
        else
          fprintf(fd_tt, "\t    T->nxt\t= ");
        fprintf(fd_tt, "settr(%d,%d,%d,0,0,\"", e->Seqno, e->status & ATOM, a);
        in_settr++;
        comment(fd_tt, e->n, e->seqno);
        in_settr--;
        if (g->status & CHECK2)
          h->this_sequence->frst->status |= I_GLOB;
        fprintf(fd_tt, "\", %d, %d, %d);",
                (h->this_sequence->frst->status & I_GLOB) ? 1 : 0, Tt0, Tt1);
        blurb(fd_tt, e);
        if (deadlink)
          fprintf(fd_tt, "#endif\n");
      }
      for (h = e->sub; h; h = h->nxt)
        put_seq(h->this_sequence, Tt0, Tt1);
    } else {
      if (0)
        printf("		[non]atomic %d\n", e->n->ntyp);
      if (e->n->ntyp == ATOMIC || e->n->ntyp == D_STEP ||
          e->n->ntyp == NON_ATOMIC)
        put_sub(e, Tt0, Tt1);
      else {
        if (0)
          printf("			put_el %d\n", e->seqno);
        put_el(e, Tt0, Tt1);
      }
    }
  checklast:
    if (e == s->last)
      break;
  }
  if (0)
    printf("put_seq done\n");
}

static void patch_atomic(Sequence *s) /* catch goto's that break the chain */
{
  Element *f, *g;
  SeqList *h;

  for (f = s->frst; f; f = f->nxt) {
    if (f->n && f->n->ntyp == GOTO) {
      g = get_lab(f->n, 1);
      cross_dsteps(f->n, g->n);
      if ((f->status & (ATOM | L_ATOM)) && !(g->status & (ATOM | L_ATOM))) {
        f->status &= ~ATOM;
        f->status |= L_ATOM;
      }
      /* bridge atomics */
      if ((f->status & L_ATOM) && (g->status & (ATOM | L_ATOM))) {
        f->status &= ~L_ATOM;
        f->status |= ATOM;
      }
    } else
      for (h = f->sub; h; h = h->nxt)
        patch_atomic(h->this_sequence);
    if (f == s->extent)
      break;
  }
}

static void mark_seq(Sequence *s) {
  Element *f;
  SeqList *h;

  for (f = s->frst; f; f = f->nxt) {
    f->status |= I_GLOB;

    if (f->n->ntyp == ATOMIC || f->n->ntyp == NON_ATOMIC ||
        f->n->ntyp == D_STEP)
      mark_seq(f->n->sl->this_sequence);

    for (h = f->sub; h; h = h->nxt)
      mark_seq(h->this_sequence);
    if (f == s->last)
      return;
  }
}

static Element *find_target(Element *e) {
  Element *f;

  if (!e)
    return e;

  if (t_cyc++ > 32) {
    loger::fatal("cycle of goto jumps");
  }
  switch (e->n->ntyp) {
  case GOTO:
    f = get_lab(e->n, 1);
    cross_dsteps(e->n, f->n);
    f = find_target(f);
    break;
  case BREAK:
    if (e->nxt) {
      f = find_target(huntele(e->nxt, e->status, -1));
      break; /* new 5.0 -- was missing */
    }
    /* else fall through */
  default:
    f = e;
    break;
  }
  return f;
}

Element *target(Element *e) {
  if (!e)
    return e;
  lineno = e->n->ln;
  Fname = e->n->fn;
  t_cyc = 0;
  return find_target(e);
}

static int seq_has_el(Sequence *s, Element *g) /* new to version 5.0 */
{
  Element *f;
  SeqList *h;

  for (f = s->frst; f; f = f->nxt) /* g in same atomic? */
  {
    if (f == g) {
      return 1;
    }
    if (f->status & CHECK3) {
      continue;
    }
    f->status |= CHECK3; /* protect against cycles */
    for (h = f->sub; h; h = h->nxt) {
      if (h->this_sequence && seq_has_el(h->this_sequence, g)) {
        return 1;
      }
    }
  }
  return 0;
}

static int scan_seq(Sequence *s) {
  Element *f, *g;
  SeqList *h;

  for (f = s->frst; f; f = f->nxt) {
    if ((f->status & CHECK2) || has_global(f->n))
      return 1;
    if (f->n->ntyp == GOTO        /* may exit or reach other atomic */
        && !(f->status & D_ATOM)) /* cannot jump from d_step */
    { /* consider jump from an atomic without globals into
       * an atomic with globals
       * example by Claus Traulsen, 22 June 2007
       */
      g = target(f);
#if 1
      if (g && !seq_has_el(s, g)) /* not internal to this atomic/dstep */

#else
      if (g && !(f->status & L_ATOM) && !(g->status & (ATOM | L_ATOM)))
#endif
      {
        fprintf(fd_tt, "\t/* mark-down line %d status %d = %d */\n", f->n->ln,
                f->status, (f->status & D_ATOM));
        return 1; /* assume worst case */
      }
    }
    for (h = f->sub; h; h = h->nxt)
      if (scan_seq(h->this_sequence))
        return 1;
    if (f == s->last)
      break;
  }
  return 0;
}

static int glob_args(Lextok *n) {
  int result = 0;
  Lextok *v;

  for (v = n->rgt; v; v = v->rgt) {
    if (v->lft->ntyp == CONST)
      continue;
    if (v->lft->ntyp == EVAL)
      result += has_global(v->lft->lft);
    else
      result += has_global(v->lft);
  }
  return result;
}

static int proc_is_safe(const Lextok *n) {
  ProcList *p;
  /* not safe unless no local var inits are used */
  /* note that a local variable init could refer to a global */

  for (p = ready; p; p = p->nxt) {
    if (n->sym->name == p->n->name) { /* printf("proc %s safety: %d\n",
                                         p->n->name, p->unsafe); */
      return (p->unsafe != 0);
    }
  }
  /* cannot happen */
  return 0;
}

int has_global(Lextok *n) {
  Lextok *v;
  static models::Symbol *n_seen = (models::Symbol *)0;

  if (!n)
    return 0;
  if (AllGlobal)
    return 1; /* global provided clause */

  switch (n->ntyp) {
  case ATOMIC:
  case D_STEP:
  case NON_ATOMIC:
    return scan_seq(n->sl->this_sequence);

  case '.':
  case BREAK:
  case GOTO:
  case CONST:
    return 0;

  case ELSE:
    return n->val; /* true if combined with chan refs */

  case 's':
    return glob_args(n) != 0 || ((n->sym->xu & (XS | XX)) != XS);
  case 'r':
    return glob_args(n) != 0 || ((n->sym->xu & (XR | XX)) != XR);
  case 'R':
    return glob_args(n) != 0 || (((n->sym->xu) & (XR | XS | XX)) != (XR | XS));
  case NEMPTY:
    return ((n->sym->xu & (XR | XX)) != XR);
  case NFULL:
    return ((n->sym->xu & (XS | XX)) != XS);
  case FULL:
    return ((n->sym->xu & (XR | XX)) != XR);
  case EMPTY:
    return ((n->sym->xu & (XS | XX)) != XS);
  case LEN:
    return (((n->sym->xu) & (XR | XS | XX)) != (XR | XS));

  case NAME:
    if (n->sym->name == "_priority") {
      if (launch_settings.need_revert_old_rultes_for_priority) {
        if (n_seen != n->sym)
          loger::fatal("cannot refer to _priority with -o6");
        n_seen = n->sym;
      }
      return 0;
    }
    if (n->sym->context || (n->sym->hidden_flags & 64) ||
        n->sym->name == "_pid" || n->sym->name == "_")
      return 0;
    return 1;

  case RUN:
    return proc_is_safe(n);

  case C_CODE:
  case C_EXPR:
    return glob_inline(n->sym->name);

  case ENABLED:
  case PC_VAL:
  case NONPROGRESS:
  case 'p':
  case 'q':
  case TIMEOUT:
  case SET_P:
  case GET_P:
    return 1;

  /* 	@ was 1 (global) since 2.8.5
          in 3.0 it is considered local and
          conditionally safe, provided:
                  II is the youngest process
                  and nrprocs < MAXPROCS
  */
  case '@':
    return 0;

  case '!':
  case UMIN:
  case '~':
  case ASSERT:
    return has_global(n->lft);

  case '/':
  case '*':
  case '-':
  case '+':
  case '%':
  case LT:
  case GT:
  case '&':
  case '^':
  case '|':
  case LE:
  case GE:
  case NE:
  case '?':
  case EQ:
  case OR:
  case AND:
  case LSHIFT:
  case RSHIFT:
  case 'c':
  case ASGN:
    return has_global(n->lft) || has_global(n->rgt);

  case PRINT:
    for (v = n->lft; v; v = v->rgt)
      if (has_global(v->lft))
        return 1;
    return 0;
  case PRINTM:
    return has_global(n->lft);
  }
  return 0;
}

static void Bailout(FILE *fd, char *str) {
  if (!GenCode) {
    fprintf(fd, "continue%s", str);
  } else if (IsGuard) {
    fprintf(fd, "%s%s", NextLab[Level].c_str(), str);
  } else {
    fprintf(fd, "Uerror(\"block in d_step seq\")%s", str);
  }
}

#define cat0(x)                                                                \
  putstmnt(fd, now->lft, m);                                                   \
  fprintf(fd, x);                                                              \
  putstmnt(fd, now->rgt, m)
#define cat1(x)                                                                \
  fprintf(fd, "(");                                                            \
  cat0(x);                                                                     \
  fprintf(fd, ")")
#define cat2(x, y)                                                             \
  fprintf(fd, x);                                                              \
  putstmnt(fd, y, m)
#define cat3(x, y, z)                                                          \
  fprintf(fd, x);                                                              \
  putstmnt(fd, y, m);                                                          \
  fprintf(fd, z)
#define cat30(x, y, z)                                                         \
  fprintf(fd, x, 0);                                                           \
  putstmnt(fd, y, m);                                                          \
  fprintf(fd, z)

void dump_tree(const char *s, Lextok *p) {
  char z[64];

  if (!p)
    return;

  printf("\n%s:\t%2d:\t%3d (", s, p->ln, p->ntyp);
  std::cout << loger::explainToString(p->ntyp);
  if (p->ntyp == 315)
    printf(": %s", p->sym->name.c_str());
  if (p->ntyp == 312)
    printf(": %d", p->val);
  printf(")");

  if (p->lft) {
    sprintf(z, "%sL", s);
    dump_tree(z, p->lft);
  }
  if (p->rgt) {
    sprintf(z, "%sR", s);
    dump_tree(z, p->rgt);
  }
}

void putstmnt(FILE *fd, Lextok *now, int m) {
  Lextok *v;
  int i, j;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  if (!now) {
    fprintf(fd, "0");
    return;
  }
  lineno = now->ln;
  Fname = now->fn;

  switch (now->ntyp) {
  case CONST:
    fprintf(fd, "%d", now->val);
    break;
  case '!':
    cat3(" !(", now->lft, ")");
    break;
  case UMIN:
    cat3(" -(", now->lft, ")");
    break;
  case '~':
    cat3(" ~(", now->lft, ")");
    break;

  case '/':
    cat1("/");
    break;
  case '*':
    cat1("*");
    break;
  case '-':
    cat1("-");
    break;
  case '+':
    cat1("+");
    break;
  case '%':
    cat1("%%");
    break;
  case '&':
    cat1("&");
    break;
  case '^':
    cat1("^");
    break;
  case '|':
    cat1("|");
    break;
  case LT:
    cat1("<");
    break;
  case GT:
    cat1(">");
    break;
  case LE:
    cat1("<=");
    break;
  case GE:
    cat1(">=");
    break;
  case NE:
    cat1("!=");
    break;
  case EQ:
    cat1("==");
    break;
  case OR:
    cat1("||");
    break;
  case AND:
    cat1("&&");
    break;
  case LSHIFT:
    cat1("<<");
    break;
  case RSHIFT:
    cat1(">>");
    break;

  case TIMEOUT:
    if (launch_settings.separate_version == 2)
      fprintf(fd, "((tau)&1)");
    else
      fprintf(fd, "((trpt->tau)&1)");
    if (GenCode)
      printf("spin: %s:%d, warning, 'timeout' in d_step sequence\n",
             Fname->name.c_str(), lineno);
    /* is okay as a guard */
    break;

  case RUN:
    if (now->sym == NULL)
      loger::fatal("internal error pangen2.c");
    if (claimproc && strcmp(now->sym->name.c_str(), claimproc) == 0)
      loger::fatal("claim %s, (not runnable)", claimproc);
    if (eventmap && strcmp(now->sym->name.c_str(), eventmap) == 0)
      loger::fatal("eventmap %s, (not runnable)", eventmap);

    if (GenCode)
      loger::fatal("'run' in d_step sequence (use atomic)");

    fprintf(fd, "addproc(II, %d, %d",
            (now->val > 0 && !launch_settings.need_revert_old_rultes_for_priority) ? now->val : 1,
            fproc(now->sym->name));
    for (v = now->lft, i = 0; v; v = v->rgt, i++) {
      cat2(", ", v->lft);
    }
    check_param_count(i, now);

    if (i > Npars) { /* printf("\t%d parameters used, max %d expected\n", i,
                        Npars); */
      loger::fatal("too many parameters in run %s(...)", now->sym->name);
    }
    for (; i < Npars; i++)
      fprintf(fd, ", 0");
    fprintf(fd, ")");
    check_mtypes(now, now->lft);
    if (now->val < 0 || now->val > 255) /* 0 itself is allowed */
    {
      loger::fatal("bad process in run %s, valid range: 1..255",
                   now->sym->name);
    }
    break;

  case ENABLED:
    cat3("enabled(II, ", now->lft, ")");
    break;

  case GET_P:
    if (launch_settings.need_revert_old_rultes_for_priority) {
      fprintf(fd, "1");
    } else {
      cat3("get_priority(", now->lft, ")");
    }
    break;

  case SET_P:
    if (!launch_settings.need_revert_old_rultes_for_priority) {
      fprintf(fd, "if (TstOnly) return 1; /* T30 */\n\t\t");
      fprintf(fd, "set_priority(");
      putstmnt(fd, now->lft->lft, m);
      fprintf(fd, ", ");
      putstmnt(fd, now->lft->rgt, m);
      fprintf(fd, ")");
    }
    break;

  case NONPROGRESS:
    /* o_pm&4=progress, tau&128=claim stutter */
    if (launch_settings.separate_version == 2)
      fprintf(fd, "(!(o_pm&4) && !(tau&128))");
    else
      fprintf(fd, "(!(trpt->o_pm&4) && !(trpt->tau&128))");
    break;

  case PC_VAL:
    cat3("((P0 *) Pptr(", now->lft, "+BASE))->_p");
    break;

  case LEN:
    if (!terse && !TestOnly && has_xu) {
      fprintf(fd, "\n#ifndef XUSAFE\n\t\t");
      putname(fd, "(!(q_claim[", now->lft, m, "]&1) || ");
      putname(fd, "q_R_check(", now->lft, m, "");
      fprintf(fd, ", II)) &&\n\t\t");
      putname(fd, "(!(q_claim[", now->lft, m, "]&2) || ");
      putname(fd, "q_S_check(", now->lft, m, ", II)) &&");
      fprintf(fd, "\n#endif\n\t\t");
    }
    putname(fd, "q_len(", now->lft, m, ")");
    break;

  case FULL:
    if (!terse && !TestOnly && has_xu) {
      fprintf(fd, "\n#ifndef XUSAFE\n\t\t");
      putname(fd, "(!(q_claim[", now->lft, m, "]&1) || ");
      putname(fd, "q_R_check(", now->lft, m, "");
      fprintf(fd, ", II)) &&\n\t\t");
      putname(fd, "(!(q_claim[", now->lft, m, "]&2) || ");
      putname(fd, "q_S_check(", now->lft, m, ", II)) &&");
      fprintf(fd, "\n#endif\n\t\t");
    }
    putname(fd, "q_full(", now->lft, m, ")");
    break;

  case EMPTY:
    if (!terse && !TestOnly && has_xu) {
      fprintf(fd, "\n#ifndef XUSAFE\n\t\t");
      putname(fd, "(!(q_claim[", now->lft, m, "]&1) || ");
      putname(fd, "q_R_check(", now->lft, m, "");
      fprintf(fd, ", II)) &&\n\t\t");
      putname(fd, "(!(q_claim[", now->lft, m, "]&2) || ");
      putname(fd, "q_S_check(", now->lft, m, ", II)) &&");
      fprintf(fd, "\n#endif\n\t\t");
    }
    putname(fd, "(q_len(", now->lft, m, ")==0)");
    break;

  case NFULL:
    if (!terse && !TestOnly && has_xu) {
      fprintf(fd, "\n#ifndef XUSAFE\n\t\t");
      putname(fd, "(!(q_claim[", now->lft, m, "]&2) || ");
      putname(fd, "q_S_check(", now->lft, m, ", II)) &&");
      fprintf(fd, "\n#endif\n\t\t");
    }
    putname(fd, "(!q_full(", now->lft, m, "))");
    break;

  case NEMPTY:
    if (!terse && !TestOnly && has_xu) {
      fprintf(fd, "\n#ifndef XUSAFE\n\t\t");
      putname(fd, "(!(q_claim[", now->lft, m, "]&1) || ");
      putname(fd, "q_R_check(", now->lft, m, ", II)) &&");
      fprintf(fd, "\n#endif\n\t\t");
    }
    putname(fd, "(q_len(", now->lft, m, ")>0)");
    break;

  case 's':
    if (Pid_nr == eventmapnr) {
      fprintf(fd, "if ((II == -EVENT_TRACE && _tp != 's') ");
      putname(fd, "|| _qid+1 != ", now->lft, m, "");
      for (v = now->rgt, i = 0; v; v = v->rgt, i++) {
        if (v->lft->ntyp != CONST && v->lft->ntyp != EVAL)
          continue;
        fprintf(fd, " \\\n\t\t|| qrecv(");
        putname(fd, "", now->lft, m, ", ");
        putname(fd, "q_len(", now->lft, m, ")-1, ");
        fprintf(fd, "%d, 0) != ", i);
        if (v->lft->ntyp == CONST)
          putstmnt(fd, v->lft, m);
        else /* EVAL */
          putstmnt(fd, v->lft->lft, m);
      }
      fprintf(fd, ")\n");
      fprintf(fd, "\t\t	continue");
      putname(fd_th, " || (x_y3_ == ", now->lft, m, ")");
      break;
    }
    if (TestOnly) {
      if (launch_settings.need_lose_msgs_sent_to_full_queues)
        fprintf(fd, "1");
      else
        putname(fd, "!q_full(", now->lft, m, ")");
      break;
    }
    if (has_xu) {
      fprintf(fd, "\n#if !defined(XUSAFE) && !defined(NOREDUCE)\n\t\t");
      putname(fd, "if (q_claim[", now->lft, m, "]&2)\n\t\t");
      putname(fd, "{	q_S_check(", now->lft, m, ", II);\n\t\t");
      fprintf(fd, "}\n");
      if (has_sorted && now->val == 1) {
        putname(fd, "\t\tif (q_claim[", now->lft, m,
                "]&1)\n\t\t"); /* &1 iso &2 */
        fprintf(fd, "{	uerror(\"sorted send on xr channel violates po "
                    "reduction\");\n\t\t");
        fprintf(fd, "}\n");
      }
      fprintf(fd, "#endif\n\t\t");
    }
    fprintf(fd, "if (q_%s", (u_sync > 0 && u_async == 0) ? "len" : "full");
    putname(fd, "(", now->lft, m, "))\n");

    if (launch_settings.need_lose_msgs_sent_to_full_queues) {
      fprintf(fd, "\t\t{ nlost++; delta_m = 1; } else {");
    } else {
      fprintf(fd, "\t\t\t");
      Bailout(fd, ";");
    }

    if (has_enabled || lexer_.GetHasPriority())
      fprintf(fd, "\n\t\tif (TstOnly) return 1; /* T1 */");

    if (u_sync && !u_async && launch_settings.need_rendezvous_optimizations)
      fprintf(fd, "\n\n\t\tif (no_recvs(II)) continue;\n");

    fprintf(fd, "\n#ifdef HAS_CODE\n");
    fprintf(fd, "\t\tif (readtrail && gui) {\n");
    fprintf(fd, "\t\t\tchar simtmp[64];\n");
    putname(fd, "\t\t\tsprintf(simvals, \"%%d!\", ", now->lft, m, ");\n");
    _isok++;
    for (v = now->rgt, i = 0; v; v = v->rgt, i++) {
      cat3("\t\tsprintf(simtmp, \"%%d\", ", v->lft,
           "); strcat(simvals, simtmp);");
      if (v->rgt)
        fprintf(fd, "\t\tstrcat(simvals, \",\");\n");
    }
    _isok--;
    fprintf(fd, "\t\t}\n");
    fprintf(fd, "#endif\n\t\t");

    putname(fd, "\n\t\tqsend(", now->lft, m, "");
    fprintf(fd, ", %d", now->val);
    for (v = now->rgt, i = 0; v; v = v->rgt, i++) {
      cat2(", ", v->lft);
    }
    if (i > Mpars) {
      terse++;
      putname(stdout, "channel name: ", now->lft, m, "\n");
      terse--;
      printf("	%d msg parameters sent, %d expected\n", i, Mpars);
      loger::fatal("too many pars in send", "");
    }
    for (j = i; i < Mpars; i++) {
      fprintf(fd, ", 0");
    }
    fprintf(fd, ", %d)", j);
    if (u_sync) {
      fprintf(fd, ";\n\t\t");
      if (u_async) {
        putname(fd, "if (q_zero(", now->lft, m, ")) ");
      }
      putname(fd, "{ boq = ", now->lft, m, "");
      if (GenCode) {
        fprintf(fd, "; Uerror(\"rv-attempt in d_step\")");
      }
      fprintf(fd, "; }");
    }
    if (launch_settings.need_lose_msgs_sent_to_full_queues) {
      fprintf(fd, ";\n\t\t}\n\t\t"); /* end of m_loss else */
    }
    break;

  case 'r':
    if (Pid_nr == eventmapnr) {
      fprintf(fd, "if ((II == -EVENT_TRACE && _tp != 'r') ");
      putname(fd, "|| _qid+1 != ", now->lft, m, "");
      for (v = now->rgt, i = 0; v; v = v->rgt, i++) {
        if (v->lft->ntyp != CONST && v->lft->ntyp != EVAL)
          continue;
        fprintf(fd, " \\\n\t\t|| qrecv(");
        putname(fd, "", now->lft, m, ", ");
        fprintf(fd, "0, %d, 0) != ", i);
        if (v->lft->ntyp == CONST)
          putstmnt(fd, v->lft, m);
        else /* EVAL */
          putstmnt(fd, v->lft->lft, m);
      }
      fprintf(fd, ")\n");
      fprintf(fd, "\t\t	continue");

      putname(fd_tc, " || (x_y3_ == ", now->lft, m, ")");

      break;
    }
    if (TestOnly) {
      fprintf(fd, "((");
      if (u_sync)
        fprintf(fd, "(boq == -1 && ");

      putname(fd, "q_len(", now->lft, m, ")");

      if (u_sync && now->val <= 1) {
        putname(fd, ") || (boq == ", now->lft, m, " && ");
        putname(fd, "q_zero(", now->lft, m, "))");
      }

      fprintf(fd, ")");
      if (now->val == 0 || now->val == 2) {
        for (v = now->rgt, i = j = 0; v; v = v->rgt, i++) {
          if (v->lft->ntyp == CONST) {
            cat3("\n\t\t&& (", v->lft, " == ");
            putname(fd, "qrecv(", now->lft, m, ", ");
            fprintf(fd, "0, %d, 0))", i);
          } else if (v->lft->ntyp == EVAL) {
            cat3("\n\t\t&& (", v->lft->lft, " == ");
            putname(fd, "qrecv(", now->lft, m, ", ");
            fprintf(fd, "0, %d, 0))", i);
          } else {
            j++;
            continue;
          }
        }
      } else {
        fprintf(fd, "\n\t\t&& Q_has(");
        putname(fd, "", now->lft, m, "");
        for (v = now->rgt, i = 0; v; v = v->rgt, i++) {
          if (v->lft->ntyp == CONST) {
            fprintf(fd, ", 1, ");
            putstmnt(fd, v->lft, m);
          } else if (v->lft->ntyp == EVAL) {
            if (v->lft->lft->ntyp == ',') /* usertype1 */
            {
              if (0) {
                dump_tree("1", v->lft->lft);
              }
              Lextok *fix = v->lft->lft;
              do {
                i++;
                fprintf(fd, ", 1, ");
                putstmnt(fd, fix->lft, m);
                fix = fix->rgt;
              } while (fix && fix->ntyp == ',');
            } else {
              fprintf(fd, ", 1, ");
              putstmnt(fd, v->lft->lft, m);
            }
          } else {
            fprintf(fd, ", 0, 0");
          }
        }
        for (; i < Mpars; i++) {
          fprintf(fd, ", 0, 0");
        }
        fprintf(fd, ")");
      }
      fprintf(fd, ")");
      break;
    }
    if (has_xu) {
      fprintf(fd, "\n#if !defined(XUSAFE) && !defined(NOREDUCE)\n\t\t");
      putname(fd, "if (q_claim[", now->lft, m, "]&1)\n\t\t");
      putname(fd, "{	q_R_check(", now->lft, m, ", II);\n\t\t");
      if (has_random && now->val != 0)
        fprintf(fd, "	uerror(\"rand receive on xr channel violates po "
                    "reduction\");\n\t\t");
      fprintf(fd, "}\n");
      fprintf(fd, "#endif\n\t\t");
    }
    if (u_sync) {
      if (now->val >= 2) {
        if (u_async) {
          fprintf(fd, "if (");
          putname(fd, "q_zero(", now->lft, m, "))");
          fprintf(fd, "\n\t\t{\t");
        }
        fprintf(fd, "uerror(\"polling ");
        fprintf(fd, "rv chan\");\n\t\t");
        if (u_async)
          fprintf(fd, "	continue;\n\t\t}\n\t\t");
        fprintf(fd, "IfNotBlocked\n\t\t");
      } else {
        fprintf(fd, "if (");
        if (u_async == 0)
          putname(fd, "boq != ", now->lft, m, ") ");
        else {
          putname(fd, "q_zero(", now->lft, m, "))");
          fprintf(fd, "\n\t\t{\tif (boq != ");
          putname(fd, "", now->lft, m, ") ");
          Bailout(fd, ";\n\t\t} else\n\t\t");
          fprintf(fd, "{\tif (boq != -1) ");
        }
        Bailout(fd, ";\n\t\t");
        if (u_async)
          fprintf(fd, "}\n\t\t");
      }
    }
    putname(fd, "if (q_len(", now->lft, m, ") == 0) ");
    Bailout(fd, "");

    for (v = now->rgt, j = 0; v; v = v->rgt) {
      if (v->lft->ntyp != CONST && v->lft->ntyp != EVAL) {
        j++; /* count settables */
      }
    }

    fprintf(fd, ";\n\n\t\tXX=1");
    /* test */ if (now->val == 0 || now->val == 2) {
      for (v = now->rgt, i = 0; v; v = v->rgt, i++) {
        if (v->lft->ntyp == CONST) {
          fprintf(fd, ";\n\t\t");
          cat3("if (", v->lft, " != ");
          putname(fd, "qrecv(", now->lft, m, ", ");
          fprintf(fd, "0, %d, 0)) ", i);
          Bailout(fd, "");
        } else if (v->lft->ntyp == EVAL) {
          fprintf(fd, ";\n\t\t");
          cat3("if (", v->lft->lft, " != ");
          putname(fd, "qrecv(", now->lft, m, ", ");
          fprintf(fd, "0, %d, 0)) ", i);
          Bailout(fd, "");
        }
      }
      if (has_enabled || lexer_.GetHasPriority())
        fprintf(fd, ";\n\t\tif (TstOnly) return 1 /* T2 */");
    } else /* random receive: val 1 or 3 */
    {
      fprintf(fd, ";\n\t\tif (!(XX = Q_has(");
      putname(fd, "", now->lft, m, "");
      for (v = now->rgt, i = 0; v; v = v->rgt, i++) {
        if (v->lft->ntyp == CONST) {
          fprintf(fd, ", 1, ");
          putstmnt(fd, v->lft, m);
        } else if (v->lft->ntyp == EVAL) {
          if (v->lft->lft->ntyp == ',') /* usertype2 */
          {
            if (0) {
              dump_tree("2", v->lft->lft);
            }
            Lextok *fix = v->lft->lft;
            do {
              i++;
              fprintf(fd, ", 1, ");
              putstmnt(fd, fix->lft, m);
              fix = fix->rgt;
            } while (fix && fix->ntyp == ',');
          } else {
            fprintf(fd, ", 1, ");
            putstmnt(fd, v->lft->lft, m);
          }
        } else {
          fprintf(fd, ", 0, 0");
        }
      }
      for (; i < Mpars; i++) {
        fprintf(fd, ", 0, 0");
      }
      fprintf(fd, "))) ");
      Bailout(fd, "");

      if (has_enabled || lexer_.GetHasPriority()) {
        fprintf(fd, ";\n\t\tif (TstOnly) return 1 /* T2 */");
      }
      if (!GenCode) {
        fprintf(fd, ";\n\t\t");
        if (multi_oval) {
          check_needed();
          fprintf(fd, "(trpt+1)->bup.ovals[%d] = ", multi_oval - 1);
          multi_oval++;
        } else {
          fprintf(fd, "(trpt+1)->bup.oval = ");
        }
        fprintf(fd, "XX");
      }
    }

    if (j == 0 && now->val >= 2) {
      fprintf(fd, ";\n\t\t");
      break; /* poll without side-effect */
    }

    if (!GenCode) {
      int jj = 0;
      fprintf(fd, ";\n\t\t");
      /* no variables modified */
      if (j == 0 && now->val == 0) {
        fprintf(fd, "\n#ifndef BFS_PAR\n\t\t");
        /* q_flds values are not shared among cores */
        fprintf(fd, "if (q_flds[((Q0 *)qptr(");
        putname(fd, "", now->lft, m, "-1))->_t]");
        fprintf(fd, " != %d)\n\t\t\t", i);
        fprintf(fd, "Uerror(\"wrong nr of msg fields in rcv\");\n");
        fprintf(fd, "#endif\n\t\t");
      }

      for (v = now->rgt; v; v = v->rgt) {
        if ((v->lft->ntyp != CONST && v->lft->ntyp != EVAL)) {
          jj++; /* nr of vars needing bup */
        }
      }

      if (jj)
        for (v = now->rgt, i = 0; v; v = v->rgt, i++) {
          char tempbuf[64];

          if ((v->lft->ntyp == CONST || v->lft->ntyp == EVAL))
            continue;

          if (multi_oval) {
            check_needed();
            sprintf(tempbuf, "(trpt+1)->bup.ovals[%d] = ", multi_oval - 1);
            multi_oval++;
          } else
            sprintf(tempbuf, "(trpt+1)->bup.oval = ");

          if (v->lft->sym && v->lft->sym->name == "_") {
            fprintf(fd, tempbuf);
            putname(fd, "qrecv(", now->lft, m, "");
            fprintf(fd, ", XX-1, %d, 0);\n\t\t", i);
          } else {
            _isok++;
            cat30(tempbuf, v->lft, ";\n\t\t");
            _isok--;
          }
        }

      if (jj) /* check for double entries q?x,x */
      {
        Lextok *w;

        for (v = now->rgt; v; v = v->rgt) {
          if (v->lft->ntyp != CONST && v->lft->ntyp != EVAL && v->lft->sym &&
              v->lft->sym->type != STRUCT /* not a struct */
              && (v->lft->sym->value_type == 1 &&
                  v->lft->sym->is_array == 0) /* not array */
              && v->lft->sym->name != "_")
            for (w = v->rgt; w; w = w->rgt)
              if (v->lft->sym == w->lft->sym) {
                loger::fatal("cannot use var ('%s') in multiple msg fields",
                             v->lft->sym->name);
              }
        }
      }
    }
    /* set */ for (v = now->rgt, i = 0; v; v = v->rgt, i++) {
      if (v->lft->ntyp == CONST && v->rgt) {
        continue;
      }

      if (v->lft->ntyp == EVAL) {
        Lextok *fix = v->lft->lft;
        int old_i = i;
        while (fix && fix->ntyp == ',') /* usertype9 */
        {
          i++;
          fix = fix->rgt;
        }
        if (i > old_i) {
          i--; /* next increment handles it */
        }
        if (v->rgt) {
          continue;
        }
      }
      fprintf(fd, ";\n\t\t");

      if (v->lft->ntyp != CONST && v->lft->ntyp != EVAL &&
          v->lft->sym != NULL && v->lft->sym->name != "_") {
        nocast = 1;
        _isok++;
        putstmnt(fd, v->lft, m);
        _isok--;
        nocast = 0;
        fprintf(fd, " = ");
      }

      putname(fd, "qrecv(", now->lft, m, ", ");
      fprintf(fd, "XX-1, %d, ", i);
      fprintf(fd, "%d)", (v->rgt || now->val >= 2) ? 0 : 1);

      if (v->lft->ntyp != CONST && v->lft->ntyp != EVAL &&
          v->lft->sym != NULL && v->lft->sym->name != "_" &&
          (v->lft->ntyp != NAME ||
           v->lft->sym->type != models::SymbolType::kChan)) {
        fprintf(fd, ";\n#ifdef VAR_RANGES");
        fprintf(fd, "\n\t\tlogval(\"");
        withprocname = terse = nocast = 1;
        _isok++;
        putstmnt(fd, v->lft, m);
        withprocname = terse = nocast = 0;
        fprintf(fd, "\", ");
        putstmnt(fd, v->lft, m);
        _isok--;
        fprintf(fd, ");\n#endif\n");
        fprintf(fd, "\t\t");
      }
    }
    fprintf(fd, ";\n\t\t");

    fprintf(fd, "\n#ifdef HAS_CODE\n");
    fprintf(fd, "\t\tif (readtrail && gui) {\n");
    fprintf(fd, "\t\t\tchar simtmp[32];\n");
    putname(fd, "\t\t\tsprintf(simvals, \"%%d?\", ", now->lft, m, ");\n");
    _isok++;
    for (v = now->rgt, i = 0; v; v = v->rgt, i++) {
      if (v->lft->ntyp != EVAL) {
        cat3("\t\t\tsprintf(simtmp, \"%%d\", ", v->lft,
             "); strcat(simvals, simtmp);");
      } else {
        if (v->lft->lft->ntyp == ',') /* usertype4 */
        {
          if (0) {
            dump_tree("4", v->lft->lft);
          }
          Lextok *fix = v->lft->lft;
          do {
            i++;
            cat3("\n\t\t\tsprintf(simtmp, \"%%d,\", ", fix->lft,
                 "); strcat(simvals, simtmp);");
            fix = fix->rgt;
          } while (fix && fix->ntyp == ',');
        } else {
          cat3("\n\t\t\tsprintf(simtmp, \"%%d\", ", v->lft->lft,
               "); strcat(simvals, simtmp);");
        }
      }
      if (v->rgt) {
        fprintf(fd, "\n\t\t\tstrcat(simvals, \",\");\n");
      }
    }
    _isok--;
    fprintf(fd, "\n\t\t}\n");
    fprintf(fd, "#endif\n\t\t");

    if (u_sync) {
      putname(fd, "if (q_zero(", now->lft, m, "))");
      fprintf(fd, "\n\t\t{	boq = -1;\n");

      fprintf(fd, "#ifndef NOFAIR\n"); /* NEW 3.0.8 */
      fprintf(fd, "\t\t\tif (fairness\n");
      fprintf(fd, "\t\t\t&& !(trpt->o_pm&32)\n");
      fprintf(fd, "\t\t\t&& (now._a_t&2)\n");
      fprintf(fd, "\t\t\t&&  now._cnt[now._a_t&1] == II+2)\n");
      fprintf(fd, "\t\t\t{	now._cnt[now._a_t&1] -= 1;\n");
      fprintf(fd, "#ifdef VERI\n");
      fprintf(fd, "\t\t\t	if (II == 1)\n");
      fprintf(fd, "\t\t\t		now._cnt[now._a_t&1] = 1;\n");
      fprintf(fd, "#endif\n");
      fprintf(fd, "#ifdef DEBUG\n");
      fprintf(fd, "\t\t\tprintf(\"%%3ld: proc %%d fairness \", depth, II);\n");
      fprintf(fd, "\t\t\tprintf(\"Rule 2: --cnt to %%d (%%d)\\n\",\n");
      fprintf(fd, "\t\t\t	now._cnt[now._a_t&1], now._a_t);\n");
      fprintf(fd, "#endif\n");
      fprintf(fd, "\t\t\t	trpt->o_pm |= (32|64);\n");
      fprintf(fd, "\t\t\t}\n");
      fprintf(fd, "#endif\n");

      fprintf(fd, "\n\t\t}");
    }
    break;

  case 'R':
    if (!terse && !TestOnly && has_xu) {
      fprintf(fd, "\n#ifndef XUSAFE\n\t\t");
      putname(fd, "(!(q_claim[", now->lft, m, "]&1) || ");
      fprintf(fd, "q_R_check(");
      putname(fd, "", now->lft, m, ", II)) &&\n\t\t");
      putname(fd, "(!(q_claim[", now->lft, m, "]&2) || ");
      putname(fd, "q_S_check(", now->lft, m, ", II)) &&");
      fprintf(fd, "\n#endif\n\t\t");
    }
    if (u_sync > 0)
      putname(fd, "not_RV(", now->lft, m, ") && \\\n\t\t");

    for (v = now->rgt, i = j = 0; v; v = v->rgt, i++)
      if (v->lft->ntyp != CONST && v->lft->ntyp != EVAL) {
        j++;
        continue;
      }
    if (now->val == 0 || i == j) {
      putname(fd, "(q_len(", now->lft, m, ") > 0");
      for (v = now->rgt, i = 0; v; v = v->rgt, i++) {
        if (v->lft->ntyp != CONST && v->lft->ntyp != EVAL)
          continue;
        fprintf(fd, " \\\n\t\t&& qrecv(");
        putname(fd, "", now->lft, m, ", ");
        fprintf(fd, "0, %d, 0) == ", i);
        if (v->lft->ntyp == CONST) {
          putstmnt(fd, v->lft, m);
        } else /* EVAL */
        {
          if (v->lft->lft->ntyp == ',') /* usertype2 */
          {
            if (0) {
              dump_tree("8", v->lft->lft);
            }
            Lextok *fix = v->lft->lft;
            do {
              i++;
              putstmnt(fd, fix->lft, m);
              fix = fix->rgt;
            } while (fix && fix->ntyp == ',');
          } else {
            putstmnt(fd, v->lft->lft, m);
          }
        }
      }
      fprintf(fd, ")");
    } else {
      putname(fd, "Q_has(", now->lft, m, "");
      for (v = now->rgt, i = 0; v; v = v->rgt, i++) {
        if (v->lft->ntyp == CONST) {
          fprintf(fd, ", 1, ");
          putstmnt(fd, v->lft, m);
        } else if (v->lft->ntyp == EVAL) {
          if (v->lft->lft->ntyp == ',') /* usertype3 */
          {
            if (0) {
              dump_tree("3", v->lft->lft);
            }
            Lextok *fix = v->lft->lft;
            do {
              i++;
              fprintf(fd, ", 1, ");
              putstmnt(fd, fix->lft, m);
              fix = fix->rgt;
            } while (fix && fix->ntyp == ',');
          } else {
            fprintf(fd, ", 1, ");
            putstmnt(fd, v->lft->lft, m);
          }
        } else
          fprintf(fd, ", 0, 0");
      }
      for (; i < Mpars; i++) {
        fprintf(fd, ", 0, 0");
      }
      fprintf(fd, ")");
    }
    break;

  case 'c':
    preruse(fd, now->lft); /* preconditions */
    cat3("if (!(", now->lft, "))\n\t\t\t");
    Bailout(fd, "");
    break;

  case ELSE:
    if (!GenCode) {
      if (launch_settings.separate_version == 2)
        fprintf(fd, "if (o_pm&1)\n\t\t\t");
      else
        fprintf(fd, "if (trpt->o_pm&1)\n\t\t\t");
      Bailout(fd, "");
    } else {
      fprintf(fd, "/* else */");
    }
    break;

  case '?':
    if (now->lft) {
      cat3("( (", now->lft, ") ? ");
    }
    if (now->rgt) {
      cat3("(", now->rgt->lft, ") : ");
      cat3("(", now->rgt->rgt, ") )");
    }
    break;

  case ASGN:
    if (check_track(now) == STRUCT) {
      break;
    }

    if (has_enabled || lexer_.GetHasPriority())
      fprintf(fd, "if (TstOnly) return 1; /* T3 */\n\t\t");
    _isok++;

    if (!GenCode) {
      if (multi_oval) {
        char tempbuf[64];
        check_needed();
        sprintf(tempbuf, "(trpt+1)->bup.ovals[%d] = ", multi_oval - 1);
        multi_oval++;
        cat30(tempbuf, now->lft, ";\n\t\t");
      } else {
        cat3("(trpt+1)->bup.oval = ", now->lft, ";\n\t\t");
      }
    }
    if (now->lft->sym && now->lft->sym->type == models::SymbolType::kPredef &&
        now->lft->sym->name != "_" && now->lft->sym->name != "_priority") {
      loger::fatal("invalid assignment to %s", now->lft->sym->name);
    }

    nocast = 1;
    putstmnt(fd, now->lft, m);
    nocast = 0;
    fprintf(fd, " = ");
    _isok--;
    if (now->lft->sym->is_array &&
        now->rgt->ntyp == ',') /* array initializer */
    {
      putstmnt(fd, now->rgt->lft, m);
      loger::non_fatal("cannot use an array list initializer here");
    } else {
      putstmnt(fd, now->rgt, m);
    }
    if (now->sym->type != CHAN || verbose_flags.Active()) {
      fprintf(fd, ";\n#ifdef VAR_RANGES");
      fprintf(fd, "\n\t\tlogval(\"");
      withprocname = terse = nocast = 1;
      _isok++;
      putstmnt(fd, now->lft, m);
      withprocname = terse = nocast = 0;
      fprintf(fd, "\", ");
      putstmnt(fd, now->lft, m);
      _isok--;
      fprintf(fd, ");\n#endif\n");
      fprintf(fd, "\t\t");
    }
    break;

  case PRINT:
    if (has_enabled || lexer_.GetHasPriority())
      fprintf(fd, "if (TstOnly) return 1; /* T4 */\n\t\t");
#ifdef PRINTF
    fprintf(fd, "printf(%s", now->sym->name.c_str());
#else
    fprintf(fd, "Printf(%s", now->sym->name.c_str());
#endif
    for (v = now->lft; v; v = v->rgt) {
      cat2(", ", v->lft);
    }
    fprintf(fd, ")");
    break;

  case PRINTM: {
    std::string s;
    if (now->lft->sym && !now->lft->sym->mtype_name) {
      s = now->lft->sym->mtype_name->name;
    }

    if (has_enabled || lexer_.GetHasPriority()) {
      fprintf(fd, "if (TstOnly) return 1; /* T5 */\n\t\t");
    }
    fprintf(fd, "/* YY */ printm(");
    if (now->lft && now->lft->ismtyp) {
      fprintf(fd, "%d", now->lft->val);
    } else {
      putstmnt(fd, now->lft, m);
    }
    if (!s.empty()) {
      fprintf(fd, ", \"%s\"", s.c_str());
    } else {
      fprintf(fd, ", 0");
    }
    fprintf(fd, ")");
  } break;

  case NAME:
    if (!nocast && now->sym && Sym_typ(now) < SHORT)
      putname(fd, "((int)", now, m, ")");
    else
      putname(fd, "", now, m, "");
    break;

  case 'p':
    putremote(fd, now, m);
    break;

  case 'q':
    if (terse)
      fprintf(fd, "%s", now->sym ? now->sym->name.c_str() : "?");
    else
      fprintf(fd, "%d", remotelab(now));
    break;

  case C_EXPR:
    fprintf(fd, "(");
    plunk_expr(fd, now->sym->name);
#if 1
    fprintf(fd, ")");
#else
    fprintf(fd, ") /* %s */ ", now->sym->name);
#endif
    break;

  case C_CODE:
    if (now->sym)
      fprintf(fd, "/* %s */\n\t\t", now->sym->name.c_str());
    if (has_enabled || lexer_.GetHasPriority())
      fprintf(fd, "if (TstOnly) return 1; /* T6 */\n\t\t");

    if (now->sym)
      plunk_inline(fd, now->sym->name, 1, GenCode);
    else
      loger::fatal("internal error pangen2.c");

    if (!GenCode) {
      fprintf(fd, "\n"); /* state changed, capture it */
      fprintf(fd, "#if defined(C_States) && (HAS_TRACK==1)\n");
      fprintf(fd, "\t\tc_update((uchar *) &(now.c_state[0]));\n");
      fprintf(fd, "#endif\n");
    }
    break;

  case ASSERT:
    if (has_enabled || lexer_.GetHasPriority())
      fprintf(fd, "if (TstOnly) return 1; /* T7 */\n\t\t");

    cat3("spin_assert(", now->lft, ", ");
    terse = nocast = 1;
    cat3("\"", now->lft, "\", II, tt, t)");
    terse = nocast = 0;
    break;

  case '.':
  case BREAK:
  case GOTO:
    if (Pid_nr == eventmapnr)
      fprintf(fd, "Uerror(\"cannot get here\")");
    putskip(m);
    break;

  case '@':
    if (Pid_nr == eventmapnr) {
      fprintf(fd, "return 0");
      break;
    }

    if (has_enabled || lexer_.GetHasPriority()) {
      fprintf(fd, "if (TstOnly)\n\t\t\t");
      fprintf(fd, "return (II+1 == now._nr_pr);\n\t\t");
    }
    fprintf(fd, "if (!delproc(1, II)) ");
    Bailout(fd, "");
    break;

  default:
    printf("spin: error, %s:%d, bad node type %d (.m)\n", now->fn->name.c_str(),
           now->ln, now->ntyp);
    fflush(fd);
    alldone(1);
  }
}

std::string simplify_name(const std::string &s) {
  std::string t = s;

  if (!launch_settings.need_old_scope_rules) {
    size_t i = 0;
    while (i < t.length() && (t[i] == '_' || std::isdigit(t[i]))) {
      i++;
    }
    t = t.substr(i);
  }

  return t;
}

void putname(FILE *fd, const std::string &pre, Lextok *n, int m,
             const std::string &suff) /* varref */
{
  models::Symbol *s = n->sym;
  std::string ptr;

  lineno = n->ln;
  Fname = n->fn;

  if (!s)
    loger::fatal("no name - putname");

  if (s->context && context && s->type)
    s = findloc(s); /* it's a local var */

  if (!s) {
    fprintf(fd, "%s%s%s", pre.c_str(), n->sym->name.c_str(), suff.c_str());
    return;
  }

  if (!s->type)          /* not a local name */
    s = lookup(s->name); /* must be a global */

  if (!s->type) {
    if (strcmp(pre.c_str(), ".") != 0)
      loger::fatal("undeclared variable '%s'", s->name);
    s->type = models::kInt;
  }

  if (s->type == PROCTYPE)
    loger::fatal("proctype-name '%s' used as array-name", s->name);

  fprintf(fd, pre.c_str(), 0);
  if (!terse && !s->owner_name && evalindex != 1) {
    if (launch_settings.need_revert_old_rultes_for_priority && s->name == "_priority") {
      fprintf(fd, "1");
      goto shortcut;
    } else {
      if (s->context || s->name == "_p" || s->name == "_pid" ||
          s->name == "_priority") {
        fprintf(fd, "((P%d *)_this)->", Pid_nr);
      } else {
        bool x = s->name == "_";
        if (!(s->hidden_flags & 1) && x)
          fprintf(fd, "now.");
        if (!x && _isok == 0)
          loger::fatal("attempt to read value of '_'");
      }
    }
  }

  if (terse && launch_settings.buzzed == 1) {
    fprintf(fd, "B_state.%s", (s->context) ? "local[B_pid]." : "");
  }

  ptr = s->name;

  if (!dont_simplify        /* new 6.4.3 */
      && s->type != PREDEF) /* new 6.0.2 */
  {
    if (withprocname && s->context && strcmp(pre.c_str(), ".")) {
      fprintf(fd, "%s:", s->context->name.c_str());
      ptr = simplify_name(ptr);
    } else {
      if (terse) {
        ptr = simplify_name(ptr);
      }
    }
  }

  if (evalindex != 1)
    fprintf(fd, "%s", ptr.c_str());

  if (s->value_type > 1 || s->is_array == 1) {
    if (no_arrays) {
      loger::non_fatal("ref to array element invalid in this context");
      printf("\thint: instead of, e.g., x[rs] qu[3], use\n");
      printf("\tchan nm_3 = qu[3]; x[rs] nm_3;\n");
      printf("\tand use nm_3 in sends/recvs instead of qu[3]\n");
    }
    /* an xr or xs reference to an array element
     * becomes an exclusion tag on the array itself -
     * which could result in invalidly labeling
     * operations on other elements of this array to
     * be also safe under the partial order reduction
     * (see procedure has_global())
     */

    if (evalindex == 2) {
      fprintf(fd, "[%%d]");
    } else if (evalindex == 1) {
      evalindex = 0; /* no good if index is indexed array */
      fprintf(fd, ", ");
      putstmnt(fd, n->lft, m);
      evalindex = 1;
    } else {
      if (terse ||
          (n->lft && n->lft->ntyp == CONST && n->lft->val < s->value_type) ||
          (!n->lft && s->value_type > 0)) {
        cat3("[", n->lft, "]");
      } else { /* attempt to catch arrays that are indexed with an array element
                * in the same array this causes trouble in the verifier in the
                * backtracking e.g., restoring a[?] in the assignment: a [a[1]]
                * = x where a[1] == 1 but it is hard when the array is inside a
                * structure, so the names don't match
                */
        cat3("[ Index(", n->lft, ", ");
        fprintf(fd, "%d) ]", s->value_type);
      }
    }
  } else {
    if (n->lft /* effectively a scalar, but with an index */
        && (n->lft->ntyp != CONST || n->lft->val != 0)) {
      loger::fatal("ref to scalar '%s' using array index", ptr.c_str());
    }
  }

  if (s->type == STRUCT && n->rgt && n->rgt->lft) {
    putname(fd, ".", n->rgt->lft, m, "");
  }
shortcut:
  fprintf(fd, suff.c_str(), 0);
}

void putremote(FILE *fd, Lextok *n, int m) /* remote reference */
{
  int promoted = 0;
  int pt;

  if (terse) {
    fprintf(fd, "%s", n->lft->sym->name.c_str()); /* proctype name */
    if (n->lft->lft) {
      fprintf(fd, "[");
      putstmnt(fd, n->lft->lft, m); /* pid */
      fprintf(fd, "]");
    }
    if (lexer_.IsLtlMode()) {
      fprintf(fd, ":%s", n->sym->name.c_str());
    } else {
      fprintf(fd, ".%s", n->sym->name.c_str());
    }
  } else {
    if (Sym_typ(n) < SHORT) {
      promoted = 1;
      fprintf(fd, "((int)");
    }

    pt = fproc(n->lft->sym->name);
    fprintf(fd, "((P%d *)Pptr(", pt);
    if (n->lft->lft) {
      fprintf(fd, "BASE+");
      putstmnt(fd, n->lft->lft, m);
    } else
      fprintf(fd, "f_pid(%d)", pt);
    fprintf(fd, "))->%s", n->sym->name.c_str());
  }
  if (n->rgt) {
    fprintf(fd, "[");
    putstmnt(fd, n->rgt, m); /* array var ref */
    fprintf(fd, "]");
  }
  if (promoted)
    fprintf(fd, ")");
}

static int
getweight(Lextok *n) { /* this piece of code is a remnant of early versions
                        * of the verifier -- in the current version of Spin
                        * only non-zero values matter - so this could probably
                        * simply return 1 in all cases.
                        */
  switch (n->ntyp) {
  case 'r':
    return 4;
  case 's':
    return 2;
  case TIMEOUT:
    return 1;
  case 'c':
    if (has_typ(n->lft, TIMEOUT))
      return 1;
  }
  return 3;
}

int has_typ(Lextok *n, int m) {
  if (!n)
    return 0;
  if (n->ntyp == m)
    return 1;
  return (has_typ(n->lft, m) || has_typ(n->rgt, m));
}

static int runcount, opcount;

static void do_count(Lextok *n, int checkop) {
  if (!n)
    return;

  switch (n->ntyp) {
  case RUN:
    runcount++;
    break;
  default:
    if (checkop)
      opcount++;
    break;
  }
  do_count(n->lft, checkop && (n->ntyp != RUN));
  do_count(n->rgt, checkop);
}

void count_runs(Lextok *n) {
  runcount = opcount = 0;
  do_count(n, 1);
  if (runcount > 1)
    loger::fatal("more than one run operator in expression", "");
  if (runcount == 1 && opcount > 1)
    loger::fatal("use of run operator in compound expression", "");
}

void any_runs(Lextok *n) {
  runcount = opcount = 0;
  do_count(n, 0);
  if (runcount >= 1)
    loger::fatal("run operator used in invalid context", "");
}
