#include "lexer/lexer.hpp"
#include "main/launch_settings.hpp"
#include "models/symbol.hpp"
#include "utils/format/preprocessed_file_viewer.hpp"
#include "utils/format/pretty_print_viewer.hpp"
#include "utils/seed/seed.hpp"
#include <filesystem>
#include <fmt/core.h>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#include "fatal/fatal.hpp"
#include "spin.hpp"
#include "utils/verbose/verbose.hpp"
#include "version/version.hpp"
#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#ifdef PC
#include <io.h>
#else
#include <unistd.h>
#endif
#include "y.tab.h"

extern int DstepStart, lineno;
extern FILE *yyin, *yyout, *tl_out;
extern models::Symbol *context;
extern char *claimproc;
extern void repro_src(void);
extern void qhide(int);
extern char CurScope[MAXSCOPESZ];
extern lexer::Lexer lexer_;
extern short has_accept;
extern int realread;
extern void ana_src(int, int);
extern void putprelude(void);

static void add_comptime(char *);
static void add_runtime(char *);

extern models::Symbol *Fname, *oFname;

int Etimeouts; /* nr timeouts in program */
int Ntimeouts; /* nr timeouts in never claim */
int has_remote, has_remvar;
int limited_vis;

extern LaunchSettings launch_settings;
int implied_semis = 1;
int ccache = 0; /* oyvind teig: 5.2.0 case caching off by default */

static int itsr, itsr_n, sw_or_bt;

static char *ltl_claims = (char *)0;
static char *pan_runtime = "";
static char *pan_comptime = "";
static char *formula = NULL;
static char out1[64];

#ifndef CPP
/* to use visual C++:
        #define CPP	"cl -E/E"
   or call spin as:	spin -P"CL -E/E"

   on OS2:
        #define CPP	"icc -E/Pd+ -E/Q+"
   or call spin as:	spin -P"icc -E/Pd+ -E/Q+"
   make sure the -E arg is always at the end,
   in each case, because the command line
   can later be truncated at that point
*/
#if 1
#define CPP                                                                    \
  "gcc -std=gnu99 -Wno-unknown-warning-option -Wformat-overflow=0 -E -x c"
/* if gcc-4 is available, this setting is modified below */
#else
#if defined(PC) || defined(MAC)
#define CPP "gcc -std=gnu99 -E -x c"
#else
#ifdef SOLARIS
#define CPP "/usr/ccs/lib/cpp"
#else
#define CPP "cpp" /* sometimes: "/lib/cpp" */
#endif
#endif
#endif
#endif

static char PreProc[512];

extern int depth; /* at least some steps were made */

void final_fiddle(void) {
  char *has_a, *has_l, *has_f;

  /* no -a or -l but has_accept: add -a */
  /* no -a or -l in pan_runtime: add -DSAFETY to pan_comptime */
  /* -a or -l but no -f then add -DNOFAIR */

  has_a = strstr(pan_runtime, "-a");
  has_l = strstr(pan_runtime, "-l");
  has_f = strstr(pan_runtime, "-f");

  if (!has_l && !has_a && strstr(pan_comptime, "-DNP")) {
    add_runtime("-l");
    has_l = strstr(pan_runtime, "-l");
  }

  if (!has_a && !has_l && !strstr(pan_comptime, "-DSAFETY")) {
    if (has_accept && !strstr(pan_comptime, "-DBFS") &&
        !strstr(pan_comptime, "-DNOCLAIM")) {
      add_runtime("-a");
      has_a = pan_runtime;
    } else {
      add_comptime("-DSAFETY");
    }
  }

  if ((has_a || has_l) && !has_f && !strstr(pan_comptime, "-DNOFAIR")) {
    add_comptime("-DNOFAIR");
  }
}

static int change_param(char *t, char *what, int range, int bottom) {
  char *ptr;
  int v;

  assert(range < 1000 && range > 0);
  if ((ptr = strstr(t, what)) != NULL) {
    ptr += strlen(what);
    if (!isdigit((int)*ptr)) {
      return 0;
    }
    v = atoi(ptr) + 1; /* was: v = (atoi(ptr)+1)%range */
    if (v >= range) {
      v = bottom;
    }
    if (v >= 100) {
      *ptr++ = '0' + (v / 100);
      v %= 100;
      *ptr++ = '0' + (v / 10);
      *ptr = '0' + (v % 10);
    } else if (v >= 10) {
      *ptr++ = '0' + (v / 10);
      *ptr++ = '0' + (v % 10);
      *ptr = ' ';
    } else {
      *ptr++ = '0' + v;
      *ptr++ = ' ';
      *ptr = ' ';
    }
  }
  return 1;
}

static void change_rs(char *t) {
  char *ptr;
  int cnt = 0;
  long v;

  if ((ptr = strstr(t, "-RS")) != NULL) {
    ptr += 3;
    /* room for at least 10 digits */
    v = Rand() % 1000000000L;
    while (v / 10 > 0) {
      *ptr++ = '0' + v % 10;
      v /= 10;
      cnt++;
    }
    *ptr++ = '0' + v;
    cnt++;
    while (cnt++ < 10) {
      *ptr++ = ' ';
    }
  }
}

int omit_str(char *in, char *s) {
  char *ptr = strstr(in, s);
  int i, nr = -1;

  if (ptr) {
    for (i = 0; i < (int)strlen(s); i++) {
      *ptr++ = ' ';
    }
    if (isdigit((int)*ptr)) {
      nr = atoi(ptr);
      while (isdigit((int)*ptr)) {
        *ptr++ = ' ';
      }
    }
  }
  return nr;
}

void string_trim(char *t) {
  int n = strlen(t) - 1;

  while (n > 0 && t[n] == ' ') {
    t[n--] = '\0';
  }
}

int e_system(int v, const char *s) {
  static int count = 1;
  auto &verbose_flags = utils::verbose::Flags::getInstance();
  /* v == 0 : checks to find non-linked version of gcc */
  /* v == 1 : all other commands */
  /* v == 2 : preprocessing the promela input */

  if (v == 1) {
    if (verbose_flags.NeedToPrintVerbose() ||
        verbose_flags.NeedToPrintVeryVerbose()) /* -v or -w */
    {
      printf("cmd%02d: %s\n", count++, s);
      fflush(stdout);
    }
    if (verbose_flags.NeedToPrintVeryVerbose()) /* only -w */
    {
      return 0; /* suppress the call to system(s) */
    }
  }
  return system(s);
}

void alldone(int estatus) { return; }
#if 0
	-P0	normal active process creation
	-P1	reversed order for *active* process creation != p_reverse

	-T0	normal transition exploration
	-T1	reversed order of transition exploration

	-DP_RAND	(random starting point +- -DP_REVERSE)
	-DPERMUTED	(also enables -p_rotateN and -p_reverse)
	-DP_REVERSE	(same as -DPERMUTED with -p_reverse, but 7% faster)

	-DT_RAND	(random starting point -- optionally with -T0..1)
	-DT_REVERSE	(superseded by -T0..1 options)

	 -hash generates new hash polynomial for -h0

	permutation modes:
	 -permuted (adds -DPERMUTED) -- this is also the default with -swarm
	 -t_reverse (same as -T1)
	 -p_reverse (similar to -P1)
	 -p_rotateN
	 -p_normal

	less useful would be (since there is less non-determinism in transitions):
		-t_rotateN -- a controlled version of -DT_RAND

	compiling with -DPERMUTED enables a number of new runtime options,
	that -swarmN,M will also exploit:
		-p_permute (default)
		-p_rotateN
		-p_reverse
#endif

void preprocess(char *a, char *b, int a_tmp) { return; }

void usage(void) {
  printf("use: spin [-option] ... [-option] file\n");
  printf("\tNote: file must always be the last argument\n");
  printf("\t-A apply slicing algorithm\n");
  printf("\t-a generate a verifier in pan.c\n");
  printf("\t-B no final state details in simulations\n");
  printf("\t-b don't execute printfs in simulation\n");
  printf("\t-C print channel access info (combine with -g etc.)\n");
  printf("\t-c columnated -s -r simulation output\n");
  printf("\t-d produce symbol-table information\n");
  printf("\t-Dyyy pass -Dyyy to the preprocessor\n");
  printf("\t-Eyyy pass yyy to the preprocessor\n");
  printf("\t-e compute synchronous product of multiple never claims (modified "
         "by -L)\n");
  printf("\t-f \"..formula..\"  translate LTL ");
  printf("into never claim\n");
  printf(
      "\t-F file  like -f, but with the LTL formula stored in a 1-line file\n");
  printf("\t-g print all global variables\n");
  printf(
      "\t-h at end of run, print value of seed for random nr generator used\n");
  printf("\t-i interactive (random simulation)\n");
  printf("\t-I show result of inlining and preprocessing\n");
  printf("\t-J reverse eval order of nested unlesses\n");
  printf("\t-jN skip the first N steps ");
  printf("in simulation trail\n");
  printf("\t-k fname use the trailfile stored in file fname, see also -t\n");
  printf("\t-L when using -e, use strict language intersection\n");
  printf("\t-l print all local variables\n");
  printf("\t-M generate msc-flow in tcl/tk format\n");
  printf("\t-m lose msgs sent to full queues\n");
  printf("\t-N fname use never claim stored in file fname\n");
  printf("\t-nN seed for random nr generator\n");
  printf("\t-O use old scope rules (pre 5.3.0)\n");
  printf("\t-o1 turn off dataflow-optimizations in verifier\n");
  printf("\t-o2 don't hide write-only variables in verifier\n");
  printf("\t-o3 turn off statement merging in verifier\n");
  printf("\t-o4 turn on rendezvous optiomizations in verifier\n");
  printf("\t-o5 turn on case caching (reduces size of pan.m, but affects "
         "reachability reports)\n");
  printf("\t-o6 revert to the old rules for interpreting priority tags (pre "
         "version 6.2)\n");
  printf(
      "\t-o7 revert to the old rules for semi-colon usage (pre version 6.3)\n");
  printf("\t-Pxxx use xxx for preprocessing\n");
  printf("\t-p print all statements\n");
  printf("\t-pp pretty-print (reformat) stdin, write stdout\n");
  printf("\t-qN suppress io for queue N in printouts\n");
  printf("\t-r print receive events\n");
  printf("\t-replay  replay an error trail-file found earlier\n");
  printf("\t	if the model contains embedded c-code, the ./pan executable is "
         "used\n");
  printf("\t	otherwise spin itself is used to replay the trailfile\n");
  printf("\t	note that pan recognizes different runtime options than spin "
         "itself\n");
  printf("\t-run  (or -search) generate a verifier, and compile and run it\n");
  printf("\t      options before -search are interpreted by spin to parse the "
         "input\n");
  printf("\t      options following a -search are used to compile and run the "
         "verifier pan\n");
  printf("\t	    valid options that can follow a -search argument "
         "include:\n");
  printf("\t	    -bfs	perform a breadth-first search\n");
  printf("\t	    -bfspar	perform a parallel breadth-first search\n");
  printf("\t	    -dfspar	perform a parallel depth-first search, same as "
         "-DNCORE=4\n");
  printf("\t	    -bcs	use the bounded-context-switching algorithm\n");
  printf("\t	    -bitstate	or -bit, use bitstate storage\n");
  printf("\t	    -biterateN,M use bitstate with iterative search refinement "
         "(-w18..-w35)\n");
  printf("\t			perform N randomized runs and increment -w "
         "every M runs\n");
  printf("\t			default value for N is 10, default for M is "
         "1\n");
  printf("\t			(use N,N to keep -w fixed for all runs)\n");
  printf("\t			(add -w to see which commands will be "
         "executed)\n");
  printf("\t			(add -W if ./pan exists and need not be "
         "recompiled)\n");
  printf("\t	    -swarmN,M like -biterate, but running all iterations in "
         "parallel\n");
  printf("\t	    -link file.c  link executable pan to file.c\n");
  printf("\t	    -collapse	use collapse state compression\n");
  printf("\t	    -noreduce	do not use partial order reduction\n");
  printf("\t	    -hc  	use hash-compact storage\n");
  printf("\t	    -noclaim	ignore all ltl and never claims\n");
  printf("\t	    -p_permute	use process scheduling order random "
         "permutation\n");
  printf("\t	    -p_rotateN	use process scheduling order rotation by N\n");
  printf("\t	    -p_reverse	use process scheduling order reversal\n");
  printf("\t	    -rhash      randomly pick one of the -p_... options\n");
  printf("\t	    -ltl p	verify the ltl property named p\n");
  printf("\t	    -safety	compile for safety properties only\n");
  printf("\t	    -i	    	use the dfs iterative shortening algorithm\n");
  printf("\t	    -a	    	search for acceptance cycles\n");
  printf("\t	    -l	    	search for non-progress cycles\n");
  printf("\t	similarly, a -D... parameter can be specified to modify the "
         "compilation\n");
  printf("\t	and any valid runtime pan argument can be specified for the "
         "verification\n");
  printf("\t-S1 and -S2 separate pan source for claim and model\n");
  printf("\t-s print send events\n");
  printf("\t-T do not indent printf output\n");
  printf("\t-t[N] follow [Nth] simulation trail, see also -k\n");
  printf("\t-Uyyy pass -Uyyy to the preprocessor\n");
  printf("\t-uN stop a simulation run after N steps\n");
  printf("\t-v verbose, more warnings\n");
  printf("\t-w very verbose (when combined with -l or -g)\n");
  printf("\t-[XYZ] reserved for use by xspin interface\n");
  printf("\t-V print version number and exit\n");
  alldone(1);
}

int optimizations(int nr) { return 0; }

static void add_comptime(char *s) {
  char *tmp;

  if (!s || strstr(pan_comptime, s)) {
    return;
  }

  tmp = (char *)emalloc(strlen(pan_comptime) + strlen(s) + 2);
  sprintf(tmp, "%s %s", pan_comptime, s);
  pan_comptime = tmp;
}

static struct {
  char *ifsee, *thendo;
  int keeparg;
} pats[] = {{"-bfspar", "-DBFS_PAR", 0},
            {"-bfs", "-DBFS", 0},
            {"-bcs", "-DBCS", 0},
            {"-bitstate", "-DBITSTATE", 0},
            {"-bit", "-DBITSTATE", 0},
            {"-hc", "-DHC4", 0},
            {"-collapse", "-DCOLLAPSE", 0},
            {"-noclaim", "-DNOCLAIM", 0},
            {"-noreduce", "-DNOREDUCE", 0},
            {"-np", "-DNP", 0},
            {"-permuted", "-DPERMUTED", 0},
            {"-p_permute", "-DPERMUTED", 1},
            {"-p_rotate", "-DPERMUTED", 1},
            {"-p_reverse", "-DPERMUTED", 1},
            {"-rhash", "-DPERMUTED", 1},
            {"-safety", "-DSAFETY", 0},
            {"-i", "-DREACH", 1},
            {"-l", "-DNP", 1},
            {0, 0}};

static void set_itsr_n(char *s) /* e.g., -swarm12,3 */
{
  char *tmp;

  if ((tmp = strchr(s, ',')) != NULL) {
    tmp++;
    if (*tmp != '\0' && isdigit((int)*tmp)) {
      itsr_n = atoi(tmp);
      if (itsr_n < 2) {
        itsr_n = 0;
      }
    }
  }
}

static void add_runtime(char *s) {
  char *tmp;
  int i;

  if (strncmp(s, "-biterate", strlen("-biterate")) == 0) {
    itsr = 10; /* default nr of sequential iterations */
    sw_or_bt = 1;
    if (isdigit((int)s[9])) {
      itsr = atoi(&s[9]);
      if (itsr < 1) {
        itsr = 1;
      }
      set_itsr_n(s);
    }
    return;
  }
  if (strncmp(s, "-swarm", strlen("-swarm")) == 0) {
    itsr = -10; /* parallel iterations */
    sw_or_bt = 1;
    if (isdigit((int)s[6])) {
      itsr = atoi(&s[6]);
      if (itsr < 1) {
        itsr = 1;
      }
      itsr = -itsr;
      set_itsr_n(s);
    }
    return;
  }

  for (i = 0; pats[i].ifsee; i++) {
    if (strncmp(s, pats[i].ifsee, strlen(pats[i].ifsee)) == 0) {
      add_comptime(pats[i].thendo);
      if (pats[i].keeparg) {
        break;
      }
      return;
    }
  }
  if (strncmp(s, "-dfspar", strlen("-dfspar")) == 0) {
    add_comptime("-DNCORE=4");
    return;
  }

  tmp = (char *)emalloc(strlen(pan_runtime) + strlen(s) + 2);
  sprintf(tmp, "%s %s", pan_runtime, s);
  pan_runtime = tmp;
}

#ifdef __MINGW32__
/* mingw on PCs doesn't have a definition of getline
 * so we fall back on using a fixed size buffer, to
 * avoid having to reimplement getline here·..
 */
ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
  static char buffer[8192];

  *lineptr = (char *)&buffer;

  if (!fgets(buffer, sizeof(buffer), stream)) {
    return 0;
  }
  return 1;
}
#endif

void ltl_list(const std::string &, const std::string &) {
  if (true
      // s_trail || launch_settings.need_to_analyze ||
      //       dumptab
      ) /* when generating pan.c or replaying a trace */
  {
    if (!ltl_claims) {
      ltl_claims = "_spin_nvr.tmp";
      /* if ((fd_ltl = fopen(ltl_claims, MFLAGS)) == NULL) {
         loger::fatal("cannot open tmp file %s", ltl_claims);
       }
       tl_out = fd_ltl;
     */
    }
    /*
    add_ltl = (char **)emalloc(5 * sizeof(char *));
    add_ltl[1] = "-c";
    add_ltl[2] = nm;
    add_ltl[3] = "-f";
    add_ltl[4] = (char *)emalloc(strlen(fm) + 4);
    strcpy(add_ltl[4], "!(");
    strcat(add_ltl[4], fm);
    strcat(add_ltl[4], ")");
    */
    /* add_ltl[4] = fm; */
    // TODO:    nr_errs += tl_main(4, add_ltl);

    fflush(tl_out);
    /* should read this file after the main file is read */
  }
}

char *emalloc(size_t n) {
  char *tmp;
  static unsigned long cnt = 0;

  if (n == 0)
    return NULL; /* robert shelton 10/20/06 */

  if (!(tmp = (char *)malloc(n))) {
    printf("spin: allocated %ld Gb, wanted %d bytes more\n",
           cnt / (1024 * 1024 * 1024), (int)n);
    loger::fatal("not enough memory");
  }
  cnt += (unsigned long)n;
  memset(tmp, 0, n);
  return tmp;
}

void trapwonly(Lextok *n /* , char *unused */) {
  short i;

  if (!n) {
    loger::fatal("unexpected error,");
  }

  i = (n->sym) ? n->sym->type : 0;

  /* printf("%s	realread %d type %d\n", n->sym?n->sym->name:"--", realread, i);
   */

  if (realread && (i == MTYPE || i == BIT || i == BYTE || i == SHORT ||
                   i == INT || i == UNSIGNED)) {
    n->sym->hidden_flags |= 128; /* var is read at least once */
  }
}

void setaccess(models::Symbol *sp, models::Symbol *what, int cnt, int t) {
  Access *a;

  for (a = sp->access; a; a = a->lnk)
    if (a->who == context && a->what == what && a->cnt == cnt && a->typ == t)
      return;

  a = (Access *)emalloc(sizeof(Access));
  a->who = context;
  a->what = what;
  a->cnt = cnt;
  a->typ = t;
  a->lnk = sp->access;
  sp->access = a;
}

Lextok *nn(Lextok *s, int t, Lextok *ll, Lextok *rl) {
  Lextok *n = (Lextok *)emalloc(sizeof(Lextok));
  static int warn_nn = 0;

  n->uiid = is_inline(); /* record origin of the statement */
  n->ntyp = (unsigned short)t;
  if (s && s->fn) {
    n->ln = s->ln;
    n->fn = s->fn;
  } else if (rl && rl->fn) {
    n->ln = rl->ln;
    n->fn = rl->fn;
  } else if (ll && ll->fn) {
    n->ln = ll->ln;
    n->fn = ll->fn;
  } else {
    n->ln = lineno;
    n->fn = Fname;
  }
  if (s)
    n->sym = s->sym;
  n->lft = ll;
  n->rgt = rl;
  n->indstep = DstepStart;

  if (t == TIMEOUT)
    Etimeouts++;

  if (!context)
    return n;

  if (t == 'r' || t == 's')
    setaccess(n->sym, ZS, 0, t);
  if (t == 'R')
    setaccess(n->sym, ZS, 0, 'P');

  if (context->name.c_str() == claimproc) {
    int forbidden = launch_settings.separate_version;
    switch (t) {
    case ASGN:
      printf("spin: Warning, never claim has side-effect\n");
      break;
    case 'r':
    case 's':
      loger::non_fatal("never claim contains i/o stmnts");
      break;
    case TIMEOUT:
      /* never claim polls timeout */
      if (Ntimeouts && Etimeouts)
        forbidden = 0;
      Ntimeouts++;
      Etimeouts--;
      break;
    case LEN:
    case EMPTY:
    case FULL:
    case 'R':
    case NFULL:
    case NEMPTY:
      /* status becomes non-exclusive */
      if (n->sym && !(n->sym->xu & XX)) {
        n->sym->xu |= XX;
        if (launch_settings.separate_version == 2) {
          printf("spin: warning, make sure that the S1 model\n");
          printf("      also polls channel '%s' in its claim\n",
                 n->sym->name.c_str());
        }
      }
      forbidden = 0;
      break;
    case 'c':
      AST_track(n, 0); /* register as a slice criterion */
                       /* fall thru */
    default:
      forbidden = 0;
      break;
    }
    if (forbidden) {
      std::cout << "spin: never, saw " << loger::explainToString(t)
                << std::endl;
      loger::fatal("incompatible with separate compilation");
    }
  } else if ((t == ENABLED || t == PC_VAL) && !(warn_nn & t)) {
    std::cout << fmt::format("spin: Warning, using {} outside never claim",
                             (t == ENABLED) ? "enabled()" : "pc_value()")
              << std::endl;
    warn_nn |= t;
  } else if (t == NONPROGRESS) {
    loger::fatal("spin: Error, using np_ outside never claim\n");
  }
  return n;
}

Lextok *rem_lab(models::Symbol *a, Lextok *b,
                models::Symbol *c) /* proctype name, pid, label name */
{
  Lextok *tmp1, *tmp2, *tmp3;

  has_remote++;
  c->type = models::kLabel; /* refered to in global context here */
  fix_dest(c, a);           /* in case target of rem_lab is jump */
  tmp1 = nn(ZN, '?', b, ZN);
  tmp1->sym = a;
  tmp1 = nn(ZN, 'p', tmp1, ZN);
  tmp1->sym = lookup("_p");
  tmp2 = nn(ZN, NAME, ZN, ZN);
  tmp2->sym = a;
  tmp3 = nn(ZN, 'q', tmp2, ZN);
  tmp3->sym = c;
  return nn(ZN, EQ, tmp1, tmp3);
#if 0
	      .---------------EQ-------.
	     /                          \
	   'p' -sym-> _p               'q' -sym-> c (label name)
	   /                           /
	 '?' -sym-> a (proctype)     NAME -sym-> a (proctype name)
	 / 
	b (pid expr)
#endif
}

Lextok *rem_var(models::Symbol *a, Lextok *b, models::Symbol *c, Lextok *ndx) {
  Lextok *tmp1;

  has_remote++;
  has_remvar++;
  launch_settings.need_use_dataflow_optimizations = false;
  launch_settings.need_statemate_merging = false;

  tmp1 = nn(ZN, '?', b, ZN);
  tmp1->sym = a;
  tmp1 = nn(ZN, 'p', tmp1, ndx);
  tmp1->sym = c;
  return tmp1;
#if 0
	cannot refer to struct elements
	only to scalars and arrays

	    'p' -sym-> c (variable name)
	    / \______  possible arrayindex on c
	   /
	 '?' -sym-> a (proctype)
	 / 
	b (pid expr)
#endif
}