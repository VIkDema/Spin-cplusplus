#include "fatal/fatal.hpp"
#include "lexer/lexer.hpp"
#include "main/launch_settings.hpp"
#include "models/access.hpp"
#include "models/lextok.hpp"
#include "models/symbol.hpp"
#include "spin.hpp"
#include "utils/format/preprocessed_file_viewer.hpp"
#include "utils/format/pretty_print_viewer.hpp"
#include "utils/seed/seed.hpp"
#include "utils/verbose/verbose.hpp"
#include "version/version.hpp"
#include <assert.h>
#include <filesystem>
#include <fmt/core.h>
#include <iostream>
#include <optional>
#include <signal.h>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <string_view>
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
extern lexer::Lexer lexer_;
extern short has_accept;
extern int realread;
extern void ana_src(int, int);
extern void putprelude(void);

extern models::Symbol *Fname, *oFname;

int Etimeouts; /* nr timeouts in program */
int Ntimeouts; /* nr timeouts in never claim */
int has_remote, has_remvar;
int limited_vis;

extern LaunchSettings launch_settings;

static char *ltl_claims = (char *)0;

extern int depth; /* at least some steps were made */

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

void trapwonly(models::Lextok *n /* , char *unused */) {
  short i;

  if (!n) {
    loger::fatal("unexpected error,");
  }

  i = (n->symbol) ? n->symbol->type : 0;

  /* printf("%s	realread %d type %d\n", n->symbol?n->symbol->name:"--",
   * realread, i);
   */

  if (realread && (i == MTYPE || i == BIT || i == BYTE || i == SHORT ||
                   i == INT || i == UNSIGNED)) {
    n->symbol->hidden_flags |= 128; /* var is read at least once */
  }
}

void setaccess(models::Symbol *sp, models::Symbol *what, int count, int t) {
  models::Access *a;

  for (a = sp->access; a; a = a->next)
    if (a->who == context && a->what == what && a->count == count &&
        a->type == t)
      return;

  a = (models::Access *)emalloc(sizeof(models::Access));
  a->who = context;
  a->what = what;
  a->count = count;
  a->type = t;
  a->next = sp->access;
  sp->access = a;
}

models::Lextok *nn(models::Lextok *s, int t, models::Lextok *ll,
                   models::Lextok *rl) {
  models::Lextok *n = (models::Lextok *)emalloc(sizeof(models::Lextok));
  static int warn_nn = 0;

  n->opt_inline_id = is_inline(); /* record origin of the statement */
  n->node_type = (unsigned short)t;
  if (s && s->file_name) {
    n->line_number = s->line_number;
    n->file_name = s->file_name;
  } else if (rl && rl->file_name) {
    n->line_number = rl->line_number;
    n->file_name = rl->file_name;
  } else if (ll && ll->file_name) {
    n->line_number = ll->line_number;
    n->file_name = ll->file_name;
  } else {
    n->line_number = lineno;
    n->file_name = Fname;
  }
  if (s)
    n->symbol = s->symbol;
  n->left = ll;
  n->right = rl;
  n->index_step = DstepStart;

  if (t == TIMEOUT)
    Etimeouts++;

  if (!context)
    return n;

  if (t == 'r' || t == 's')
    setaccess(n->symbol, ZS, 0, t);
  if (t == 'R')
    setaccess(n->symbol, ZS, 0, 'P');

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
      if (n->symbol && !(n->symbol->xu & XX)) {
        n->symbol->xu |= XX;
        if (launch_settings.separate_version == 2) {
          printf("spin: warning, make sure that the S1 model\n");
          printf("      also polls channel '%s' in its claim\n",
                 n->symbol->name.c_str());
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

models::Lextok *rem_lab(models::Symbol *a, models::Lextok *b,
                        models::Symbol *c) /* proctype name, pid, label name */
{
  models::Lextok *tmp1, *tmp2, *tmp3;

  has_remote++;
  c->type = models::kLabel; /* refered to in global context here */
  fix_dest(c, a);           /* in case target of rem_lab is jump */
  tmp1 = nn(ZN, '?', b, ZN);
  tmp1->symbol = a;
  tmp1 = nn(ZN, 'p', tmp1, ZN);
  tmp1->symbol = lookup("_p");
  tmp2 = nn(ZN, NAME, ZN, ZN);
  tmp2->symbol = a;
  tmp3 = nn(ZN, 'q', tmp2, ZN);
  tmp3->symbol = c;
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

models::Lextok *rem_var(models::Symbol *a, models::Lextok *b, models::Symbol *c,
                        models::Lextok *ndx) {
  models::Lextok *tmp1;

  has_remote++;
  has_remvar++;
  launch_settings.need_use_dataflow_optimizations = false;
  launch_settings.need_statemate_merging = false;

  tmp1 = nn(ZN, '?', b, ZN);
  tmp1->symbol = a;
  tmp1 = nn(ZN, 'p', tmp1, ndx);
  tmp1->symbol = c;
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