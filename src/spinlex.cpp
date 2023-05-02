/***** spin: spinlex.c *****/

/*
 * This file is part of the public release of Spin. It is subject to the
 * terms in the LICENSE file that is included in this source directory.
 * Tool documentation is available at http://spinroot.com
 */

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
  Symbol *nm;        /* name of the type */
  Lextok *cn;        /* contents */
  Lextok *params;    /* formal pars if any */
  Lextok *rval;      /* variable to assign return value, if any */
  char **anms;       /* literal text for actual pars */
  char *prec;        /* precondition for c_code or c_expr */
  int uiid;          /* unique inline id */
  int is_expr;       /* c_expr in an ltl formula */
  int dln, cln;      /* def and call linenr */
  Symbol *dfn, *cfn; /* def and call filename */
  struct IType *nxt; /* linked list */
};

struct C_Added {
  Symbol *s;
  Symbol *t;
  Symbol *ival;
  Symbol *fnm;
  int lno;
  struct C_Added *nxt;
};

extern RunList *X_lst;
extern ProcList *ready;
extern Symbol *Fname, *oFname;
extern Symbol *context, *owner;
extern YYSTYPE yylval;
extern int need_arguments, hastrack, separate;
extern int implied_semis, in_seq;
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

static void def_inline(Symbol *s, int ln, char *ptr, char *prc, Lextok *nms) {
  IType *tmp;
  int cnt = 0;
  char *nw = (char *)emalloc(strlen(ptr) + 1);
  strcpy(nw, ptr);

  for (tmp = seqnames; tmp; cnt++, tmp = tmp->nxt)
    if (!strcmp(s->name, tmp->nm->name)) {
      log::non_fatal("procedure name %s redefined", tmp->nm->name);
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
        fprintf(fd, "\t{ \"%s\", ", tmp->nm->name);
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

static int iseqname(char *t) {
  IType *tmp;

  for (tmp = seqnames; tmp; tmp = tmp->nxt) {
    if (!strcmp(t, tmp->nm->name))
      return 1;
  }
  return 0;
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
    log::fatal("return statement outside inline");
  }
  return ZN;
}

int is_inline(void) {
  if (Inlining < 0)
    return 0; /* i.e., not an inline */
  if (Inline_stub[Inlining] == NULL)
    log::fatal("unexpected, inline_stub not set");
  return Inline_stub[Inlining]->uiid;
}

IType *find_inline(char *s) {
  IType *tmp;

  for (tmp = seqnames; tmp; tmp = tmp->nxt)
    if (!strcmp(s, tmp->nm->name))
      break;
  if (!tmp)
    log::fatal("cannot happen, missing inline def %s", s);

  return tmp;
}

void c_state(Symbol *s, Symbol *t, Symbol *ival) /* name, scope, ival */
{
  C_Added *r;

  r = (C_Added *)emalloc(sizeof(C_Added));
  r->s = s; /* pointer to a data object */
  r->t = t; /* size of object, or "global", or "local proctype_name"  */
  r->ival = ival;
  r->lno = lineno;
  r->fnm = Fname;
  r->nxt = c_added;

  if (strncmp(r->s->name, "\"unsigned unsigned", 18) == 0) {
    int i;
    for (i = 10; i < 18; i++) {
      r->s->name[i] = ' ';
    }
    /*	printf("corrected <%s>\n", r->s->name);	*/
  }
  c_added = r;
}

void c_track(Symbol *s, Symbol *t, Symbol *stackonly) /* name, size */
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
    if (strcmp(stackonly->name, "\"Matched\"") == 0)
      r->ival = ZS; /* the default */
    else if (strcmp(stackonly->name, "\"UnMatched\"") != 0 &&
             strcmp(stackonly->name, "\"unMatched\"") != 0 &&
             strcmp(stackonly->name, "\"StackOnly\"") != 0)
      log::non_fatal("expecting '[Un]Matched', saw %s", stackonly->name);
    else
      has_stack = 1; /* unmatched stack */
  }
}

char *skip_white(char *p) {
  if (p != NULL) {
    while (*p == ' ' || *p == '\t')
      p++;
  } else {
    log::fatal("bad format - 1");
  }
  return p;
}

char *skip_nonwhite(char *p) {
  if (p != NULL) {
    while (*p != ' ' && *p != '\t')
      p++;
  } else {
    log::fatal("bad format - 2");
  }
  return p;
}

static char *jump_etc(C_Added *r) {
  char *op = r->s->name;
  char *p = op;
  char *q = (char *)0;
  int oln = lineno;
  Symbol *ofnm = Fname;

  /* try to get the type separated from the name */
  lineno = r->lno;
  Fname = r->fnm;

  p = skip_white(p); /* initial white space */

  if (strncmp(p, "enum", strlen("enum")) ==
      0) /* special case: a two-part typename */
  {
    p += strlen("enum") + 1;
    p = skip_white(p);
  }
  if (strncmp(p, "unsigned", strlen("unsigned")) ==
      0) /* possibly a two-part typename */
  {
    p += strlen("unsigned") + 1;
    q = p = skip_white(p);
  }
  p = skip_nonwhite(p); /* type name */
  p = skip_white(p);    /* white space */
  while (*p == '*')
    p++;             /* decorations */
  p = skip_white(p); /* white space */

  if (*p == '\0') {
    if (q) {
      p = q; /* unsigned with implied 'int' */
    } else {
      log::fatal("c_state format (%s)", op);
    }
  }

  if (strchr(p, '[') &&
      (!r->ival || !r->ival->name ||
       !strchr(r->ival->name, '{'))) /* was !strchr(p, '{')) */
  {
    log::non_fatal("array initialization error, c_state (%s)", p);
    p = (char *)0;
  }

  lineno = oln;
  Fname = ofnm;

  return p;
}

void c_add_globinit(FILE *fd) {
  C_Added *r;
  char *p, *q;

  fprintf(fd, "void\nglobinit(void)\n{\n");
  for (r = c_added; r; r = r->nxt) {
    if (r->ival == ZS)
      continue;

    if (strncmp(r->t->name, " Global ", strlen(" Global ")) == 0) {
      for (q = r->ival->name; *q; q++) {
        if (*q == '\"')
          *q = ' ';
        if (*q == '\\')
          *q++ = ' '; /* skip over the next */
      }
      p = jump_etc(r); /* e.g., "int **q" */
      if (p)
        fprintf(fd, "	now.%s = %s;\n", p, r->ival->name);

    } else if (strncmp(r->t->name, " Hidden ", strlen(" Hidden ")) == 0) {
      for (q = r->ival->name; *q; q++) {
        if (*q == '\"')
          *q = ' ';
        if (*q == '\\')
          *q++ = ' '; /* skip over the next */
      }
      p = jump_etc(r); /* e.g., "int **q" */
      if (p)
        fprintf(fd, "	%s = %s;\n", p, r->ival->name); /* no now. prefix */
    }
  }
  fprintf(fd, "}\n");
}

void c_add_locinit(FILE *fd, int tpnr, char *pnm) {
  C_Added *r;
  char *p, *q, *s;
  int frst = 1;

  fprintf(fd, "void\nlocinit%d(int h)\n{\n", tpnr);
  for (r = c_added; r; r = r->nxt)
    if (r->ival != ZS && strncmp(r->t->name, " Local", strlen(" Local")) == 0) {
      for (q = r->ival->name; *q; q++)
        if (*q == '\"')
          *q = ' ';
      p = jump_etc(r); /* e.g., "int **q" */

      q = r->t->name + strlen(" Local");
      while (*q == ' ' || *q == '\t')
        q++; /* process name */

      s = (char *)emalloc(strlen(q) + 1);
      strcpy(s, q);

      q = &s[strlen(s) - 1];
      while (*q == ' ' || *q == '\t')
        *q-- = '\0';

      if (strcmp(pnm, s) != 0)
        continue;

      if (frst) {
        fprintf(fd, "\tuchar *_this = pptr(h);\n");
        frst = 0;
      }

      if (p) {
        fprintf(fd, "\t\t((P%d *)_this)->%s = %s;\n", tpnr, p, r->ival->name);
      }
    }
  fprintf(fd, "}\n");
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
      if (strncmp(r->t->name, " Global ", strlen(" Global ")) != 0 &&
          strncmp(r->t->name, " Hidden ", strlen(" Hidden ")) != 0 &&
          strncmp(r->t->name, " Local", strlen(" Local")) != 0) {
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
    if (strncmp(r->t->name, " Global ", strlen(" Global ")) == 0)
      fprintf(fd, "	%s;\n", r->s->name);

  for (r = c_added; r; r = r->nxt)
    if (strncmp(r->t->name, " Global ", strlen(" Global ")) != 0 &&
        strncmp(r->t->name, " Hidden ", strlen(" Hidden ")) != 0 &&
        strncmp(r->t->name, " Local", strlen(" Local")) != 0) {
      cnt++; /* obsolete use */
    }

  for (r = c_tracked; r; r = r->nxt)
    cnt++; /* preferred use */

  if (cnt == 0)
    return 0;

  cnt = 0;
  fprintf(fd, "	uchar c_state[");
  for (r = c_added; r; r = r->nxt)
    if (strncmp(r->t->name, " Global ", strlen(" Global ")) != 0 &&
        strncmp(r->t->name, " Hidden ", strlen(" Hidden ")) != 0 &&
        strncmp(r->t->name, " Local", strlen(" Local")) != 0) {
      fprintf(fd, "%ssizeof(%s)", (cnt == 0) ? "" : "+", r->t->name);
      cnt++;
    }

  for (r = c_tracked; r; r = r->nxt) {
    if (r->ival != ZS)
      continue;

    fprintf(fd, "%s%s", (cnt == 0) ? "" : "+", r->t->name);
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
      fprintf(fd, "%s%s", (cnt == 0) ? "" : "+", r->t->name);
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

  for (r = c_added; r; r = r->nxt) /* pickup hidden decls */
    if (strncmp(r->t->name, "\"Hidden\"", strlen("\"Hidden\"")) == 0) {
      r->s->name[strlen(r->s->name) - 1] = ' ';
      fprintf(fd, "%s;	/* Hidden */\n", &r->s->name[1]);
      r->s->name[strlen(r->s->name) - 1] = '"';
    }
  /* called before c_add_def - quotes are still there */
}

void c_add_loc(FILE *fd, char *s) /* state vector entries for proctype s */
{
  C_Added *r;
  static char buf[1024];
  char *p;

  if (!c_added)
    return;

  strcpy(buf, s);
  strcat(buf, " ");
  for (r = c_added; r; r = r->nxt) /* pickup local decls */
  {
    if (strncmp(r->t->name, " Local", strlen(" Local")) == 0) {
      p = r->t->name + strlen(" Local");
      fprintf(fd, "/* XXX p=<%s>, s=<%s>, buf=<%s> r->s->name=<%s>XXX */\n", p,
              s, buf, r->s->name);
      while (*p == ' ' || *p == '\t') {
        p++;
      }
      if (strcmp(p, buf) == 0 ||
          (strncmp(p, "init", 4) == 0 && strncmp(buf, ":init:", 6) == 0)) {
        fprintf(fd, "	%s;\n", r->s->name);
      }
    }
  }
}
void c_add_def(FILE *fd) /* 3 - called in plunk_c_fcts() */
{
  C_Added *r;

  fprintf(fd, "#if defined(C_States) && (HAS_TRACK==1)\n");
  for (r = c_added; r; r = r->nxt) {
    r->s->name[strlen(r->s->name) - 1] = ' ';
    r->s->name[0] = ' '; /* remove the "s */

    r->t->name[strlen(r->t->name) - 1] = ' ';
    r->t->name[0] = ' ';

    if (strncmp(r->t->name, " Global ", strlen(" Global ")) == 0 ||
        strncmp(r->t->name, " Hidden ", strlen(" Hidden ")) == 0 ||
        strncmp(r->t->name, " Local", strlen(" Local")) == 0)
      continue;

    if (strchr(r->s->name, '&'))
      log::fatal("dereferencing state object: %s", r->s->name);

    fprintf(fd, "extern %s %s;\n", r->t->name, r->s->name);
  }
  for (r = c_tracked; r; r = r->nxt) {
    r->s->name[strlen(r->s->name) - 1] = ' ';
    r->s->name[0] = ' '; /* remove " */

    r->t->name[strlen(r->t->name) - 1] = ' ';
    r->t->name[0] = ' ';
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

      fprintf(fd, "\tif(%s)\n", r->s->name);
      fprintf(fd, "\t\tmemcpy(p_t_r, %s, %s);\n", r->s->name, r->t->name);
      fprintf(fd, "\telse\n");
      fprintf(fd, "\t\tmemset(p_t_r, 0, %s);\n", r->t->name);
      fprintf(fd, "\tp_t_r += %s;\n", r->t->name);
    }
    fprintf(fd, "}\n\n");
  }

  fprintf(fd, "void\nc_update(uchar *p_t_r)\n{\n");
  fprintf(fd, "#ifdef VERBOSE\n");
  fprintf(fd, "	printf(\"c_update %%p\\n\", p_t_r);\n");
  fprintf(fd, "#endif\n");
  for (r = c_added; r; r = r->nxt) {
    if (strncmp(r->t->name, " Global ", strlen(" Global ")) == 0 ||
        strncmp(r->t->name, " Hidden ", strlen(" Hidden ")) == 0 ||
        strncmp(r->t->name, " Local", strlen(" Local")) == 0)
      continue;

    fprintf(fd, "\tmemcpy(p_t_r, &%s, sizeof(%s));\n", r->s->name, r->t->name);
    fprintf(fd, "\tp_t_r += sizeof(%s);\n", r->t->name);
  }

  for (r = c_tracked; r; r = r->nxt) {
    if (r->ival)
      continue;

    fprintf(fd, "\tif(%s)\n", r->s->name);
    fprintf(fd, "\t\tmemcpy(p_t_r, %s, %s);\n", r->s->name, r->t->name);
    fprintf(fd, "\telse\n");
    fprintf(fd, "\t\tmemset(p_t_r, 0, %s);\n", r->t->name);
    fprintf(fd, "\tp_t_r += %s;\n", r->t->name);
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

      fprintf(fd, "\tif(%s)\n", r->s->name);
      fprintf(fd, "\t\tmemcpy(%s, p_t_r, %s);\n", r->s->name, r->t->name);
      fprintf(fd, "\tp_t_r += %s;\n", r->t->name);
    }
    fprintf(fd, "}\n");
  }

  fprintf(fd, "void\nc_revert(uchar *p_t_r)\n{\n");
  fprintf(fd, "#ifdef VERBOSE\n");
  fprintf(fd, "	printf(\"c_revert %%p\\n\", p_t_r);\n");
  fprintf(fd, "#endif\n");
  for (r = c_added; r; r = r->nxt) {
    if (strncmp(r->t->name, " Global ", strlen(" Global ")) == 0 ||
        strncmp(r->t->name, " Hidden ", strlen(" Hidden ")) == 0 ||
        strncmp(r->t->name, " Local", strlen(" Local")) == 0)
      continue;

    fprintf(fd, "\tmemcpy(&%s, p_t_r, sizeof(%s));\n", r->s->name, r->t->name);
    fprintf(fd, "\tp_t_r += sizeof(%s);\n", r->t->name);
  }
  for (r = c_tracked; r; r = r->nxt) {
    if (r->ival != ZS)
      continue;

    fprintf(fd, "\tif(%s)\n", r->s->name);
    fprintf(fd, "\t\tmemcpy(%s, p_t_r, %s);\n", r->s->name, r->t->name);
    fprintf(fd, "\tp_t_r += %s;\n", r->t->name);
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
    fprintf(fd, "\n/* start of %s */\n", p->nm->name);
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
    fprintf(fd, "\n/* end of %s */\n", p->nm->name);
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
    if (strcmp(p->n->name, X_lst->n->name) == 0)
      continue;
    sprintf(buf, "P%s->", p->n->name);
    if (strstr((char *)tmp->cn, buf)) {
      printf("spin: in proctype %s, ref to object in proctype %s\n",
             X_lst->n->name, p->n->name);
      log::fatal("invalid variable ref in '%s'", tmp->nm->name);
    }
  }
}

extern short terse;
extern short nocast;

void plunk_expr(FILE *fd, char *s) {
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

int glob_inline(char *s) {
  IType *tmp;
  char *bdy;

  tmp = find_inline(s);
  bdy = (char *)tmp->cn;
  return (strstr(bdy, "now.")         /* global ref or   */
          || strchr(bdy, '(') > bdy); /* possible C-function call */
}

char *put_inline(FILE *fd, char *s) {
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

void plunk_inline(FILE *fd, char *s, int how,
                  int gencode) /* c_code with precondition */
{
  IType *tmp;

  tmp = find_inline(s);
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

void no_side_effects(char *s) {
  IType *tmp;
  char *t;
  char *z;

  /* could still defeat this check via hidden
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
    log::non_fatal("c_expr %s has side-effects", s);
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

void pickup_inline(Symbol *t, Lextok *apars, Lextok *rval) {
  IType *tmp;
  Lextok *p, *q;
  int j;

  tmp = find_inline(t->name);

  if (++Inlining >= MAXINL)
    log::fatal("inlines nested too deeply");
  tmp->cln = lineno; /* remember calling point */
  tmp->cfn = Fname;  /* and filename */
  tmp->rval = rval;

  for (p = apars, q = tmp->params, j = 0; p && q; p = p->rgt, q = q->rgt)
    j++; /* count them */
  if (p || q)
    log::fatal("wrong nr of params on call of '%s'", t->name);

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
      log::fatal("cyclic inline attempt on: %s", t->name);
    }
  }
  last_token = SEMI; /* avoid insertion of extra semi */
}

void precondition(char *q) {
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
  log::fatal("cannot happen"); /* unreachable */
}

Symbol *prep_inline(Symbol *s, Lextok *nms) {
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
        log::fatal("bad param to inline %s", s ? s->name : s_s_name);
      }
      t->lft->sym->hidden |= 32;
    }

  if (!s) /* C_Code fragment */
  {
    s = (Symbol *)emalloc(sizeof(Symbol));
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
      log::fatal("bad inline: %s", s->name);
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
    log::fatal("inline text too long");
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
        log::fatal("inline text too long");
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
}