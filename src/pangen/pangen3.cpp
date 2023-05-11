/***** spin: pangen3.c *****/

#include "../lexer/lexer.hpp"
#include "../spin.hpp"
#include "y.tab.h"
#include <assert.h>
#include "../main/launch_settings.hpp"
#include "../symbol/symbol.hpp"
#include "../trail/mesg.hpp"

extern LaunchSettings launch_settings;
extern FILE *fd_th, *fd_tc;
extern int eventmapnr, in_settr;

struct SRC {
  int line_number, st;         /* linenr, statenr */
  models::Symbol *file_name; /* filename */
  struct SRC *next;
};

static int col;
static models::Symbol *lastfnm;
static models::Symbol lastdef;
static int lastfrom;
static SRC *frst = (SRC *)0;
static SRC *skip = (SRC *)0;
extern lexer::Lexer lexer_;


static void putnr(int n) {
  if (col++ == 8) {
    fprintf(fd_tc, "\n\t"); /* was th */
    col = 1;
  }
  fprintf(fd_tc, "%3d, ", n); /* was th */
}

static void putfnm(int j, models::Symbol *s) {
  if (lastfnm && lastfnm == s && j != -1)
    return;

  if (lastfnm)
    fprintf(fd_tc, "{ \"%s\", %d, %d },\n\t", /* was th */
            lastfnm->name.c_str(), lastfrom, j - 1);
  lastfnm = s;
  lastfrom = j;
}

static void putfnm_flush(int j) {
  if (lastfnm)
    fprintf(fd_tc, "{ \"%s\", %d, %d }\n", /* was th */
            lastfnm->name.c_str(), lastfrom, j);
}

static SRC *newsrc(int m, SRC *n) {
  SRC *tmp;
  tmp = (SRC *)emalloc(sizeof(SRC));
  tmp->st = m;
  tmp->next = n;
  return tmp;
}

void putskip(int m) /* states that need not be reached */
{
  SRC *tmp, *lst = (SRC *)0;
  /* 6.4.0: now an ordered list */
  for (tmp = skip; tmp; lst = tmp, tmp = tmp->next) {
    if (tmp->st == m) {
      return;
    }
    if (tmp->st > m) /* insert before */
    {
      if (tmp == skip) {
        tmp = newsrc(m, skip);
        skip = tmp;
      } else {
        assert(lst);
        tmp = newsrc(m, lst->next);
        lst->next = tmp;
      }
      return;
    }
  }
  /* insert at the end */
  if (lst) {
    lst->next = newsrc(m, 0);
  } else /* empty list */
  {
    skip = newsrc(m, 0);
  }
}

void unskip(int m) /* a state that needs to be reached after all */
{
  SRC *tmp, *lst = (SRC *)0;

  for (tmp = skip; tmp; lst = tmp, tmp = tmp->next) {
    if (tmp->st == m) {
      if (tmp == skip)
        skip = skip->next;
      else if (lst) /* always true, but helps coverity */
        lst->next = tmp->next;
      break;
    }
    if (tmp->st > m) {
      break; /* m is not in list */
    }
  }
}

void putsrc(models::Element *e) /* match states to source lines */
{
  SRC *tmp, *lst = (SRC *)0;
  int n, m;

  if (!e || !e->n)
    return;

  n = e->n->line_number;
  m = e->seqno;
  /* 6.4.0: now an ordered list */
  for (tmp = frst; tmp; lst = tmp, tmp = tmp->next) {
    if (tmp->st == m) {
      if (tmp->line_number != n || tmp->file_name != e->n->file_name)
        printf("putsrc mismatch seqno %d, line %d - %d, file %s\n", m, n,
               tmp->line_number, tmp->file_name->name.c_str());
      return;
    }
    if (tmp->st > m) /* insert before */
    {
      if (tmp == frst) {
        tmp = newsrc(m, frst);
        frst = tmp;
      } else {
        assert(lst);
        tmp = newsrc(m, lst->next);
        lst->next = tmp;
      }
      tmp->line_number = n;
      tmp->file_name = e->n->file_name;
      return;
    }
  }
  /* insert at the end */
  tmp = newsrc(m, lst ? lst->next : 0);
  tmp->line_number = n;
  tmp->file_name = e->n->file_name;
  if (lst) {
    lst->next = tmp;
  } else {
    frst = tmp;
  }
}

static void dumpskip(int n, int m) {
  SRC *tmp, *lst;
  FILE *tz = fd_tc; /* was fd_th */
  int j;

  fprintf(tz, "uchar reached%d [] = {\n\t", m);
  tmp = skip;
  lst = (SRC *)0;
  for (j = 0, col = 0; j <= n; j++) { /* find j in the sorted list */
    for (; tmp; lst = tmp, tmp = tmp->next) {
      if (tmp->st == j) {
        putnr(1);
        if (lst)
          lst->next = tmp->next;
        else
          skip = tmp->next;
        break;
      }
      if (tmp->st > j) {
        putnr(0);
        break; /* j is not in the list */
      }
    }

    if (!tmp) {
      putnr(0);
    }
  }
  fprintf(tz, "};\n");
  fprintf(tz, "uchar *loopstate%d;\n", m);

  if (m == eventmapnr)
    fprintf(fd_th, "#define reached_event	reached%d\n", m);

  skip = (SRC *)0;
}

void dumpsrc(int n, int m) {
  SRC *tmp, *lst;
  int j;
  static int did_claim = 0;
  FILE *tz = fd_tc; /* was fd_th */

  fprintf(tz, "\nshort src_ln%d [] = {\n\t", m);
  tmp = frst;
  for (j = 0, col = 0; j <= n; j++) {
    for (; tmp; tmp = tmp->next) {
      if (tmp->st == j) {
        putnr(tmp->line_number);
        break;
      }
      if (tmp->st > j) {
        putnr(0);
        break;
      }
    }
    if (!tmp) {
      putnr(0);
    }
  }
  fprintf(tz, "};\n");

  lastfnm = (models::Symbol *)0;
  lastdef.name = "-";
  fprintf(tz, "S_F_MAP src_file%d [] = {\n\t", m);
  tmp = frst;
  lst = (SRC *)0;
  for (j = 0, col = 0; j <= n; j++) {
    for (; tmp; lst = tmp, tmp = tmp->next) {
      if (tmp->st == j) {
        putfnm(j, tmp->file_name);
        if (lst)
          lst->next = tmp->next;
        else
          frst = tmp->next;
        break;
      }
      if (tmp->st > j) {
        putfnm(j, &lastdef);
        break;
      }
    }
    if (!tmp) {
      putfnm(j, &lastdef);
    }
  }
  putfnm_flush(j);
  fprintf(tz, "};\n");

  if (pid_is_claim(m) && !did_claim) {
    fprintf(tz, "short *src_claim;\n");
    did_claim++;
  }
  if (m == eventmapnr)
    fprintf(fd_th, "#define src_event	src_ln%d\n", m);

  frst = (SRC *)0;
  dumpskip(n, m);
}

#define Cat0(x)                                                                \
  comwork(fd, now->left, m);                                                    \
  fprintf(fd, x);                                                              \
  comwork(fd, now->right, m)
#define Cat1(x)                                                                \
  fprintf(fd, "(");                                                            \
  Cat0(x);                                                                     \
  fprintf(fd, ")")
#define Cat2(x, y)                                                             \
  fprintf(fd, x);                                                              \
  comwork(fd, y, m)
#define Cat3(x, y, z)                                                          \
  fprintf(fd, x);                                                              \
  comwork(fd, y, m);                                                           \
  fprintf(fd, z)

static int symbolic(FILE *fd, models::Lextok *tv) {
  models::Lextok *n, *Mtype;
  int cnt = 1;

  if (tv->is_mtype_token) {
    std::string s = "_unnamed_";
    if (tv->symbol && tv->symbol->mtype_name) {
      s = tv->symbol->mtype_name->name;
    }
    Mtype = *symbol::GetListOfMtype(s);
    for (n = Mtype; n; n = n->right, cnt++) {
      if (cnt == tv->value) {
        fprintf(fd, "%s", n->left->symbol->name.c_str());
        return 1;
      }
    }
  }

  return 0;
}

static void comwork(FILE *fd, models::Lextok *now, int m) {
  models::Lextok *v;
  std::string s;
  int i, j;

  if (!now) {
    fprintf(fd, "0");
    return;
  }
  char *now_val_two = "!!";
  char *now_val_one = "!";
  switch (now->node_type) {
  case CONST:
    if (now->is_mtype_token && now->symbol && now->symbol->mtype_name) {
      s = now->symbol->mtype_name->name;
    }
    mesg::PrintFormattedMessage(fd, now->value, now->is_mtype_token, s.c_str());
    break;

  case '!':
    Cat3("!(", now->left, ")");
    break;
  case UMIN:
    Cat3("-(", now->left, ")");
    break;
  case '~':
    Cat3("~(", now->left, ")");
    break;

  case '/':
    Cat1("/");
    break;
  case '*':
    Cat1("*");
    break;
  case '-':
    Cat1("-");
    break;
  case '+':
    Cat1("+");
    break;
  case '%':
    Cat1("%%");
    break;
  case '&':
    Cat1("&");
    break;
  case '^':
    Cat1("^");
    break;
  case '|':
    Cat1("|");
    break;
  case LE:
    Cat1("<=");
    break;
  case GE:
    Cat1(">=");
    break;
  case GT:
    Cat1(">");
    break;
  case LT:
    Cat1("<");
    break;
  case NE:
    Cat1("!=");
    break;
  case EQ:
    if (lexer_.IsLtlMode() && now->left->node_type == 'p' &&
        now->right->node_type == 'q') /* remote ref */
    {
      models::Lextok *p = now->left->left;

      fprintf(fd, "(");
      fprintf(fd, "%s", p->symbol->name.c_str());
      if (p->left) {
        fprintf(fd, "[");
        putstmnt(fd, p->left, 0); /* pid */
        fprintf(fd, "]");
      }
      fprintf(fd, "@");
      fprintf(fd, "%s", now->right->symbol->name.c_str());
      fprintf(fd, ")");
      break;
    }
    Cat1("==");
    break;

  case OR:
    Cat1("||");
    break;
  case AND:
    Cat1("&&");
    break;
  case LSHIFT:
    Cat1("<<");
    break;
  case RSHIFT:
    Cat1(">>");
    break;

  case RUN:
    fprintf(fd, "run %s(", now->symbol->name.c_str());
    for (v = now->left; v; v = v->right)
      if (v == now->left) {
        comwork(fd, v->left, m);
      } else {
        Cat2(",", v->left);
      }
    fprintf(fd, ")");
    break;

  case LEN:
    putname(fd, "len(", now->left, m, ")");
    break;
  case FULL:
    putname(fd, "full(", now->left, m, ")");
    break;
  case EMPTY:
    putname(fd, "empty(", now->left, m, ")");
    break;
  case NFULL:
    putname(fd, "nfull(", now->left, m, ")");
    break;
  case NEMPTY:
    putname(fd, "nempty(", now->left, m, ")");
    break;

  case 's':

    putname(fd, "", now->left, m, now->value ? now_val_two : now_val_one);
    for (v = now->right, i = 0; v; v = v->right, i++) {
      if (v != now->right)
        fprintf(fd, ",");
      if (!symbolic(fd, v->left))
        comwork(fd, v->left, m);
    }
    break;
  case 'r':
    putname(fd, "", now->left, m, "?");
    switch (now->value) {
    case 0:
      break;
    case 1:
      fprintf(fd, "?");
      break;
    case 2:
      fprintf(fd, "<");
      break;
    case 3:
      fprintf(fd, "?<");
      break;
    }
    for (v = now->right, i = 0; v; v = v->right, i++) {
      if (v != now->right)
        fprintf(fd, ",");
      if (!symbolic(fd, v->left))
        comwork(fd, v->left, m);
    }
    if (now->value >= 2)
      fprintf(fd, ">");
    break;
  case 'R':
    now_val_two = "??[";
    now_val_one = "?[";
    putname(fd, "", now->left, m, now->value ? now_val_two : now_val_one);
    for (v = now->right, i = 0; v; v = v->right, i++) {
      if (v != now->right)
        fprintf(fd, ",");
      if (!symbolic(fd, v->left))
        comwork(fd, v->left, m);
    }
    fprintf(fd, "]");
    break;

  case ENABLED:
    Cat3("enabled(", now->left, ")");
    break;

  case GET_P:
    if (launch_settings.need_revert_old_rultes_for_priority) {
      fprintf(fd, "1");
    } else {
      Cat3("get_priority(", now->left, ")");
    }
    break;

  case SET_P:
    if (!launch_settings.need_revert_old_rultes_for_priority) {
      fprintf(fd, "set_priority(");
      comwork(fd, now->left->left, m);
      fprintf(fd, ", ");
      comwork(fd, now->left->right, m);
      fprintf(fd, ")");
    }
    break;

  case EVAL:
    if (now->left->node_type == ',') {
      Cat3("eval(", now->left->left, ")");
    } else {
      Cat3("eval(", now->left, ")");
    }
    break;

  case NONPROGRESS:
    fprintf(fd, "np_");
    break;

  case PC_VAL:
    Cat3("pc_value(", now->left, ")");
    break;

  case 'c':
    Cat3("(", now->left, ")");
    break;

  case '?':
    if (now->left) {
      Cat3("( (", now->left, ") -> ");
    }
    if (now->right) {
      Cat3("(", now->right->left, ") : ");
      Cat3("(", now->right->right, ") )");
    }
    break;

  case ASGN:
    if (check_track(now) == STRUCT) {
      break;
    }
    comwork(fd, now->left, m);
    fprintf(fd, " = ");
    comwork(fd, now->right, m);
    break;

  case PRINT: {
    char c;
    std::string buf;
    buf = now->symbol->name.substr(0, 510);

    for (i = j = 0; i < 510; i++, j++) {
      c = now->symbol->name[i];
      buf[j] = c;
      if (c == '\\')
        buf[++j] = c;
      if (c == '\"')
        buf[j] = '\'';
      if (c == '\0')
        break;
    }
    if (now->node_type == PRINT)
      fprintf(fd, "printf");
    else
      fprintf(fd, "annotate");
    fprintf(fd, "(%s", buf.c_str());
  }
    for (v = now->left; v; v = v->right) {
      Cat2(",", v->left);
    }
    fprintf(fd, ")");
    break;
  case PRINTM:
    fprintf(fd, "printm(");
    {
      std::string s;
      if (now->left->symbol && now->left->symbol->mtype_name) {
        s = now->left->symbol->mtype_name->name;
      }

      if (now->left && now->left->is_mtype_token) {
        fprintf(fd, "%d", now->left->value);
      } else {
        comwork(fd, now->left, m);
      }

      if (!s.empty()) {
        if (in_settr) {
          fprintf(fd, ", '%s')", s.c_str());
        } else {
          fprintf(fd, ", \"%s\")", s.c_str());
        }
      } else {
        fprintf(fd, ", 0)");
      }
    }
    break;
  case NAME:
    putname(fd, "", now, m, "");
    break;

  case 'p':
    if (lexer_.IsLtlMode()) {
      fprintf(fd, "%s", now->left->symbol->name.c_str()); /* proctype */
      if (now->left->left) {
        fprintf(fd, "[");
        putstmnt(fd, now->left->left, 0); /* pid */
        fprintf(fd, "]");
      }
      fprintf(fd, ":");                          /* remote varref */
      fprintf(fd, "%s", now->symbol->name.c_str()); /* varname */
      break;
    }
    putremote(fd, now, m);
    break;
  case 'q':
    fprintf(fd, "%s", now->symbol->name.c_str());
    break;
  case C_EXPR:
  case C_CODE:
    fprintf(fd, "{%s}", now->symbol->name.c_str());
    break;
  case ASSERT:
    Cat3("assert(", now->left, ")");
    break;
  case '.':
    fprintf(fd, ".(goto)");
    break;
  case GOTO:
    fprintf(fd, "goto %s", now->symbol->name.c_str());
    break;
  case BREAK:
    fprintf(fd, "break");
    break;
  case ELSE:
    fprintf(fd, "else");
    break;
  case '@':
    fprintf(fd, "-end-");
    break;

  case D_STEP:
    fprintf(fd, "D_STEP%d", now->line_number);
    break;
  case ATOMIC:
    fprintf(fd, "ATOMIC");
    break;
  case NON_ATOMIC:
    fprintf(fd, "sub-sequence");
    break;
  case IF:
    fprintf(fd, "IF");
    break;
  case DO:
    fprintf(fd, "DO");
    break;
  case UNLESS:
    fprintf(fd, "unless");
    break;
  case TIMEOUT:
    fprintf(fd, "timeout");
    break;
  default:
    if (isprint(now->node_type))
      fprintf(fd, "'%c'", now->node_type);
    else
      fprintf(fd, "%d", now->node_type);
    break;
  }
}

void comment(FILE *fd, models::Lextok *now, int m) {
  extern short terse, nocast;

  terse = nocast = 1;
  comwork(fd, now, m);
  terse = nocast = 0;
}
