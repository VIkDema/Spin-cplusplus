#include "pangen2.hpp"
#include "../codegen/codegen.hpp"
#include "../fatal/fatal.hpp"
#include "../lexer/inline_processor.hpp"
#include "../lexer/lexer.hpp"
#include "../lexer/line_number.hpp"
#include "../main/launch_settings.hpp"
#include "../main/main_processor.hpp"
#include "../spin.hpp"
#include "../utils/verbose/verbose.hpp"
#include "../version/version.hpp"
#include "pangen4.hpp"
#include "pangen5.hpp"
#include "pangen7.hpp"
#include "y.tab.h"
#include <fmt/core.h>
#include <iostream>

extern LaunchSettings launch_settings;

#define DELTA 500 /* sets an upperbound on nr of chan names */

#define blurb(fd, e)                                                           \
  {                                                                            \
    fprintf(fd, "\n");                                                         \
    if (!launch_settings.need_statemate_merging)                               \
      fprintf(fd, "\t\t/* %s:%d */\n", e->n->file_name->name.c_str(),          \
              e->n->line_number);                                              \
  }
#define tr_map(m, e)                                                           \
  {                                                                            \
    if (!launch_settings.need_statemate_merging)                               \
      fprintf(fd_tt, "\t\ttr_2_src(%d, \"%s\", %d);\n", m,                     \
              e->n->file_name->name.c_str(), e->n->line_number);               \
  }

extern models::ProcList *ready;
extern models::RunList *run_lst;
extern models::Lextok *runstmnts;
extern models::Symbol *Fname, *oFname;
extern char *claimproc, *eventmap;
extern int Npars, Mpars, nclaims;
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
short has_badelse = 0; /* spec contains else combined with chan refs */
short has_enabled = 0; /* spec contains enabled() */
short has_pcvalue = 0; /* spec contains pc_value() */
short has_np = 0;      /* spec contains np_ */
short has_sorted = 0;  /* spec contains `!!' (sorted-send) operator */
short has_random = 0;  /* spec contains `??' (random-recv) operator */
short has_xu = 0;      /* spec contains xr or xs assertions */
short has_unless = 0;  /* spec contains unless statements */
extern lexer::Lexer lexer_;
int mstp = 0;        /* max nr of state/process */
int claimnr = -1;    /* claim process, if any */
int eventmapnr = -1; /* event trace, if any */
int Pid_nr;          /* proc currently processed */
int multi_oval;      /* set in merges, used also in pangen4.c */
int in_settr;        /* avoid quotes inside quotes */

#define MAXMERGE 256 /* max nr of bups per merge sequence */

static short CnT[MAXMERGE];
static models::Lextok XZ, YZ[MAXMERGE];
static int didcase, YZmax, YZcnt;

static models::Lextok *Nn[2];
static int Det; /* set if deterministic */
static int T_sum, T_mus, t_cyc;
static int TPE[2], EPT[2];
static int uniq = 1;
static int multi_needed, multi_undo;
static short AllGlobal = 0;    /* set if process has provided clause */
static short withprocname = 0; /* prefix local varnames with procname */
static short _isok = 0;        /* checks usage of predefined variable _ */
static short evalindex = 0;    /* evaluate index of var names */

extern int has_global(models::Lextok *);
extern void check_mtypes(models::Lextok *, models::Lextok *);
extern void walk2_struct(const std::string &, models::Symbol *);
extern int find_min(models::Sequence *);
extern int find_max(models::Sequence *);

static int getweight(models::Lextok *);
static int scan_seq(models::Sequence *);
static void genconditionals(void);
static void mark_seq(models::Sequence *);
static void patch_atomic(models::Sequence *);
static void put_seq(models::Sequence *, int, int);
static void putproc(models::ProcList *);
static void Tpe(models::Lextok *);
extern void spit_recvs(FILE *, FILE *);

static models::L_List *keep_track;

void keep_track_off(models::Lextok *n) {
  models::L_List *p;

  p = (models::L_List *)emalloc(sizeof(models::L_List));
  p->n = n;
  p->next = keep_track;
  keep_track = p;
}

int check_track(models::Lextok *n) {
  models::L_List *p;

  for (p = keep_track; p; p = p->next) {
    if (p->n == n) {
      return n->symbol ? n->symbol->type : 0;
    }
  }
  return 0;
}

static int fproc(const std::string &s) {
  models::ProcList *p;

  for (p = ready; p; p = p->next)
    if (p->n->name.c_str() == s)
      return p->tn;

  loger::fatal("proctype %s not found", s);
  return -1;
}

int pid_is_claim(int p) /* Pid_nr (p->tn) to type (p->b) */
{
  models::ProcList *r;

  for (r = ready; r; r = r->next) {
    if (r->tn == p)
      return (r->b == models::btypes::N_CLAIM);
  }
  printf("spin: error, cannot find pid %d\n", p);
  return 0;
}

static void reverse_procs(models::RunList *q) {
  if (!q)
    return;
  reverse_procs(q->next);
  fprintf(fd_tc, "		Addproc(%d, %d);\n", q->tn,
          q->priority < 1 ? 1 : q->priority);
}

static void forward_procs(models::RunList *q) {
  if (!q)
    return;
  fprintf(fd_tc, "		Addproc(%d, %d);\n", q->tn,
          q->priority < 1 ? 1 : q->priority);
  forward_procs(q->next);
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
  fprintf(fd_tt, "\t    T->next	  = ");
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
  models::ProcList *p;
  int i;

  disambiguate(); /* avoid name-clashes between scopes */

  if (!(fd_tc = fopen(Cfile[0].nm[launch_settings.separate_version],
                      MFLAGS)) /* main routines */
      || !(fd_th = fopen(Cfile[1].nm[launch_settings.separate_version],
                         MFLAGS)) /* header file   */
      || !(fd_tt = fopen(Cfile[2].nm[launch_settings.separate_version],
                         MFLAGS)) /* transition matrix */
      || !(fd_tm = fopen(Cfile[3].nm[launch_settings.separate_version],
                         MFLAGS)) /* forward  moves */
      || !(fd_tb = fopen(Cfile[4].nm[launch_settings.separate_version],
                         MFLAGS))) /* backward moves */
  {
    printf("spin: cannot create pan.[chtmfb]\n");
    MainProcessor::Exit(1);
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
    models::Sequence *s = (models::Sequence *)emalloc(sizeof(models::Sequence));
    s->minel = -1;
    claimproc = "_:never_template:_";
    n->name = "_:never_template:_";
    mk_rdy(n, ZN, s, 0, ZN, models::btypes::N_CLAIM);
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
    if (lexer_.GetHasPriority() &&
        !launch_settings.need_revert_old_rultes_for_priority)
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
  if (lexer_.GetHasPriority() &&
      !launch_settings.need_revert_old_rultes_for_priority)
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
  if (has_enabled || (lexer_.GetHasPriority() &&
                      !launch_settings.need_revert_old_rultes_for_priority))
    fprintf(fd_th, "#define HAS_ENABLED	1\n");
  if (has_unless)
    fprintf(fd_th, "#define HAS_UNLESS	%d\n", has_unless);
  if (launch_settings.has_provided)
    fprintf(fd_th, "#define HAS_PROVIDED	%d\n",
            launch_settings.has_provided);
  if (has_pcvalue)
    fprintf(fd_th, "#define HAS_PCVALUE	%d\n", has_pcvalue);
  if (has_badelse)
    fprintf(fd_th, "#define HAS_BADELSE	%d\n", has_badelse);
  if (has_enabled ||
      (lexer_.GetHasPriority() &&
       !launch_settings.need_revert_old_rultes_for_priority) ||
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

  fprintf(fd_th, "\ntypedef struct S_F_MAP {\n");
  fprintf(fd_th, "	char *fnm;\n\tint from;\n\tint upto;\n");
  fprintf(fd_th, "} S_F_MAP;\n");

  fprintf(fd_tc, "/*** Generated by %s ***/\n", SpinVersion);
  fprintf(fd_tc, "/*** From source: %s ***/\n\n", oFname->name.c_str());

  ntimes(fd_tc, 0, 1, Pre0);

  codegen::HandleCDescls(fd_tc); /* types can be refered to in State */

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

  codegen::HandleCFCTS(fd_tc); /* State can be used in fcts */

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

  codegen::CPreview(); /* sets hastrack */

  for (p = ready; p; p = p->next)
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

  for (p = ready; p; p = p->next) {
    putproc(p);
  }

  if (launch_settings.separate_version != 2) {
    fprintf(fd_th, "\n");
    for (p = ready; p; p = p->next)
      fprintf(fd_th, "extern short src_ln%d[];\n", p->tn);
    for (p = ready; p; p = p->next)
      fprintf(fd_th, "extern S_F_MAP src_file%d[];\n", p->tn);
    fprintf(fd_th, "\n");

    fprintf(fd_tc, "uchar reached%d[3];  /* np_ */\n", nrRdy);
    fprintf(fd_tc, "uchar *loopstate%d;  /* np_ */\n", nrRdy);

    fprintf(fd_tc, "struct {\n");
    fprintf(fd_tc, "	int tp; short *src;\n");
    fprintf(fd_tc, "} src_all[] = {\n");
    for (p = ready; p; p = p->next)
      fprintf(fd_tc, "	{ %d, &src_ln%d[0] },\n", p->tn, p->tn);
    fprintf(fd_tc, "	{ 0, (short *) 0 }\n");
    fprintf(fd_tc, "};\n");

    fprintf(fd_tc, "S_F_MAP *flref[] = {\n"); /* 5.3.0 */
    for (p = ready; p; p = p->next) {
      fprintf(fd_tc, "	src_file%d%c\n", p->tn, p->next ? ',' : ' ');
    }
    fprintf(fd_tc, "};\n\n");
  } else {
    fprintf(fd_tc, "extern uchar reached%d[3];  /* np_ */\n", nrRdy);
  }
  codegen::GenCodeTable(fd_tc); /* was th */

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
  models::ProcList *p;

  if (s)
    for (p = ready; p; p = p->next)
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
  extern models::Ordered *all_names;
  models::Ordered *walk;

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

static void putproc(models::ProcList *p) {
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

  if (p->b == models::btypes::N_CLAIM || p->b == models::btypes::E_TRACE ||
      p->b == models::btypes::N_TRACE) {
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

static void cnt_seq(models::Sequence *s) {
  models::Element *f;
  models::SeqList *h;

  if (s)
    for (f = s->frst; f; f = f->next) {
      Tpe(f->n); /* sets EPT */
      addTpe(EPT[0]);
      addTpe(EPT[1]);
      for (h = f->sub; h; h = h->next)
        cnt_seq(h->this_sequence);
      if (f == s->last)
        break;
    }
}

static void typ_seq(models::Sequence *s) {
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

static int hidden_flags(models::Lextok *n) {
  if (n)
    switch (n->node_type) {
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
      (void)hidden_flags(n->left);
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
      (void)hidden_flags(n->left);
      (void)hidden_flags(n->right);
      break;
    }
  return T_mus;
}

static int getNid(models::Lextok *n) {
  if (n->symbol && n->symbol->type == STRUCT && n->right && n->right->left)
    return getNid(n->right->left);

  if (!n->symbol || n->symbol->id == 0) {
    char *no_name = "no name";
    loger::fatal("bad channel name '%s'",
                 (n->symbol) ? n->symbol->name : no_name);
  }
  return n->symbol->id;
}

static int valTpe(models::Lextok *n) {
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
  switch (n->node_type) { /* a series of fall-thru cases: */
  case FULL:
    res += DELTA; /* add 3*DELTA + chan nr */
  case EMPTY:
    res += DELTA; /* add 2*DELTA + chan nr */
  case 'r':
  case NEMPTY:
    res += DELTA; /* add 1*DELTA + chan nr */
  case 's':
  case NFULL:
    res += getNid(n->left); /* add channel nr */
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

static void Tpe(models::Lextok *n) /* mixing in selections */
{
  EPT[0] = 2;
  EPT[1] = 0;

  if (!n)
    return;

  T_mus = 0;
  Nn[0] = Nn[1] = ZN;

  if (n->node_type == 'c') {
    if (hidden_flags(n->left) > 2) {
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

static void put_escp(models::Element *e) {
  int n;
  models::SeqList *x;

  if (e->esc /* && e->n->node_type != GOTO */ && e->n->node_type != '.') {
    for (x = e->esc, n = 0; x; x = x->next, n++) {
      int i = huntele(x->this_sequence->frst, e->status, -1)->seqno;
      fprintf(fd_tt, "\ttrans[%d][%d]->escp[%d] = %d;\n", Pid_nr, e->seqno, n,
              i);
      fprintf(fd_tt, "\treached%d[%d] = 1;\n", Pid_nr, i);
    }
    for (x = e->esc, n = 0; x; x = x->next, n++) {
      fprintf(fd_tt, "	/* escape #%d: %d */\n", n,
              huntele(x->this_sequence->frst, e->status, -1)->seqno);
      put_seq(x->this_sequence, 2, 0); /* args?? */
    }
    fprintf(fd_tt, "	/* end-escapes */\n");
  }
}

static void put_sub(models::Element *e, int Tt0, int Tt1) {
  models::Sequence *s = e->n->seq_list->this_sequence;
  models::Element *g = ZE;
  int a;

  patch_atomic(s);
  putskip(s->frst->seqno);
  g = huntstart(s->frst);
  a = g->seqno;

  if (0)
    printf("put_sub %d -> %d -> %d\n", e->seqno, s->frst->seqno, a);

  if ((e->n->node_type == ATOMIC || e->n->node_type == D_STEP) && scan_seq(s))
    mark_seq(s);
  s->last->next = e->next;

  typ_seq(s); /* sets TPE */

  if (e->n->node_type == D_STEP) {
    int inherit = (e->status & (ATOM | L_ATOM));
    fprintf(fd_tm, "\tcase %d: ", uniq++);
    fprintf(fd_tm, "// STATE %d - %s:%d - [", e->seqno,
            e->n->file_name->name.c_str(), e->n->line_number);
    comment(fd_tm, e->n, 0);
    fprintf(fd_tm, "]\n\t\t");

    if (s->last->n->node_type == BREAK)
      OkBreak = target(huntele(s->last->next, s->last->status, -1))->Seqno;
    else
      OkBreak = -1;

    if (!putcode(fd_tm, s, e->next, 0, e->n->line_number, e->seqno)) {
      fprintf(fd_tm, "\n#if defined(C_States) && (HAS_TRACK==1)\n");
      fprintf(fd_tm, "\t\tc_update((uchar *) &(now.c_state[0]));\n");
      fprintf(fd_tm, "#endif\n");

      fprintf(fd_tm, "\t\t_m = %d", getweight(s->frst->n));
      if (launch_settings.need_lose_msgs_sent_to_full_queues &&
          s->frst->n->node_type == 's')
        fprintf(fd_tm, "+delta_m; delta_m = 0");
      fprintf(fd_tm, "; goto P999;\n\n");
    }

    fprintf(fd_tb, "\tcase %d: ", uniq - 1);
    fprintf(fd_tb, "// STATE %d\n", e->seqno);
    fprintf(fd_tb, "\t\tsv_restor();\n");
    fprintf(fd_tb, "\t\tgoto R999;\n");
    if (e->next)
      a = huntele(e->next, e->status, -1)->seqno;
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
            (e->n->node_type == ATOMIC) ? ATOM : 0);
    in_settr++;
    comment(fd_tt, e->n, e->seqno);
    in_settr--;
    if ((e->status & CHECK2) || (g->status & CHECK2))
      s->frst->status |= I_GLOB;
    fprintf(fd_tt, "\", %d, %d, %d);", (s->frst->status & I_GLOB) ? 1 : 0, Tt0,
            Tt1);
    blurb(fd_tt, e);
    fprintf(fd_tt, "\tT->next\t= ");
    fprintf(fd_tt, "settr(%d,%d,%d,0,0,\"", e->Seqno,
            (e->n->node_type == ATOMIC) ? ATOM : 0, a);
    in_settr++;
    comment(fd_tt, e->n, e->seqno);
    in_settr--;
    fprintf(fd_tt, "\", %d, ", (s->frst->status & I_GLOB) ? 1 : 0);
    if (e->n->node_type == NON_ATOMIC) {
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
  models::Element *e;
  models::Lextok *n;
  models::FSM_use *u;
  struct CaseCache *next;
};

static CaseCache *casing[6];

static int identical(models::Lextok *p, models::Lextok *q) {
  if ((!p && q) || (p && !q))
    return 0;
  if (!p)
    return 1;

  if (p->node_type != q->node_type || p->is_mtype_token != q->is_mtype_token ||
      p->value != q->value || p->index_step != q->index_step ||
      p->symbol != q->symbol || p->sequence != q->sequence ||
      p->seq_list != q->seq_list)
    return 0;

  return identical(p->left, q->left) && identical(p->right, q->right);
}

static int samedeads(models::FSM_use *a, models::FSM_use *b) {
  models::FSM_use *p, *q;

  for (p = a, q = b; p && q; p = p->next, q = q->next)
    if (p->var != q->var || p->special != q->special)
      return 0;
  return (!p && !q);
}

static models::Element *findnext(models::Element *f) {
  models::Element *g;

  if (f->n->node_type == GOTO) {
    g = get_lab(f->n, 1);
    return huntele(g, f->status, -1);
  }
  return f->next;
}

static models::Element *advance(models::Element *e, int stopat) {
  models::Element *f = e;

  if (stopat)
    while (f && f->seqno != stopat) {
      f = findnext(f);
      if (!f) {
        break;
      }
      switch (f->n->node_type) {
      case GOTO:
      case '.':
      case PRINT:
      case PRINTM:
        break;
      default:
        return f;
      }
    }
  return (models::Element *)0;
}

static int equiv_merges(models::Element *a, models::Element *b) {
  models::Element *f, *g;
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

static CaseCache *prev_case(models::Element *e, int owner) {
  int j;
  CaseCache *nc;

  switch (e->n->node_type) {
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
  for (nc = casing[j]; nc; nc = nc->next)
    if (identical(nc->n, e->n) && samedeads(nc->u, e->dead) &&
        equiv_merges(nc->e, e) && nc->owner == owner)
      return nc;

  return (CaseCache *)0;
}

static void new_case(models::Element *e, int m, int b, int owner) {
  int j;
  CaseCache *nc;

  switch (e->n->node_type) {
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
  nc->next = casing[j];
  casing[j] = nc;
}

static int nr_bup(models::Element *e) {
  models::FSM_use *u;
  models::Lextok *v;
  int nr = 0;

  switch (e->n->node_type) {
  case ASGN:
    if (check_track(e->n) == STRUCT) {
      break;
    }
    nr++;
    break;
  case 'r':
    if (e->n->value >= 1)
      nr++; /* random recv */
    for (v = e->n->right; v; v = v->right) {
      if ((v->left->node_type == CONST || v->left->node_type == EVAL))
        continue;
      nr++;
    }
    break;
  default:
    break;
  }
  for (u = e->dead; u; u = u->next) {
    switch (u->special) {
    case 2: /* dead after write */
      if (e->n->node_type == ASGN && e->n->right->node_type == CONST &&
          e->n->right->value == 0)
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

static int nrhops(models::Element *e) {
  models::Element *f = e, *g;
  int cnt = 0;
  int stopat;

  if (e->merge_start)
    stopat = e->merge_start;
  else
    stopat = e->merge;
  do {
    cnt += nr_bup(f);

    if (f->n->node_type == GOTO) {
      g = get_lab(f->n, 1);
      if (g->seqno == stopat)
        f = g;
      else
        f = huntele(g, f->status, stopat);
    } else {
      f = f->next;
    }

    if (f && !f->merge && !f->merge_single && f->seqno != stopat) {
      fprintf(fd_tm, "\n\t\t// bad hop %s:%d -- at %d, <",
              f->n->file_name->name.c_str(), f->n->line_number, f->seqno);
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

static void doforward(FILE *tm_fd, models::Element *e) {
  models::FSM_use *u;

  putstmnt(tm_fd, e->n, e->seqno);

  if (e->n->node_type != ELSE && Det) {
    fprintf(tm_fd, ";\n\t\tif (trpt->o_pm&1)\n\t\t");
    fprintf(tm_fd, "\tuerror(\"non-determinism in D_proctype\")");
  }
  if (launch_settings.need_hide_write_only_variables && !lexer_.GetHasCode())
    for (u = e->dead; u; u = u->next) {
      fprintf(tm_fd, ";\n\t\t");
      fprintf(tm_fd, "if (TstOnly) return 1; /* TT */\n");
      fprintf(tm_fd, "\t\t/* dead %d: %s */  ", u->special,
              u->var->name.c_str());

      switch (u->special) {
      case 2: /* dead after write -- lval already bupped */
        if (e->n->node_type == ASGN) /* could be recv or asgn */
        {
          if (e->n->right->node_type == CONST && e->n->right->value == 0)
            continue; /* already set to 0 */
        }
        if (e->n->node_type != 'r') {
          XZ.symbol = u->var;
          fprintf(tm_fd, "\n#ifdef HAS_CODE\n");
          fprintf(tm_fd, "\t\tif (!readtrail)\n");
          fprintf(tm_fd, "#endif\n\t\t\t");
          putname(tm_fd, "", &XZ, 0, " = 0");
          break;
        }     /* else fall through */
      case 1: /* dead after read -- add asgn of rval -- needs bup */
        YZ[YZmax].symbol = u->var; /* store for pan.b */
        CnT[YZcnt]++;              /* this step added bups */
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

static int dobackward(models::Element *e, int casenr) {
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

  if (e->n->node_type != '.') {
    fprintf(fd_tb, ";\n\t\t");
    undostmnt(e->n, e->seqno);
  }
  _isok--;

  YZcnt--;
  return 1;
}

static void lastfirst(int stopat, models::Element *fin, int casenr) {
  models::Element *f = fin, *g;

  if (f->n->node_type == GOTO) {
    g = get_lab(f->n, 1);
    if (g->seqno == stopat)
      f = g;
    else
      f = huntele(g, f->status, stopat);
  } else
    f = f->next;

  if (!f || f->seqno == stopat || (!f->merge && !f->merge_single))
    return;
  lastfirst(stopat, f, casenr);
  dobackward(f, casenr);
}

static int modifier;

static void lab_transfer(models::Element *to, models::Element *from) {
  models::Symbol *ns, *s = has_lab(from, (1 | 2 | 4));
  models::Symbol *oc;
  int ltp, usedit = 0;

  if (!s)
    return;

  /* "from" could have all three labels -- rename
   * to prevent jumps to the transfered copies
   */
  oc = models::Symbol::GetContext();                    /* remember */
  for (ltp = 1; ltp < 8; ltp *= 2) /* 1, 2, and 4 */
    if ((s = has_lab(from, ltp)) != (models::Symbol *)0) {
      ns = (models::Symbol *)emalloc(sizeof(models::Symbol));
      ns->name = (char *)emalloc((int)strlen(s->name.c_str()) + 4);
      ns->name = fmt::format("{}{}", s->name, modifier);
      models::Symbol::SetContext(s->context);
      set_lab(ns, to);
      usedit++;
    }
  models::Symbol::SetContext(oc);/* restore */
  if (usedit) {
    if (modifier++ > 990)
      loger::fatal("modifier overflow error");
  }
}

static int case_cache(models::Element *e, int a) {
  int bupcase = 0, casenr = uniq, fromcache = 0;
  CaseCache *Cached = (CaseCache *)0;
  models::Element *f, *g;
  int j, nrbups, mark, ntarget;

  mark = (e->status & ATOM); /* could lose atomicity in a merge chain */

  if (e->merge_mark > 0 ||
      (launch_settings.need_statemate_merging &&
       e->merge_in ==
           0)) { /* state nominally unreachable (part of merge chains) */
    if (e->n->node_type != '.' && e->n->node_type != GOTO) {
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

    fprintf(fd_tm, "// STATE %d - %s:%d - [", e->seqno,
            e->n->file_name->name.c_str(), e->n->line_number);
    comment(fd_tm, e->n, 0);
    fprintf(fd_tm, "] (%d:%d - %d) same as %d (%d:%d - %d)\n", e->merge_start,
            e->merge, e->merge_in, casenr, Cached->e->merge_start,
            Cached->e->merge, Cached->e->merge_in);

    goto gotit;
  }

  fprintf(fd_tm, "\tcase %d: // STATE %d - %s:%d - [", uniq++, e->seqno,
          e->n->file_name->name.c_str(), e->n->line_number);
  comment(fd_tm, e->n, 0);
  nrbups = (e->merge || e->merge_start) ? nrhops(e) : nr_bup(e);
  fprintf(fd_tm, "] (%d:%d:%d - %d)\n\t\t", e->merge_start, e->merge, nrbups,
          e->merge_in);

  if (nrbups > MAXMERGE - 1)
    loger::fatal("merge requires more than 256 bups");

  if (e->n->node_type != 'r' && !pid_is_claim(Pid_nr) && Pid_nr != eventmapnr)
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
    if (f->n->node_type == GOTO) {
      g = get_lab(f->n, 1);
      if (g->seqno == ntarget)
        f = g;
      else
        f = huntele(g, f->status, ntarget);
    } else
      f = f->next;

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
  if (launch_settings.need_lose_msgs_sent_to_full_queues &&
      e->n->node_type == 's')
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

static void put_el(models::Element *e, int Tt0, int Tt1) {
  int a, casenr, Global_ref;
  models::Element *g = ZE;

  if (e->n->node_type == GOTO) {
    g = get_lab(e->n, 1);
    g = huntele(g, e->status, -1);
    cross_dsteps(e->n, g->n);
    a = g->seqno;
  } else if (e->next) {
    g = huntele(e->next, e->status, -1);
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
  switch (e->n->node_type) {
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
    if (e->n->left->node_type == CONST &&
        e->n->left->value == 1) /* skip or true */
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

static void nested_unless(models::Element *e, models::Element *g) {
  struct models::SeqList *y = e->esc, *z = g->esc;

  for (; y && z; y = y->next, z = z->next)
    if (z->this_sequence != y->this_sequence)
      break;
  if (!y && !z)
    return;

  if (g->n->node_type != GOTO && g->n->node_type != '.' && e->sub->next) {
    printf("error: (%s:%d) saw 'unless' on a guard:\n",
           (e->n) ? e->n->file_name->name.c_str() : "-",
           (e->n) ? e->n->line_number : 0);
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

static void put_seq(models::Sequence *s, int Tt0, int Tt1) {
  models::SeqList *h;
  models::Element *e, *g;
  int a, deadlink;
  auto &verbose_flags = utils::verbose::Flags::getInstance();
  if (0)
    printf("put_seq %d\n", s->frst->seqno);

  for (e = s->frst; e; e = e->next) {
    if (0)
      printf("	step %d\n", e->seqno);
    if (e->status & DONE) {
      if (0)
        printf("		done before\n");
      goto checklast;
    }
    e->status |= DONE;

    if (e->n->line_number)
      putsrc(e);

    if (e->n->node_type == UNLESS) {
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
      for (h = e->sub; h; h = h->next) {
        putskip(h->this_sequence->frst->seqno);
        g = huntstart(h->this_sequence->frst);
        if (g->esc)
          nested_unless(e, g);
        a = g->seqno;

        if (g->n->node_type == 'c' && g->n->left->node_type == CONST &&
            g->n->left->value == 0 /* 0 or false */
            && !g->esc) {
          fprintf(fd_tt, "#if 0\n\t/* dead link: */\n");
          deadlink = 1;
          if (verbose_flags.NeedToPrintVerbose())
            printf("spin: %s:%d, warning, condition is always false\n",
                   g->n->file_name ? g->n->file_name->name.c_str() : "",
                   g->n->line_number);
        } else
          deadlink = 0;
        if (0)
          printf("			settr %d %d\n", a, 0);
        if (h->next)
          fprintf(fd_tt, "\tT = T->next\t= ");
        else
          fprintf(fd_tt, "\t    T->next\t= ");
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
      for (h = e->sub; h; h = h->next)
        put_seq(h->this_sequence, Tt0, Tt1);
    } else {
      if (0)
        printf("		[non]atomic %d\n", e->n->node_type);
      if (e->n->node_type == ATOMIC || e->n->node_type == D_STEP ||
          e->n->node_type == NON_ATOMIC)
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

static void
patch_atomic(models::Sequence *s) /* catch goto's that break the chain */
{
  models::Element *f, *g;
  models::SeqList *h;

  for (f = s->frst; f; f = f->next) {
    if (f->n && f->n->node_type == GOTO) {
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
      for (h = f->sub; h; h = h->next)
        patch_atomic(h->this_sequence);
    if (f == s->extent)
      break;
  }
}

static void mark_seq(models::Sequence *s) {
  models::Element *f;
  models::SeqList *h;

  for (f = s->frst; f; f = f->next) {
    f->status |= I_GLOB;

    if (f->n->node_type == ATOMIC || f->n->node_type == NON_ATOMIC ||
        f->n->node_type == D_STEP)
      mark_seq(f->n->seq_list->this_sequence);

    for (h = f->sub; h; h = h->next)
      mark_seq(h->this_sequence);
    if (f == s->last)
      return;
  }
}

static models::Element *find_target(models::Element *e) {
  models::Element *f;

  if (!e)
    return e;

  if (t_cyc++ > 32) {
    loger::fatal("cycle of goto jumps");
  }
  switch (e->n->node_type) {
  case GOTO:
    f = get_lab(e->n, 1);
    cross_dsteps(e->n, f->n);
    f = find_target(f);
    break;
  case BREAK:
    if (e->next) {
      f = find_target(huntele(e->next, e->status, -1));
      break; /* new 5.0 -- was missing */
    }
    /* else fall through */
  default:
    f = e;
    break;
  }
  return f;
}

models::Element *target(models::Element *e) {
  if (!e)
    return e;
  file::LineNumber::Set(e->n->line_number);
  Fname = e->n->file_name;
  t_cyc = 0;
  return find_target(e);
}

static int seq_has_el(models::Sequence *s,
                      models::Element *g) /* new to version 5.0 */
{
  models::Element *f;
  models::SeqList *h;

  for (f = s->frst; f; f = f->next) /* g in same atomic? */
  {
    if (f == g) {
      return 1;
    }
    if (f->status & CHECK3) {
      continue;
    }
    f->status |= CHECK3; /* protect against cycles */
    for (h = f->sub; h; h = h->next) {
      if (h->this_sequence && seq_has_el(h->this_sequence, g)) {
        return 1;
      }
    }
  }
  return 0;
}

static int scan_seq(models::Sequence *s) {
  models::Element *f, *g;
  models::SeqList *h;

  for (f = s->frst; f; f = f->next) {
    if ((f->status & CHECK2) || has_global(f->n))
      return 1;
    if (f->n->node_type == GOTO   /* may exit or reach other atomic */
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
        fprintf(fd_tt, "\t/* mark-down line %d status %d = %d */\n",
                f->n->line_number, f->status, (f->status & D_ATOM));
        return 1; /* assume worst case */
      }
    }
    for (h = f->sub; h; h = h->next)
      if (scan_seq(h->this_sequence))
        return 1;
    if (f == s->last)
      break;
  }
  return 0;
}

static int glob_args(models::Lextok *n) {
  int result = 0;
  models::Lextok *v;

  for (v = n->right; v; v = v->right) {
    if (v->left->node_type == CONST)
      continue;
    if (v->left->node_type == EVAL)
      result += has_global(v->left->left);
    else
      result += has_global(v->left);
  }
  return result;
}

static int proc_is_safe(const models::Lextok *n) {
  models::ProcList *p;
  /* not safe unless no local var inits are used */
  /* note that a local variable init could refer to a global */

  for (p = ready; p; p = p->next) {
    if (n->symbol->name == p->n->name) { /* printf("proc %s safety: %d\n",
                                         p->n->name, p->unsafe); */
      return (p->unsafe != 0);
    }
  }
  /* cannot happen */
  return 0;
}

int has_global(models::Lextok *n) {
  models::Lextok *v;
  static models::Symbol *n_seen = (models::Symbol *)0;

  if (!n)
    return 0;
  if (AllGlobal)
    return 1; /* global provided clause */

  switch (n->node_type) {
  case ATOMIC:
  case D_STEP:
  case NON_ATOMIC:
    return scan_seq(n->seq_list->this_sequence);

  case '.':
  case BREAK:
  case GOTO:
  case CONST:
    return 0;

  case ELSE:
    return n->value; /* true if combined with chan refs */

  case 's':
    return glob_args(n) != 0 || ((n->symbol->xu & (XS | XX)) != XS);
  case 'r':
    return glob_args(n) != 0 || ((n->symbol->xu & (XR | XX)) != XR);
  case 'R':
    return glob_args(n) != 0 ||
           (((n->symbol->xu) & (XR | XS | XX)) != (XR | XS));
  case NEMPTY:
    return ((n->symbol->xu & (XR | XX)) != XR);
  case NFULL:
    return ((n->symbol->xu & (XS | XX)) != XS);
  case FULL:
    return ((n->symbol->xu & (XR | XX)) != XR);
  case EMPTY:
    return ((n->symbol->xu & (XS | XX)) != XS);
  case LEN:
    return (((n->symbol->xu) & (XR | XS | XX)) != (XR | XS));

  case NAME:
    if (n->symbol->name == "_priority") {
      if (launch_settings.need_revert_old_rultes_for_priority) {
        if (n_seen != n->symbol)
          loger::fatal("cannot refer to _priority with -o6");
        n_seen = n->symbol;
      }
      return 0;
    }
    if (n->symbol->context || (n->symbol->hidden_flags & 64) ||
        n->symbol->name == "_pid" || n->symbol->name == "_")
      return 0;
    return 1;

  case RUN:
    return proc_is_safe(n);

  case C_CODE:
  case C_EXPR:
    return lexer::InlineProcessor::CheckGlobInline(n->symbol->name);

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
    return has_global(n->left);

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
    return has_global(n->left) || has_global(n->right);

  case PRINT:
    for (v = n->left; v; v = v->right)
      if (has_global(v->left))
        return 1;
    return 0;
  case PRINTM:
    return has_global(n->left);
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
  putstmnt(fd, now->left, m);                                                  \
  fprintf(fd, x);                                                              \
  putstmnt(fd, now->right, m)
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

void dump_tree(const char *s, models::Lextok *p) {
  char z[64];

  if (!p)
    return;

  printf("\n%s:\t%2d:\t%3d (", s, p->line_number, p->node_type);
  std::cout << loger::explainToString(p->node_type);
  if (p->node_type == 315)
    printf(": %s", p->symbol->name.c_str());
  if (p->node_type == 312)
    printf(": %d", p->value);
  printf(")");

  if (p->left) {
    sprintf(z, "%sL", s);
    dump_tree(z, p->left);
  }
  if (p->right) {
    sprintf(z, "%sR", s);
    dump_tree(z, p->right);
  }
}

void putstmnt(FILE *fd, models::Lextok *now, int m) {
  models::Lextok *v;
  int i, j;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  if (!now) {
    fprintf(fd, "0");
    return;
  }
  file::LineNumber::Set(now->line_number);
  Fname = now->file_name;

  switch (now->node_type) {
  case CONST:
    fprintf(fd, "%d", now->value);
    break;
  case '!':
    cat3(" !(", now->left, ")");
    break;
  case UMIN:
    cat3(" -(", now->left, ")");
    break;
  case '~':
    cat3(" ~(", now->left, ")");
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
             Fname->name.c_str(), file::LineNumber::Get());
    /* is okay as a guard */
    break;

  case RUN:
    if (now->symbol == NULL)
      loger::fatal("internal error pangen2.c");
    if (claimproc && strcmp(now->symbol->name.c_str(), claimproc) == 0)
      loger::fatal("claim %s, (not runnable)", claimproc);
    if (eventmap && strcmp(now->symbol->name.c_str(), eventmap) == 0)
      loger::fatal("eventmap %s, (not runnable)", eventmap);

    if (GenCode)
      loger::fatal("'run' in d_step sequence (use atomic)");

    fprintf(
        fd, "addproc(II, %d, %d",
        (now->value > 0 && !launch_settings.need_revert_old_rultes_for_priority)
            ? now->value
            : 1,
        fproc(now->symbol->name));
    for (v = now->left, i = 0; v; v = v->right, i++) {
      cat2(", ", v->left);
    }
    check_param_count(i, now);

    if (i > Npars) { /* printf("\t%d parameters used, max %d expected\n", i,
                        Npars); */
      loger::fatal("too many parameters in run %s(...)", now->symbol->name);
    }
    for (; i < Npars; i++)
      fprintf(fd, ", 0");
    fprintf(fd, ")");
    check_mtypes(now, now->left);
    if (now->value < 0 || now->value > 255) /* 0 itself is allowed */
    {
      loger::fatal("bad process in run %s, valid range: 1..255",
                   now->symbol->name);
    }
    break;

  case ENABLED:
    cat3("enabled(II, ", now->left, ")");
    break;

  case GET_P:
    if (launch_settings.need_revert_old_rultes_for_priority) {
      fprintf(fd, "1");
    } else {
      cat3("get_priority(", now->left, ")");
    }
    break;

  case SET_P:
    if (!launch_settings.need_revert_old_rultes_for_priority) {
      fprintf(fd, "if (TstOnly) return 1; /* T30 */\n\t\t");
      fprintf(fd, "set_priority(");
      putstmnt(fd, now->left->left, m);
      fprintf(fd, ", ");
      putstmnt(fd, now->left->right, m);
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
    cat3("((P0 *) Pptr(", now->left, "+BASE))->_p");
    break;

  case LEN:
    if (!terse && !TestOnly && has_xu) {
      fprintf(fd, "\n#ifndef XUSAFE\n\t\t");
      putname(fd, "(!(q_claim[", now->left, m, "]&1) || ");
      putname(fd, "q_R_check(", now->left, m, "");
      fprintf(fd, ", II)) &&\n\t\t");
      putname(fd, "(!(q_claim[", now->left, m, "]&2) || ");
      putname(fd, "q_S_check(", now->left, m, ", II)) &&");
      fprintf(fd, "\n#endif\n\t\t");
    }
    putname(fd, "q_len(", now->left, m, ")");
    break;

  case FULL:
    if (!terse && !TestOnly && has_xu) {
      fprintf(fd, "\n#ifndef XUSAFE\n\t\t");
      putname(fd, "(!(q_claim[", now->left, m, "]&1) || ");
      putname(fd, "q_R_check(", now->left, m, "");
      fprintf(fd, ", II)) &&\n\t\t");
      putname(fd, "(!(q_claim[", now->left, m, "]&2) || ");
      putname(fd, "q_S_check(", now->left, m, ", II)) &&");
      fprintf(fd, "\n#endif\n\t\t");
    }
    putname(fd, "q_full(", now->left, m, ")");
    break;

  case EMPTY:
    if (!terse && !TestOnly && has_xu) {
      fprintf(fd, "\n#ifndef XUSAFE\n\t\t");
      putname(fd, "(!(q_claim[", now->left, m, "]&1) || ");
      putname(fd, "q_R_check(", now->left, m, "");
      fprintf(fd, ", II)) &&\n\t\t");
      putname(fd, "(!(q_claim[", now->left, m, "]&2) || ");
      putname(fd, "q_S_check(", now->left, m, ", II)) &&");
      fprintf(fd, "\n#endif\n\t\t");
    }
    putname(fd, "(q_len(", now->left, m, ")==0)");
    break;

  case NFULL:
    if (!terse && !TestOnly && has_xu) {
      fprintf(fd, "\n#ifndef XUSAFE\n\t\t");
      putname(fd, "(!(q_claim[", now->left, m, "]&2) || ");
      putname(fd, "q_S_check(", now->left, m, ", II)) &&");
      fprintf(fd, "\n#endif\n\t\t");
    }
    putname(fd, "(!q_full(", now->left, m, "))");
    break;

  case NEMPTY:
    if (!terse && !TestOnly && has_xu) {
      fprintf(fd, "\n#ifndef XUSAFE\n\t\t");
      putname(fd, "(!(q_claim[", now->left, m, "]&1) || ");
      putname(fd, "q_R_check(", now->left, m, ", II)) &&");
      fprintf(fd, "\n#endif\n\t\t");
    }
    putname(fd, "(q_len(", now->left, m, ")>0)");
    break;

  case 's':
    if (Pid_nr == eventmapnr) {
      fprintf(fd, "if ((II == -EVENT_TRACE && _tp != 's') ");
      putname(fd, "|| _qid+1 != ", now->left, m, "");
      for (v = now->right, i = 0; v; v = v->right, i++) {
        if (v->left->node_type != CONST && v->left->node_type != EVAL)
          continue;
        fprintf(fd, " \\\n\t\t|| qrecv(");
        putname(fd, "", now->left, m, ", ");
        putname(fd, "q_len(", now->left, m, ")-1, ");
        fprintf(fd, "%d, 0) != ", i);
        if (v->left->node_type == CONST)
          putstmnt(fd, v->left, m);
        else /* EVAL */
          putstmnt(fd, v->left->left, m);
      }
      fprintf(fd, ")\n");
      fprintf(fd, "\t\t	continue");
      putname(fd_th, " || (x_y3_ == ", now->left, m, ")");
      break;
    }
    if (TestOnly) {
      if (launch_settings.need_lose_msgs_sent_to_full_queues)
        fprintf(fd, "1");
      else
        putname(fd, "!q_full(", now->left, m, ")");
      break;
    }
    if (has_xu) {
      fprintf(fd, "\n#if !defined(XUSAFE) && !defined(NOREDUCE)\n\t\t");
      putname(fd, "if (q_claim[", now->left, m, "]&2)\n\t\t");
      putname(fd, "{	q_S_check(", now->left, m, ", II);\n\t\t");
      fprintf(fd, "}\n");
      if (has_sorted && now->value == 1) {
        putname(fd, "\t\tif (q_claim[", now->left, m,
                "]&1)\n\t\t"); /* &1 iso &2 */
        fprintf(fd, "{	uerror(\"sorted send on xr channel violates po "
                    "reduction\");\n\t\t");
        fprintf(fd, "}\n");
      }
      fprintf(fd, "#endif\n\t\t");
    }
    fprintf(fd, "if (q_%s", (u_sync > 0 && u_async == 0) ? "len" : "full");
    putname(fd, "(", now->left, m, "))\n");

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
    putname(fd, "\t\t\tsprintf(simvals, \"%%d!\", ", now->left, m, ");\n");
    _isok++;
    for (v = now->right, i = 0; v; v = v->right, i++) {
      cat3("\t\tsprintf(simtmp, \"%%d\", ", v->left,
           "); strcat(simvals, simtmp);");
      if (v->right)
        fprintf(fd, "\t\tstrcat(simvals, \",\");\n");
    }
    _isok--;
    fprintf(fd, "\t\t}\n");
    fprintf(fd, "#endif\n\t\t");

    putname(fd, "\n\t\tqsend(", now->left, m, "");
    fprintf(fd, ", %d", now->value);
    for (v = now->right, i = 0; v; v = v->right, i++) {
      cat2(", ", v->left);
    }
    if (i > Mpars) {
      terse++;
      putname(stdout, "channel name: ", now->left, m, "\n");
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
        putname(fd, "if (q_zero(", now->left, m, ")) ");
      }
      putname(fd, "{ boq = ", now->left, m, "");
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
      putname(fd, "|| _qid+1 != ", now->left, m, "");
      for (v = now->right, i = 0; v; v = v->right, i++) {
        if (v->left->node_type != CONST && v->left->node_type != EVAL)
          continue;
        fprintf(fd, " \\\n\t\t|| qrecv(");
        putname(fd, "", now->left, m, ", ");
        fprintf(fd, "0, %d, 0) != ", i);
        if (v->left->node_type == CONST)
          putstmnt(fd, v->left, m);
        else /* EVAL */
          putstmnt(fd, v->left->left, m);
      }
      fprintf(fd, ")\n");
      fprintf(fd, "\t\t	continue");

      putname(fd_tc, " || (x_y3_ == ", now->left, m, ")");

      break;
    }
    if (TestOnly) {
      fprintf(fd, "((");
      if (u_sync)
        fprintf(fd, "(boq == -1 && ");

      putname(fd, "q_len(", now->left, m, ")");

      if (u_sync && now->value <= 1) {
        putname(fd, ") || (boq == ", now->left, m, " && ");
        putname(fd, "q_zero(", now->left, m, "))");
      }

      fprintf(fd, ")");
      if (now->value == 0 || now->value == 2) {
        for (v = now->right, i = j = 0; v; v = v->right, i++) {
          if (v->left->node_type == CONST) {
            cat3("\n\t\t&& (", v->left, " == ");
            putname(fd, "qrecv(", now->left, m, ", ");
            fprintf(fd, "0, %d, 0))", i);
          } else if (v->left->node_type == EVAL) {
            cat3("\n\t\t&& (", v->left->left, " == ");
            putname(fd, "qrecv(", now->left, m, ", ");
            fprintf(fd, "0, %d, 0))", i);
          } else {
            j++;
            continue;
          }
        }
      } else {
        fprintf(fd, "\n\t\t&& Q_has(");
        putname(fd, "", now->left, m, "");
        for (v = now->right, i = 0; v; v = v->right, i++) {
          if (v->left->node_type == CONST) {
            fprintf(fd, ", 1, ");
            putstmnt(fd, v->left, m);
          } else if (v->left->node_type == EVAL) {
            if (v->left->left->node_type == ',') /* usertype1 */
            {
              if (0) {
                dump_tree("1", v->left->left);
              }
              models::Lextok *fix = v->left->left;
              do {
                i++;
                fprintf(fd, ", 1, ");
                putstmnt(fd, fix->left, m);
                fix = fix->right;
              } while (fix && fix->node_type == ',');
            } else {
              fprintf(fd, ", 1, ");
              putstmnt(fd, v->left->left, m);
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
      putname(fd, "if (q_claim[", now->left, m, "]&1)\n\t\t");
      putname(fd, "{	q_R_check(", now->left, m, ", II);\n\t\t");
      if (has_random && now->value != 0)
        fprintf(fd, "	uerror(\"rand receive on xr channel violates po "
                    "reduction\");\n\t\t");
      fprintf(fd, "}\n");
      fprintf(fd, "#endif\n\t\t");
    }
    if (u_sync) {
      if (now->value >= 2) {
        if (u_async) {
          fprintf(fd, "if (");
          putname(fd, "q_zero(", now->left, m, "))");
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
          putname(fd, "boq != ", now->left, m, ") ");
        else {
          putname(fd, "q_zero(", now->left, m, "))");
          fprintf(fd, "\n\t\t{\tif (boq != ");
          putname(fd, "", now->left, m, ") ");
          Bailout(fd, ";\n\t\t} else\n\t\t");
          fprintf(fd, "{\tif (boq != -1) ");
        }
        Bailout(fd, ";\n\t\t");
        if (u_async)
          fprintf(fd, "}\n\t\t");
      }
    }
    putname(fd, "if (q_len(", now->left, m, ") == 0) ");
    Bailout(fd, "");

    for (v = now->right, j = 0; v; v = v->right) {
      if (v->left->node_type != CONST && v->left->node_type != EVAL) {
        j++; /* count settables */
      }
    }

    fprintf(fd, ";\n\n\t\tXX=1");
    /* test */ if (now->value == 0 || now->value == 2) {
      for (v = now->right, i = 0; v; v = v->right, i++) {
        if (v->left->node_type == CONST) {
          fprintf(fd, ";\n\t\t");
          cat3("if (", v->left, " != ");
          putname(fd, "qrecv(", now->left, m, ", ");
          fprintf(fd, "0, %d, 0)) ", i);
          Bailout(fd, "");
        } else if (v->left->node_type == EVAL) {
          fprintf(fd, ";\n\t\t");
          cat3("if (", v->left->left, " != ");
          putname(fd, "qrecv(", now->left, m, ", ");
          fprintf(fd, "0, %d, 0)) ", i);
          Bailout(fd, "");
        }
      }
      if (has_enabled || lexer_.GetHasPriority())
        fprintf(fd, ";\n\t\tif (TstOnly) return 1 /* T2 */");
    } else /* random receive: val 1 or 3 */
    {
      fprintf(fd, ";\n\t\tif (!(XX = Q_has(");
      putname(fd, "", now->left, m, "");
      for (v = now->right, i = 0; v; v = v->right, i++) {
        if (v->left->node_type == CONST) {
          fprintf(fd, ", 1, ");
          putstmnt(fd, v->left, m);
        } else if (v->left->node_type == EVAL) {
          if (v->left->left->node_type == ',') /* usertype2 */
          {
            if (0) {
              dump_tree("2", v->left->left);
            }
            models::Lextok *fix = v->left->left;
            do {
              i++;
              fprintf(fd, ", 1, ");
              putstmnt(fd, fix->left, m);
              fix = fix->right;
            } while (fix && fix->node_type == ',');
          } else {
            fprintf(fd, ", 1, ");
            putstmnt(fd, v->left->left, m);
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

    if (j == 0 && now->value >= 2) {
      fprintf(fd, ";\n\t\t");
      break; /* poll without side-effect */
    }

    if (!GenCode) {
      int jj = 0;
      fprintf(fd, ";\n\t\t");
      /* no variables modified */
      if (j == 0 && now->value == 0) {
        fprintf(fd, "\n#ifndef BFS_PAR\n\t\t");
        /* q_flds values are not shared among cores */
        fprintf(fd, "if (q_flds[((Q0 *)qptr(");
        putname(fd, "", now->left, m, "-1))->_t]");
        fprintf(fd, " != %d)\n\t\t\t", i);
        fprintf(fd, "Uerror(\"wrong nr of msg fields in rcv\");\n");
        fprintf(fd, "#endif\n\t\t");
      }

      for (v = now->right; v; v = v->right) {
        if ((v->left->node_type != CONST && v->left->node_type != EVAL)) {
          jj++; /* nr of vars needing bup */
        }
      }

      if (jj)
        for (v = now->right, i = 0; v; v = v->right, i++) {
          char tempbuf[64];

          if ((v->left->node_type == CONST || v->left->node_type == EVAL))
            continue;

          if (multi_oval) {
            check_needed();
            sprintf(tempbuf, "(trpt+1)->bup.ovals[%d] = ", multi_oval - 1);
            multi_oval++;
          } else
            sprintf(tempbuf, "(trpt+1)->bup.oval = ");

          if (v->left->symbol && v->left->symbol->name == "_") {
            fprintf(fd, tempbuf);
            putname(fd, "qrecv(", now->left, m, "");
            fprintf(fd, ", XX-1, %d, 0);\n\t\t", i);
          } else {
            _isok++;
            cat30(tempbuf, v->left, ";\n\t\t");
            _isok--;
          }
        }

      if (jj) /* check for double entries q?x,x */
      {
        models::Lextok *w;

        for (v = now->right; v; v = v->right) {
          if (v->left->node_type != CONST && v->left->node_type != EVAL &&
              v->left->symbol &&
              v->left->symbol->type != STRUCT /* not a struct */
              && (v->left->symbol->value_type == 1 &&
                  v->left->symbol->is_array == 0) /* not array */
              && v->left->symbol->name != "_")
            for (w = v->right; w; w = w->right)
              if (v->left->symbol == w->left->symbol) {
                loger::fatal("cannot use var ('%s') in multiple msg fields",
                             v->left->symbol->name);
              }
        }
      }
    }
    /* set */ for (v = now->right, i = 0; v; v = v->right, i++) {
      if (v->left->node_type == CONST && v->right) {
        continue;
      }

      if (v->left->node_type == EVAL) {
        models::Lextok *fix = v->left->left;
        int old_i = i;
        while (fix && fix->node_type == ',') /* usertype9 */
        {
          i++;
          fix = fix->right;
        }
        if (i > old_i) {
          i--; /* next increment handles it */
        }
        if (v->right) {
          continue;
        }
      }
      fprintf(fd, ";\n\t\t");

      if (v->left->node_type != CONST && v->left->node_type != EVAL &&
          v->left->symbol != NULL && v->left->symbol->name != "_") {
        nocast = 1;
        _isok++;
        putstmnt(fd, v->left, m);
        _isok--;
        nocast = 0;
        fprintf(fd, " = ");
      }

      putname(fd, "qrecv(", now->left, m, ", ");
      fprintf(fd, "XX-1, %d, ", i);
      fprintf(fd, "%d)", (v->right || now->value >= 2) ? 0 : 1);

      if (v->left->node_type != CONST && v->left->node_type != EVAL &&
          v->left->symbol != NULL && v->left->symbol->name != "_" &&
          (v->left->node_type != NAME ||
           v->left->symbol->type != models::SymbolType::kChan)) {
        fprintf(fd, ";\n#ifdef VAR_RANGES");
        fprintf(fd, "\n\t\tlogval(\"");
        withprocname = terse = nocast = 1;
        _isok++;
        putstmnt(fd, v->left, m);
        withprocname = terse = nocast = 0;
        fprintf(fd, "\", ");
        putstmnt(fd, v->left, m);
        _isok--;
        fprintf(fd, ");\n#endif\n");
        fprintf(fd, "\t\t");
      }
    }
    fprintf(fd, ";\n\t\t");

    fprintf(fd, "\n#ifdef HAS_CODE\n");
    fprintf(fd, "\t\tif (readtrail && gui) {\n");
    fprintf(fd, "\t\t\tchar simtmp[32];\n");
    putname(fd, "\t\t\tsprintf(simvals, \"%%d?\", ", now->left, m, ");\n");
    _isok++;
    for (v = now->right, i = 0; v; v = v->right, i++) {
      if (v->left->node_type != EVAL) {
        cat3("\t\t\tsprintf(simtmp, \"%%d\", ", v->left,
             "); strcat(simvals, simtmp);");
      } else {
        if (v->left->left->node_type == ',') /* usertype4 */
        {
          if (0) {
            dump_tree("4", v->left->left);
          }
          models::Lextok *fix = v->left->left;
          do {
            i++;
            cat3("\n\t\t\tsprintf(simtmp, \"%%d,\", ", fix->left,
                 "); strcat(simvals, simtmp);");
            fix = fix->right;
          } while (fix && fix->node_type == ',');
        } else {
          cat3("\n\t\t\tsprintf(simtmp, \"%%d\", ", v->left->left,
               "); strcat(simvals, simtmp);");
        }
      }
      if (v->right) {
        fprintf(fd, "\n\t\t\tstrcat(simvals, \",\");\n");
      }
    }
    _isok--;
    fprintf(fd, "\n\t\t}\n");
    fprintf(fd, "#endif\n\t\t");

    if (u_sync) {
      putname(fd, "if (q_zero(", now->left, m, "))");
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
      putname(fd, "(!(q_claim[", now->left, m, "]&1) || ");
      fprintf(fd, "q_R_check(");
      putname(fd, "", now->left, m, ", II)) &&\n\t\t");
      putname(fd, "(!(q_claim[", now->left, m, "]&2) || ");
      putname(fd, "q_S_check(", now->left, m, ", II)) &&");
      fprintf(fd, "\n#endif\n\t\t");
    }
    if (u_sync > 0)
      putname(fd, "not_RV(", now->left, m, ") && \\\n\t\t");

    for (v = now->right, i = j = 0; v; v = v->right, i++)
      if (v->left->node_type != CONST && v->left->node_type != EVAL) {
        j++;
        continue;
      }
    if (now->value == 0 || i == j) {
      putname(fd, "(q_len(", now->left, m, ") > 0");
      for (v = now->right, i = 0; v; v = v->right, i++) {
        if (v->left->node_type != CONST && v->left->node_type != EVAL)
          continue;
        fprintf(fd, " \\\n\t\t&& qrecv(");
        putname(fd, "", now->left, m, ", ");
        fprintf(fd, "0, %d, 0) == ", i);
        if (v->left->node_type == CONST) {
          putstmnt(fd, v->left, m);
        } else /* EVAL */
        {
          if (v->left->left->node_type == ',') /* usertype2 */
          {
            if (0) {
              dump_tree("8", v->left->left);
            }
            models::Lextok *fix = v->left->left;
            do {
              i++;
              putstmnt(fd, fix->left, m);
              fix = fix->right;
            } while (fix && fix->node_type == ',');
          } else {
            putstmnt(fd, v->left->left, m);
          }
        }
      }
      fprintf(fd, ")");
    } else {
      putname(fd, "Q_has(", now->left, m, "");
      for (v = now->right, i = 0; v; v = v->right, i++) {
        if (v->left->node_type == CONST) {
          fprintf(fd, ", 1, ");
          putstmnt(fd, v->left, m);
        } else if (v->left->node_type == EVAL) {
          if (v->left->left->node_type == ',') /* usertype3 */
          {
            if (0) {
              dump_tree("3", v->left->left);
            }
            models::Lextok *fix = v->left->left;
            do {
              i++;
              fprintf(fd, ", 1, ");
              putstmnt(fd, fix->left, m);
              fix = fix->right;
            } while (fix && fix->node_type == ',');
          } else {
            fprintf(fd, ", 1, ");
            putstmnt(fd, v->left->left, m);
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
    codegen::PreRuse(fd, now->left); /* preconditions */
    cat3("if (!(", now->left, "))\n\t\t\t");
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
    if (now->left) {
      cat3("( (", now->left, ") ? ");
    }
    if (now->right) {
      cat3("(", now->right->left, ") : ");
      cat3("(", now->right->right, ") )");
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
        cat30(tempbuf, now->left, ";\n\t\t");
      } else {
        cat3("(trpt+1)->bup.oval = ", now->left, ";\n\t\t");
      }
    }
    if (now->left->symbol &&
        now->left->symbol->type == models::SymbolType::kPredef &&
        now->left->symbol->name != "_" &&
        now->left->symbol->name != "_priority") {
      loger::fatal("invalid assignment to %s", now->left->symbol->name);
    }

    nocast = 1;
    putstmnt(fd, now->left, m);
    nocast = 0;
    fprintf(fd, " = ");
    _isok--;
    if (now->left->symbol->is_array &&
        now->right->node_type == ',') /* array initializer */
    {
      putstmnt(fd, now->right->left, m);
      loger::non_fatal("cannot use an array list initializer here");
    } else {
      putstmnt(fd, now->right, m);
    }
    if (now->symbol->type != CHAN || verbose_flags.Active()) {
      fprintf(fd, ";\n#ifdef VAR_RANGES");
      fprintf(fd, "\n\t\tlogval(\"");
      withprocname = terse = nocast = 1;
      _isok++;
      putstmnt(fd, now->left, m);
      withprocname = terse = nocast = 0;
      fprintf(fd, "\", ");
      putstmnt(fd, now->left, m);
      _isok--;
      fprintf(fd, ");\n#endif\n");
      fprintf(fd, "\t\t");
    }
    break;

  case PRINT:
    if (has_enabled || lexer_.GetHasPriority())
      fprintf(fd, "if (TstOnly) return 1; /* T4 */\n\t\t");
#ifdef PRINTF
    fprintf(fd, "printf(%s", now->symbol->name.c_str());
#else
    fprintf(fd, "Printf(%s", now->symbol->name.c_str());
#endif
    for (v = now->left; v; v = v->right) {
      cat2(", ", v->left);
    }
    fprintf(fd, ")");
    break;

  case PRINTM: {
    std::string s;
    if (now->left->symbol && !now->left->symbol->mtype_name) {
      s = now->left->symbol->mtype_name->name;
    }

    if (has_enabled || lexer_.GetHasPriority()) {
      fprintf(fd, "if (TstOnly) return 1; /* T5 */\n\t\t");
    }
    fprintf(fd, "/* YY */ printm(");
    if (now->left && now->left->is_mtype_token) {
      fprintf(fd, "%d", now->left->value);
    } else {
      putstmnt(fd, now->left, m);
    }
    if (!s.empty()) {
      fprintf(fd, ", \"%s\"", s.c_str());
    } else {
      fprintf(fd, ", 0");
    }
    fprintf(fd, ")");
  } break;

  case NAME:
    if (!nocast && now->symbol && now->ResolveSymbolType() < SHORT)
      putname(fd, "((int)", now, m, ")");
    else
      putname(fd, "", now, m, "");
    break;

  case 'p':
    putremote(fd, now, m);
    break;

  case 'q':
    if (terse)
      fprintf(fd, "%s", now->symbol ? now->symbol->name.c_str() : "?");
    else
      fprintf(fd, "%d", remotelab(now));
    break;

  case C_EXPR:
    fprintf(fd, "(");
    codegen::PlunkExpr(fd, now->symbol->name);
#if 1
    fprintf(fd, ")");
#else
    fprintf(fd, ") /* %s */ ", now->symbol->name);
#endif
    break;

  case C_CODE:
    if (now->symbol)
      fprintf(fd, "/* %s */\n\t\t", now->symbol->name.c_str());
    if (has_enabled || lexer_.GetHasPriority())
      fprintf(fd, "if (TstOnly) return 1; /* T6 */\n\t\t");

    if (now->symbol) {
      codegen::PlunkInline(fd, now->symbol->name, 1, GenCode);
    } else {
      loger::fatal("internal error pangen2.c");
    }
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

    cat3("spin_assert(", now->left, ", ");
    terse = nocast = 1;
    cat3("\"", now->left, "\", II, tt, t)");
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
    printf("spin: error, %s:%d, bad node type %d (.m)\n",
           now->file_name->name.c_str(), now->line_number, now->node_type);
    fflush(fd);
    MainProcessor::Exit(1);
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

void putname(FILE *fd, const std::string &pre, models::Lextok *n, int m,
             const std::string &suff) /* varref */
{
  models::Symbol *s = n->symbol;
  std::string ptr;

  file::LineNumber::Set(n->line_number);
  Fname = n->file_name;

  if (!s)
    loger::fatal("no name - putname");

  if (s->context && models::Symbol::GetContext() && s->type)
    s = findloc(s); /* it's a local var */

  if (!s) {
    fprintf(fd, "%s%s%s", pre.c_str(), n->symbol->name.c_str(), suff.c_str());
    return;
  }

  if (!s->type)          /* not a local name */
    s = models::Symbol::BuildOrFind(s->name); /* must be a global */

  if (!s->type) {
    if (pre != ".") {
      loger::fatal("undeclared variable '%s'", s->name);
    }
    s->type = models::kInt;
  }

  if (s->type == PROCTYPE)
    loger::fatal("proctype-name '%s' used as array-name", s->name);

  fprintf(fd, pre.c_str(), 0);
  if (!terse && !s->owner_name && evalindex != 1) {
    if (launch_settings.need_revert_old_rultes_for_priority &&
        s->name == "_priority") {
      fprintf(fd, "1");
      goto shortcut;
    } else {
      if (s->context || s->name == "_p" || s->name == "_pid" ||
          s->name == "_priority") {
        fprintf(fd, "((P%d *)_this)->", Pid_nr);
      } else {
        bool x = s->name == "_";
        if (!(s->hidden_flags & 1) && x == false)
          fprintf(fd, "now.");
        if (x == true && _isok == 0)
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
      putstmnt(fd, n->left, m);
      evalindex = 1;
    } else {
      if (terse ||
          (n->left && n->left->node_type == CONST &&
           n->left->value < s->value_type) ||
          (!n->left && s->value_type > 0)) {
        cat3("[", n->left, "]");
      } else { /* attempt to catch arrays that are indexed with an array element
                * in the same array this causes trouble in the verifier in the
                * backtracking e.g., restoring a[?] in the assignment: a [a[1]]
                * = x where a[1] == 1 but it is hard when the array is inside a
                * structure, so the names don't match
                */
        cat3("[ Index(", n->left, ", ");
        fprintf(fd, "%d) ]", s->value_type);
      }
    }
  } else {
    if (n->left /* effectively a scalar, but with an index */
        && (n->left->node_type != CONST || n->left->value != 0)) {
      loger::fatal("ref to scalar '%s' using array index", ptr.c_str());
    }
  }

  if (s->type == STRUCT && n->right && n->right->left) {
    putname(fd, ".", n->right->left, m, "");
  }
shortcut:
  fprintf(fd, suff.c_str(), 0);
}

void putremote(FILE *fd, models::Lextok *n, int m) /* remote reference */
{
  int promoted = 0;
  int pt;

  if (terse) {
    fprintf(fd, "%s", n->left->symbol->name.c_str()); /* proctype name */
    if (n->left->left) {
      fprintf(fd, "[");
      putstmnt(fd, n->left->left, m); /* pid */
      fprintf(fd, "]");
    }
    if (lexer_.IsLtlMode()) {
      fprintf(fd, ":%s", n->symbol->name.c_str());
    } else {
      fprintf(fd, ".%s", n->symbol->name.c_str());
    }
  } else {
    if (n->ResolveSymbolType() < SHORT) {
      promoted = 1;
      fprintf(fd, "((int)");
    }

    pt = fproc(n->left->symbol->name);
    fprintf(fd, "((P%d *)Pptr(", pt);
    if (n->left->left) {
      fprintf(fd, "BASE+");
      putstmnt(fd, n->left->left, m);
    } else
      fprintf(fd, "f_pid(%d)", pt);
    fprintf(fd, "))->%s", n->symbol->name.c_str());
  }
  if (n->right) {
    fprintf(fd, "[");
    putstmnt(fd, n->right, m); /* array var ref */
    fprintf(fd, "]");
  }
  if (promoted)
    fprintf(fd, ")");
}

static int getweight(
    models::Lextok *n) { /* this piece of code is a remnant of early versions
                          * of the verifier -- in the current version of Spin
                          * only non-zero values matter - so this could probably
                          * simply return 1 in all cases.
                          */
  switch (n->node_type) {
  case 'r':
    return 4;
  case 's':
    return 2;
  case TIMEOUT:
    return 1;
  case 'c':
    if (has_typ(n->left, TIMEOUT))
      return 1;
  }
  return 3;
}

int has_typ(models::Lextok *n, int m) {
  if (!n)
    return 0;
  if (n->node_type == m)
    return 1;
  return (has_typ(n->left, m) || has_typ(n->right, m));
}

static int runcount, opcount;

static void do_count(models::Lextok *n, int checkop) {
  if (!n)
    return;

  switch (n->node_type) {
  case RUN:
    runcount++;
    break;
  default:
    if (checkop)
      opcount++;
    break;
  }
  do_count(n->left, checkop && (n->node_type != RUN));
  do_count(n->right, checkop);
}

void count_runs(models::Lextok *n) {
  runcount = opcount = 0;
  do_count(n, 1);
  if (runcount > 1)
    loger::fatal("more than one run operator in expression", "");
  if (runcount == 1 && opcount > 1)
    loger::fatal("use of run operator in compound expression", "");
}

void any_runs(models::Lextok *n) {
  runcount = opcount = 0;
  do_count(n, 0);
  if (runcount >= 1)
    loger::fatal("run operator used in invalid context", "");
}
