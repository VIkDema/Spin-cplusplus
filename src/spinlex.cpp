/***** spin: spinlex.c *****/

#include "fatal/fatal.hpp"
#include "lexer/lexer.hpp"
#include "spin.hpp"
#include "utils/verbose/verbose.hpp"
#include "y.tab.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <optional>
#include <stdlib.h>
#include <string>

#define MAXINL 16  /* max recursion depth inline fcts */
#define MAXPAR 32  /* max params to an inline call */
#define MAXLEN 512 /* max len of an actual parameter text */

struct IType {
  models::Symbol *nm;        /* name of the type */
  Lextok *cn;                /* contents */
  Lextok *params;            /* formal pars if any */
  Lextok *rval;              /* variable to assign return value, if any */
  char **anms;               /* literal text for actual pars */
  char *prec;                /* precondition for c_code or c_expr */
  int uiid;                  /* unique inline id */
  int is_expr;               /* c_expr in an ltl formula */
  int dln, cln;              /* def and call linenr */
  models::Symbol *dfn, *cfn; /* def and call filename */
  struct IType *nxt;         /* linked list */
};

struct C_Added {
  models::Symbol *s;
  models::Symbol *t;
  models::Symbol *ival;
  models::Symbol *fnm;
  int lno;
  struct C_Added *nxt;
};

extern RunList *X_lst;
extern ProcList *ready;
extern models::Symbol *Fname, *oFname;
extern models::Symbol *context, *owner;
extern YYSTYPE yylval;
extern int need_arguments, hastrack, separate;
extern int implied_semis;
extern lexer::Lexer lexer_;
short has_stack = 0;
int lineno = 1;
int scope_seq[256], scope_level = 0;
char CurScope[MAXSCOPESZ];
std::string yytext;
FILE *yyin, *yyout;

static C_Added *c_added, *c_tracked;
static IType *Inline_stub[MAXINL];
static char *ReDiRect;
static char *Inliner[MAXINL], IArg_cont[MAXPAR][MAXLEN];
static int IArgno = 0, Inlining = -1;
static int last_token = 0;

static int notdollar(int c) { return (c != '$' && c != '\n'); }

static int notquote(int c) { return (c != '\"' && c != '\n'); }

int isalnum_(int c) { return (isalnum(c) || c == '_'); }

static int isalpha_(int c) { return isalpha(c); /* could be macro */ }

static int isdigit_(int c) { return isdigit(c); /* could be macro */ }

static IType *seqnames;

static void def_inline(models::Symbol *s, int ln, char *ptr, char *prc,
                       Lextok *nms) {
  IType *tmp;
  int cnt = 0;
  char *nw = (char *)emalloc(strlen(ptr) + 1);
  strcpy(nw, ptr);

  for (tmp = seqnames; tmp; cnt++, tmp = tmp->nxt)
    if (s->name != tmp->nm->name) {
      loger::non_fatal("procedure name %s redefined", tmp->nm->name);
      tmp->cn = (Lextok *)nw;
      tmp->params = nms;
      tmp->dln = ln;
      tmp->dfn = Fname;
      return;
    }
  tmp = (IType *)emalloc(sizeof(IType));
  tmp->nm = s;
  tmp->cn = (Lextok *)nw;
  tmp->params = nms;
  if (strlen(prc) > 0) {
    tmp->prec = (char *)emalloc(strlen(prc) + 1);
    strcpy(tmp->prec, prc);
  }
  tmp->dln = ln;
  tmp->dfn = Fname;
  tmp->uiid = cnt + 1; /* so that 0 means: not an inline */
  tmp->nxt = seqnames;
  seqnames = tmp;
}

void gencodetable(FILE *fd) {
  IType *tmp;
  char *q;
  int cnt;

  if (separate == 2)
    return;

  fprintf(fd, "struct {\n");
  fprintf(fd, "	char *c; char *t;\n");
  fprintf(fd, "} code_lookup[] = {\n");

  if (lexer_.GetHasCode())
    for (tmp = seqnames; tmp; tmp = tmp->nxt)
      if (tmp->nm->type == CODE_FRAG || tmp->nm->type == CODE_DECL) {
        fprintf(fd, "\t{ \"%s\", ", tmp->nm->name.c_str());
        q = (char *)tmp->cn;

        while (*q == '\n' || *q == '\r' || *q == '\\')
          q++;

        fprintf(fd, "\"");
        cnt = 0;
        while (*q && cnt < 1024) /* pangen1.h allows 2048 */
        {
          switch (*q) {
          case '"':
            fprintf(fd, "\\\"");
            break;
          case '%':
            fprintf(fd, "%%");
            break;
          case '\n':
            fprintf(fd, "\\n");
            break;
          default:
            putc(*q, fd);
            break;
          }
          q++;
          cnt++;
        }
        if (*q)
          fprintf(fd, "...");
        fprintf(fd, "\"");
        fprintf(fd, " },\n");
      }

  fprintf(fd, "	{ (char *) 0, \"\" }\n");
  fprintf(fd, "};\n");
}

bool IsEqname(const std::string &value) {
  IType *tmp;

  for (tmp = seqnames; tmp; tmp = tmp->nxt) {
    if (value == std::string(tmp->nm->name)) {
      return true;
    }
  }
  return false;
}

Lextok *return_statement(Lextok *n) {
  if (Inline_stub[Inlining]->rval) {
    Lextok *g = nn(ZN, NAME, ZN, ZN);
    Lextok *h = Inline_stub[Inlining]->rval;
    g->sym = lookup("rv_");
    return nn(h, ASGN, h, n);
  } else {
    loger::fatal("return statement outside inline");
  }
  return ZN;
}

int is_inline(void) {
  if (Inlining < 0)
    return 0; /* i.e., not an inline */
  if (Inline_stub[Inlining] == NULL)
    loger::fatal("unexpected, inline_stub not set");
  return Inline_stub[Inlining]->uiid;
}

IType *find_inline(const std::string &s) {
  IType *tmp;

  for (tmp = seqnames; tmp; tmp = tmp->nxt)
    if (s == tmp->nm->name) {
      break;
    }
  if (!tmp)
    loger::fatal("cannot happen, missing inline def %s", s);

  return tmp;
}

void c_state(models::Symbol *s, models::Symbol *t,
             models::Symbol *ival) /* name, scope, ival */
{
  C_Added *r;

  r = (C_Added *)emalloc(sizeof(C_Added));
  r->s = s; /* pointer to a data object */
  r->t = t; /* size of object, or "global", or "local proctype_name"  */
  r->ival = ival;
  r->lno = lineno;
  r->fnm = Fname;
  r->nxt = c_added;

  if (strncmp(r->s->name.c_str(), "\"unsigned unsigned", 18) == 0) {
    int i;
    for (i = 10; i < 18; i++) {
      r->s->name[i] = ' ';
    }
    /*	printf("corrected <%s>\n", r->s->name);	*/
  }
  c_added = r;
}

void c_track(models::Symbol *s, models::Symbol *t,
             models::Symbol *stackonly) /* name, size */
{
  C_Added *r;

  r = (C_Added *)emalloc(sizeof(C_Added));
  r->s = s;
  r->t = t;
  r->ival = stackonly; /* abuse of name */
  r->nxt = c_tracked;
  r->fnm = Fname;
  r->lno = lineno;
  c_tracked = r;

  if (stackonly != ZS) {
    if (stackonly->name == "\"Matched\"")
      r->ival = ZS; /* the default */
    else if (stackonly->name != "\"UnMatched\"" &&
             stackonly->name != "\"unMatched\"" &&
             stackonly->name != "\"StackOnly\"")
      loger::non_fatal("expecting '[Un]Matched', saw %s", stackonly->name);
    else
      has_stack = 1; /* unmatched stack */
  }
}

std::string skip_white(const std::string &p) {
  std::string::size_type i = 0;
  while (i < p.length() && (p[i] == ' ' || p[i] == '\t')) {
    i++;
  }
  if (i == p.length()) {
    loger::fatal("bad format - 1");
  }
  return p.substr(i);
}

std::string skip_nonwhite(const std::string &p) {
  std::string::size_type i = 0;
  while (i < p.length() && (p[i] != ' ' && p[i] != '\t')) {
    i++;
  }
  if (i == p.length()) {
    loger::fatal("bad format - 2");
  }
  return p.substr(i);
}

std::string jump_etc(C_Added *r) {
  std::string op = r->s->name;
  std::string p = op;
  std::string q;

  int oln = lineno;
  models::Symbol *ofnm = Fname;

  // Попытка разделить тип от имени
  lineno = r->lno;
  Fname = r->fnm;

  p = skip_white(p);

  if (p.compare(0, 4, "enum") == 0) {
    p = p.substr(4);
    p = skip_white(p);
  }
  if (p.compare(0, 8, "unsigned") == 0) {
    p = p.substr(8);
    q = skip_white(p);
  }
  p = skip_nonwhite(p);
  p = skip_white(p);

  while (!p.empty() && p.front() == '*') {
    p = p.substr(1);
  }

  p = skip_white(p);

  if (p.empty()) {
    if (!q.empty()) {
      p = q;
    } else {
      loger::fatal("c_state format (%s)", op.c_str());
    }
  }

  if (p.find('[') != std::string::npos &&
      (!r->ival || r->ival->name.empty() ||
       r->ival->name.find('{') == std::string::npos)) {
    loger::non_fatal("array initialization error, c_state (%s)", p.c_str());
    p.clear();
  }

  lineno = oln;
  Fname = ofnm;

  return p;
}
void c_add_globinit(FILE *fd) {
  C_Added *r;
  std::string p;

  fprintf(fd, "void\nglobinit(void)\n{\n");
  for (r = c_added; r; r = r->nxt) {
    if (r->ival == ZS)
      continue;

    if (r->t->name.compare(0, strlen(" Global "), " Global ") == 0) {
      std::string::iterator it = r->ival->name.begin();
      while (it != r->ival->name.end()) {
        if (*it == '\"')
          *it = ' ';
        if (*it == '\\') {
          *it = ' ';
          ++it; /* skip over the next */
        } else {
          ++it;
        }
      }
      p = jump_etc(r); /* e.g., "int **q" */
      if (!p.empty())
        fprintf(fd, "  now.%s = %s;\n", p.c_str(), r->ival->name.c_str());

    } else if (r->t->name.compare(0, strlen(" Hidden "), " Hidden ") == 0) {
      std::string::iterator it = r->ival->name.begin();
      while (it != r->ival->name.end()) {
        if (*it == '\"')
          *it = ' ';
        if (*it == '\\') {
          *it = ' ';
          ++it; /* skip over the next */
        } else {
          ++it;
        }
      }
      p = jump_etc(r); /* e.g., "int **q" */
      if (!p.empty())
        fprintf(fd, "  %s = %s;\n", p.c_str(),
                r->ival->name.c_str()); /* no now. prefix */
    }
  }
  fprintf(fd, "}\n");
}

void c_add_locinit(FILE *fd, int tpnr, const std::string &pnm) {
  C_Added *r;
  std::string p, q, s;
  int frst = 1;

  fprintf(fd, "void\nlocinit%d(int h)\n{\n", tpnr);
  for (r = c_added; r; r = r->nxt)
    if (r->ival != ZS &&
        r->t->name.compare(0, strlen(" Local"), " Local") == 0) {
      q = r->ival->name;
      for (char &c : q)
        if (c == '"')
          c = ' ';
      p = jump_etc(r); /* e.g., "int **q" */
      q = r->t->name.substr(strlen(" Local"));
      size_t q_len = q.length();
      size_t last_non_space_index = q_len - 1;
      while (last_non_space_index > 0 && (q[last_non_space_index] == ' ' ||
                                          q[last_non_space_index] == '\t'))
        --last_non_space_index;
      q = q.substr(0, last_non_space_index + 1);

      if (strcmp(pnm.c_str(), q.c_str()) != 0)
        continue;

      if (frst) {
        fprintf(fd, "\tuchar *_this = pptr(h);\n");
        frst = 0;
      }

      if (!p.empty()) {
        fprintf(fd, "\t\t((P%d *)_this)->%s = %s;\n", tpnr, p.c_str(),
                r->ival->name.c_str());
      }
    }
}

/* tracking:
        1. for non-global and non-local c_state decls: add up all the sizes in
   c_added
        2. add a global char array of that size into now
        3. generate a routine that memcpy's the required values into that array
        4. generate a call to that routine
 */

void c_preview(void) {
  C_Added *r;

  hastrack = 0;
  if (c_tracked)
    hastrack = 1;
  else
    for (r = c_added; r; r = r->nxt)
      if (r->t->name.substr(0, 8) != " Global " &&
          r->t->name.substr(0, 8) != " Hidden " &&
          r->t->name.substr(0, 6) != " Local") {
        hastrack = 1; /* c_state variant now obsolete */
        break;
      }
}

int c_add_sv(FILE *fd) /* 1+2 -- called in pangen1.c */
{
  C_Added *r;
  int cnt = 0;

  if (!c_added && !c_tracked)
    return 0;

  for (r = c_added; r; r = r->nxt) /* pickup global decls */
    if (r->t->name.substr(0, 8) == " Global ")
      fprintf(fd, "	%s;\n", r->s->name.c_str());

  for (r = c_added; r; r = r->nxt)
    if (r->t->name.substr(0, 8) != " Global " &&
        r->t->name.substr(0, 8) != " Hidden " &&
        r->t->name.substr(0, 6) != " Local") {
      cnt++; /* obsolete use */
    }

  for (r = c_tracked; r; r = r->nxt)
    cnt++; /* preferred use */

  if (cnt == 0)
    return 0;

  cnt = 0;
  fprintf(fd, "	uchar c_state[");
  for (r = c_added; r; r = r->nxt)
    if (r->t->name.substr(0, 8) != " Global " &&
        r->t->name.substr(0, 8) != " Hidden " &&
        r->t->name.substr(0, 6) != " Local") {
      fprintf(fd, "%ssizeof(%s)", (cnt == 0) ? "" : "+", r->t->name.c_str());
      cnt++;
    }

  for (r = c_tracked; r; r = r->nxt) {
    if (r->ival != ZS)
      continue;

    fprintf(fd, "%s%s", (cnt == 0) ? "" : "+", r->t->name.c_str());
    cnt++;
  }

  if (cnt == 0)
    fprintf(fd, "4"); /* now redundant */
  fprintf(fd, "];\n");
  return 1;
}

void c_stack_size(FILE *fd) {
  C_Added *r;
  int cnt = 0;

  for (r = c_tracked; r; r = r->nxt)
    if (r->ival != ZS) {
      fprintf(fd, "%s%s", (cnt == 0) ? "" : "+", r->t->name.c_str());
      cnt++;
    }
  if (cnt == 0) {
    fprintf(fd, "WS");
  }
}

void c_add_stack(FILE *fd) {
  C_Added *r;
  int cnt = 0;

  if ((!c_added && !c_tracked) || !has_stack) {
    return;
  }

  for (r = c_tracked; r; r = r->nxt)
    if (r->ival != ZS) {
      cnt++;
    }

  if (cnt > 0) {
    fprintf(fd, "	uchar c_stack[StackSize];\n");
  }
}

void c_add_hidden(FILE *fd) {
  C_Added *r;

  for (r = c_added; r; r = r->nxt) /* pickup hidden_flags decls */
    if (r->t->name.compare(0, 6, "Hidden") == 0) {
      r->s->name.back() = ' ';
      fprintf(fd, "%s;	/* Hidden */\n", r->s->name.substr(1).c_str());
      r->s->name.back() = '"';
    }
  /* called before c_add_def - quotes are still there */
}

void c_add_loc(FILE *fd, const std::string &s) {
  C_Added *r;
  static char buf[1024];
  const char *p;

  if (!c_added)
    return;

  strcpy(buf, s.c_str());
  strcat(buf, " ");
  for (r = c_added; r; r = r->nxt) {
    if (strncmp(r->t->name.c_str(), " Local", strlen(" Local")) == 0) {
      p = r->t->name.c_str() + strlen(" Local");
      fprintf(fd, "/* XXX p=<%s>, s=<%s>, buf=<%s> r->s->name=<%s>XXX */\n", p,
              s.c_str(), buf, r->s->name.c_str());
      while (*p == ' ' || *p == '\t') {
        p++;
      }
      if (strcmp(p, buf) == 0 ||
          (strncmp(p, "init", 4) == 0 && strncmp(buf, ":init:", 6) == 0)) {
        fprintf(fd, "    %s;\n", r->s->name.c_str());
      }
    }
  }
}

void c_add_def(FILE *fd) /* 3 - called in plunk_c_fcts() */
{
  C_Added *r;

  fprintf(fd, "#if defined(C_States) && (HAS_TRACK==1)\n");
  for (r = c_added; r; r = r->nxt) {
    r->s->name.back() = ' ';
    r->s->name.front() = ' ';

    r->t->name.back() = ' ';
    r->t->name.front() = ' ';

    if (r->t->name.substr(0, 8) == " Global " &&
        r->t->name.substr(0, 8) == " Hidden " &&
        r->t->name.substr(0, 6) == " Local")
      continue;

    if (r->s->name.find('&') != std::string::npos)
      loger::fatal("dereferencing state object: %s", r->s->name);

    fprintf(fd, "extern %s %s;\n", r->t->name.c_str(), r->s->name.c_str());
  }
  for (r = c_tracked; r; r = r->nxt) {
    r->s->name.back() = ' ';
    r->s->name.front() = ' ';

    r->t->name.back() = ' ';
    r->t->name.front() = ' ';
  }

  if (separate == 2) {
    fprintf(fd, "#endif\n");
    return;
  }

  if (has_stack) {
    fprintf(fd, "int cpu_printf(const char *, ...);\n");
    fprintf(fd, "void\nc_stack(uchar *p_t_r)\n{\n");
    fprintf(fd, "#ifdef VERBOSE\n");
    fprintf(fd, "	cpu_printf(\"c_stack %%u\\n\", p_t_r);\n");
    fprintf(fd, "#endif\n");
    for (r = c_tracked; r; r = r->nxt) {
      if (r->ival == ZS)
        continue;

      fprintf(fd, "\tif(%s)\n", r->s->name.c_str());
      fprintf(fd, "\t\tmemcpy(p_t_r, %s, %s);\n", r->s->name.c_str(),
              r->t->name.c_str());
      fprintf(fd, "\telse\n");
      fprintf(fd, "\t\tmemset(p_t_r, 0, %s);\n", r->t->name.c_str());
      fprintf(fd, "\tp_t_r += %s;\n", r->t->name.c_str());
    }
    fprintf(fd, "}\n\n");
  }

  fprintf(fd, "void\nc_update(uchar *p_t_r)\n{\n");
  fprintf(fd, "#ifdef VERBOSE\n");
  fprintf(fd, "	printf(\"c_update %%p\\n\", p_t_r);\n");
  fprintf(fd, "#endif\n");
  for (r = c_added; r; r = r->nxt) {
    if (r->t->name.substr(0, 8) == " Global " &&
        r->t->name.substr(0, 8) == " Hidden " &&
        r->t->name.substr(0, 6) == " Local")
      continue;

    fprintf(fd, "\tmemcpy(p_t_r, &%s, sizeof(%s));\n", r->s->name.c_str(),
            r->t->name.c_str());
    fprintf(fd, "\tp_t_r += sizeof(%s);\n", r->t->name.c_str());
  }

  for (r = c_tracked; r; r = r->nxt) {
    if (r->ival)
      continue;

    fprintf(fd, "\tif(%s)\n", r->s->name.c_str());
    fprintf(fd, "\t\tmemcpy(p_t_r, %s, %s);\n", r->s->name.c_str(),
            r->t->name.c_str());
    fprintf(fd, "\telse\n");
    fprintf(fd, "\t\tmemset(p_t_r, 0, %s);\n", r->t->name.c_str());
    fprintf(fd, "\tp_t_r += %s;\n", r->t->name.c_str());
  }

  fprintf(fd, "}\n");

  if (has_stack) {
    fprintf(fd, "void\nc_unstack(uchar *p_t_r)\n{\n");
    fprintf(fd, "#ifdef VERBOSE\n");
    fprintf(fd, "	cpu_printf(\"c_unstack %%u\\n\", p_t_r);\n");
    fprintf(fd, "#endif\n");
    for (r = c_tracked; r; r = r->nxt) {
      if (r->ival == ZS)
        continue;

      fprintf(fd, "\tif(%s)\n", r->s->name.c_str());
      fprintf(fd, "\t\tmemcpy(%s, p_t_r, %s);\n", r->s->name.c_str(),
              r->t->name.c_str());
      fprintf(fd, "\tp_t_r += %s;\n", r->t->name.c_str());
    }
    fprintf(fd, "}\n");
  }

  fprintf(fd, "void\nc_revert(uchar *p_t_r)\n{\n");
  fprintf(fd, "#ifdef VERBOSE\n");
  fprintf(fd, "	printf(\"c_revert %%p\\n\", p_t_r);\n");
  fprintf(fd, "#endif\n");
  for (r = c_added; r; r = r->nxt) {
    if (r->t->name.substr(0, 8) == " Global " &&
        r->t->name.substr(0, 8) == " Hidden " &&
        r->t->name.substr(0, 6) == " Local")
      continue;

    fprintf(fd, "\tmemcpy(&%s, p_t_r, sizeof(%s));\n", r->s->name.c_str(),
            r->t->name.c_str());
    fprintf(fd, "\tp_t_r += sizeof(%s);\n", r->t->name.c_str());
  }
  for (r = c_tracked; r; r = r->nxt) {
    if (r->ival != ZS)
      continue;

    fprintf(fd, "\tif(%s)\n", r->s->name.c_str());
    fprintf(fd, "\t\tmemcpy(%s, p_t_r, %s);\n", r->s->name.c_str(),
            r->t->name.c_str());
    fprintf(fd, "\tp_t_r += %s;\n", r->t->name.c_str());
  }

  fprintf(fd, "}\n");
  fprintf(fd, "#endif\n");
}

void plunk_reverse(FILE *fd, IType *p, int matchthis) {
  char *y, *z;

  if (!p)
    return;
  plunk_reverse(fd, p->nxt, matchthis);

  if (!p->nm->context && p->nm->type == matchthis && p->is_expr == 0) {
    fprintf(fd, "\n/* start of %s */\n", p->nm->name.c_str());
    z = (char *)p->cn;
    while (*z == '\n' || *z == '\r' || *z == '\\') {
      z++;
    }
    /* e.g.: \#include "..." */

    y = z;
    while ((y = strstr(y, "\\#")) != NULL) {
      *y = '\n';
      y++;
    }

    fprintf(fd, "%s\n", z);
    fprintf(fd, "\n/* end of %s */\n", p->nm->name.c_str());
  }
}

void plunk_c_decls(FILE *fd) { plunk_reverse(fd, seqnames, CODE_DECL); }

void plunk_c_fcts(FILE *fd) {
  if (separate == 2 && hastrack) {
    c_add_def(fd);
    return;
  }

  c_add_hidden(fd);
  plunk_reverse(fd, seqnames, CODE_FRAG);

  if (c_added || c_tracked) /* enables calls to c_revert and c_update */
    fprintf(fd, "#define C_States	1\n");
  else
    fprintf(fd, "#undef C_States\n");

  if (hastrack)
    c_add_def(fd);

  c_add_globinit(fd);
  do_locinits(fd);
}

static void check_inline(IType *tmp) {
  char buf[128];
  ProcList *p;

  if (!X_lst)
    return;

  for (p = ready; p; p = p->nxt) {
    if (p->n->name == X_lst->n->name) {
      continue;
    }
    sprintf(buf, "P%s->", p->n->name.c_str());
    if (strstr((char *)tmp->cn, buf)) {
      printf("spin: in proctype %s, ref to object in proctype %s\n",
             X_lst->n->name.c_str(), p->n->name.c_str());
      loger::fatal("invalid variable ref in '%s'", tmp->nm->name);
    }
  }
}

extern short terse;
extern short nocast;

void plunk_expr(FILE *fd, const std::string &s) {
  IType *tmp;
  char *q;

  tmp = find_inline(s);
  check_inline(tmp);

  if (terse && nocast) {
    for (q = (char *)tmp->cn; q && *q != '\0'; q++) {
      fflush(fd);
      if (*q == '"') {
        fprintf(fd, "\\");
      }
      fprintf(fd, "%c", *q);
    }
  } else {
    fprintf(fd, "%s", (char *)tmp->cn);
  }
}

void preruse(FILE *fd,
             Lextok *n) /* check a condition for c_expr with preconditions */
{
  IType *tmp;

  if (!n)
    return;
  if (n->ntyp == C_EXPR) {
    tmp = find_inline(n->sym->name);
    if (tmp->prec) {
      fprintf(fd, "if (!(%s)) { if (!readtrail) { depth++; ", tmp->prec);
      fprintf(fd, "trpt++; trpt->pr = II; trpt->o_t = t; trpt->st = tt; ");
      fprintf(fd,
              "uerror(\"c_expr line %d precondition false: %s\"); continue;",
              tmp->dln, tmp->prec);
      fprintf(fd, " } else { printf(\"pan: precondition false: %s\\n\"); ",
              tmp->prec);
      fprintf(fd, "_m = 3; goto P999; } } \n\t\t");
    }
  } else {
    preruse(fd, n->rgt);
    preruse(fd, n->lft);
  }
}

int glob_inline(const std::string &s) {
  IType *tmp;
  char *bdy;

  tmp = find_inline(s);
  bdy = (char *)tmp->cn;
  return (strstr(bdy, "now.")         /* global ref or   */
          || strchr(bdy, '(') > bdy); /* possible C-function call */
}

char *put_inline(FILE *fd, const std::string& s) {
  IType *tmp;

  tmp = find_inline(s);
  check_inline(tmp);
  return (char *)tmp->cn;
}

void mark_last(void) {
  if (seqnames) {
    seqnames->is_expr = 1;
  }
}

void plunk_inline(FILE *fd, const std::string &s, int how,
                  int gencode) /* c_code with precondition */
{
  IType *tmp;

  tmp = find_inline(s.c_str());
  check_inline(tmp);

  fprintf(fd, "{ ");
  if (how && tmp->prec) {
    fprintf(fd, "if (!(%s)) { if (!readtrail) {", tmp->prec);
    fprintf(fd,
            " uerror(\"c_code line %d precondition false: %s\"); continue; ",
            tmp->dln, tmp->prec);
    fprintf(fd, "} else { ");
    fprintf(
        fd,
        "printf(\"pan: precondition false: %s\\n\"); _m = 3; goto P999; } } ",
        tmp->prec);
  }

  if (!gencode) /* not in d_step */
  {
    fprintf(fd, "\n\t\tsv_save();");
  }

  fprintf(fd, "%s", (char *)tmp->cn);
  fprintf(fd, " }\n");
}

int side_scan(char *t, char *pat) {
  char *r = strstr(t, pat);
  return (r && *(r - 1) != '"' && *(r - 1) != '\'');
}

void no_side_effects(const std::string& s) {
  IType *tmp;
  char *t;
  char *z;

  /* could still defeat this check via hidden_flags
   * side effects in function calls,
   * but this will catch at least some cases
   */

  tmp = find_inline(s);
  t = (char *)tmp->cn;
  while (t && *t == ' ') {
    t++;
  }

  z = strchr(t, '(');
  if (z && z > t && isalnum((int)*(z - 1)) &&
      strncmp(t, "spin_mutex_free(", strlen("spin_mutex_free(")) != 0) {
    goto bad; /* fct call */
  }

  if (side_scan(t, ";") || side_scan(t, "++") || side_scan(t, "--")) {
  bad:
    lineno = tmp->dln;
    Fname = tmp->dfn;
    loger::non_fatal("c_expr %s has side-effects", s);
    return;
  }
  while ((t = strchr(t, '=')) != NULL) {
    if (*(t - 1) == '!' || *(t - 1) == '>' || *(t - 1) == '<' ||
        *(t - 1) == '"' || *(t - 1) == '\'') {
      t += 2;
      continue;
    }
    t++;
    if (*t != '=')
      goto bad;
    t++;
  }
}

void pickup_inline(models::Symbol *t, Lextok *apars, Lextok *rval) {
  IType *tmp;
  Lextok *p, *q;
  int j;

  tmp = find_inline(t->name);

  if (++Inlining >= MAXINL)
    loger::fatal("inlines nested too deeply");
  tmp->cln = lineno; /* remember calling point */
  tmp->cfn = Fname;  /* and filename */
  tmp->rval = rval;

  for (p = apars, q = tmp->params, j = 0; p && q; p = p->rgt, q = q->rgt)
    j++; /* count them */
  if (p || q)
    loger::fatal("wrong nr of params on call of '%s'", t->name);

  tmp->anms = (char **)emalloc(j * sizeof(char *));
  for (p = apars, j = 0; p; p = p->rgt, j++) {
    tmp->anms[j] = (char *)emalloc(strlen(IArg_cont[j]) + 1);
    strcpy(tmp->anms[j], IArg_cont[j]);
  }

  lineno = tmp->dln; /* linenr of def */
  Fname = tmp->dfn;  /* filename of same */
  Inliner[Inlining] = (char *)tmp->cn;
  Inline_stub[Inlining] = tmp;
  for (j = 0; j < Inlining; j++) {
    if (Inline_stub[j] == Inline_stub[Inlining]) {
      loger::fatal("cyclic inline attempt on: %s", t->name);
    }
  }
  last_token = SEMI; /* avoid insertion of extra semi */
}

void precondition(char *q) {
#if 0
  int c, nest = 1;

  for (;;) {
    c = Getchar();
    *q++ = c;
    switch (c) {
    case '\n':
      lineno++;
      break;
    case '[':
      nest++;
      break;
    case ']':
      if (--nest <= 0) {
        *--q = '\0';
        return;
      }
      break;
    }
  }
  loger::fatal("cannot happen"); /* unreachable */
#endif
}

models::Symbol *prep_inline(models::Symbol *s, Lextok *nms) {
  return nullptr;
#if 0

  int c, nest = 1, dln, firstchar, cnr;
  char *p;
  Lextok *t;
  static char Buf1[SOMETHINGBIG], Buf2[RATHERSMALL];
  static int c_code = 1;
  auto &verbose_flags = utils::verbose::Flags::getInstance();
  for (t = nms; t; t = t->rgt)
    if (t->lft) {
      if (t->lft->ntyp != NAME) {
        char *s_s_name = "--";
        loger::fatal("bad param to inline %s", s ? s->name : s_s_name);
      }
      t->lft->sym->hidden_flags |= 32;
    }

  if (!s) /* C_Code fragment */
  {
    s = (models::Symbol *)emalloc(sizeof(models::Symbol));
    s->name = (char *)emalloc(strlen("c_code") + 26);
    sprintf(s->name, "c_code%d", c_code++);
    s->context = context;
    s->type = CODE_FRAG;
  } else {
    s->type = PREDEF;
  }

  p = &Buf1[0];
  Buf2[0] = '\0';
  for (;;) {
    c = Getchar();
    switch (c) {
    case '[':
      if (s->type != CODE_FRAG)
        goto bad;
      precondition(&Buf2[0]); /* e.g., c_code [p] { r = p-r; } */
      continue;
    case '{':
      break;
    case '\n':
      lineno++;
      /* fall through */
    case ' ':
    case '\t':
    case '\f':
    case '\r':
      continue;
    default:
      printf("spin: saw char '%c'\n", c);
    bad:
      loger::fatal("bad inline: %s", s->name);
    }
    break;
  }
  dln = lineno;
  if (s->type == CODE_FRAG) {
    if (verbose_flags.NeedToPrintVerbose()) {
      sprintf(Buf1, "\t/* line %d %s */\n\t\t", lineno, Fname->name);
    } else {
      strcpy(Buf1, "");
    }
  } else {
    sprintf(Buf1, "\n#line %d \"%s\"\n{", lineno, Fname->name);
  }
  p += strlen(Buf1);
  firstchar = 1;

  cnr = 1; /* not zero */
more:
  c = Getchar();
  *p++ = (char)c;
  if (p - Buf1 >= SOMETHINGBIG)
    loger::fatal("inline text too long");
  switch (c) {
  case '\n':
    lineno++;
    cnr = 0;
    break;
  case '{':
    cnr++;
    nest++;
    break;
  case '}':
    cnr++;
    if (--nest <= 0) {
      *p = '\0';
      if (s->type == CODE_FRAG) {
        *--p = '\0'; /* remove trailing '}' */
      }
      def_inline(s, dln, &Buf1[0], &Buf2[0], nms);
      if (firstchar) {
        printf("%3d: %s, warning: empty inline definition (%s)\n", dln,
               Fname->name, s->name);
      }
      return s; /* normal return */
    }
    break;
  case '#':
    if (cnr == 0) {
      p--;
      do_directive(c); /* reads to newline */
    } else {
      firstchar = 0;
      cnr++;
    }
    break;
  case '\t':
  case ' ':
  case '\f':
    cnr++;
    break;
  case '"':
    do {
      c = Getchar();
      *p++ = (char)c;
      if (c == '\\') {
        *p++ = (char)Getchar();
      }
      if (p - Buf1 >= SOMETHINGBIG) {
        loger::fatal("inline text too long");
      }
    } while (c != '"'); /* end of string */
    /* *p = '\0'; */
    break;
  case '\'':
    c = Getchar();
    *p++ = (char)c;
    if (c == '\\') {
      *p++ = (char)Getchar();
    }
    c = Getchar();
    *p++ = (char)c;
    assert(c == '\'');
    break;
  default:
    firstchar = 0;
    cnr++;
    break;
  }
  goto more;
#endif
}