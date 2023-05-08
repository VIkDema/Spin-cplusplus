#ifndef SEEN_SPIN_H
#define SEEN_SPIN_H

#include "models/lextok.hpp"
#include "models/symbol.hpp"
#include "models/models_fwd.hpp"
#include <ctype.h>
#include <optional>
#include <stdio.h>
#include <string.h>

#if !defined(WIN32) && !defined(WIN64)
#include <unistd.h>
#endif
#ifndef PC
#include <memory.h>
#endif

struct Slicer {
  models::Lextok *n;  /* global var, usable as slice criterion */
  short code;         /* type of use: DEREF_USE or normal USE */
  short used;         /* set when handled */
  struct Slicer *nxt; /* linked list */
};

struct Access {
  models::Symbol *who;  /* proctype name of accessor */
  models::Symbol *what; /* proctype name of accessed */
  int cnt, typ;         /* parameter nr and, e.g., 's' or 'r' */
  struct Access *lnk;   /* linked list */
};

struct Ordered { /* links all names in Symbol table */
  models::Symbol *entry;
  struct Ordered *next;
};

struct Mtypes_t {
  std::string nm;       /* name of mtype, or "_unnamed_" */
  models::Lextok *mt;   /* the linked list of names */
  struct Mtypes_t *nxt; /* linked list of mtypes */
};

struct Queue {
  short qid;         /* runtime q index */
  int qlen;          /* nr messages stored */
  int nslots, nflds; /* capacity, flds/slot */
  int setat;         /* last depth value changed */
  int *fld_width;    /* type of each field */
  int *contents;     /* the values stored */
  int *stepnr;       /* depth when each msg was sent */
  char **mtp;        /* if mtype, name of list, else 0 */
  struct Queue *nxt; /* linked list */
};

struct FSM_state { /* used in pangen5.c - dataflow */
  int from;        /* state number */
  int seen;        /* used for dfs */
  int in;          /* nr of incoming edges */
  int cr;          /* has reachable 1-relevant successor */
  int scratch;
  unsigned long *dom, *mod; /* to mark dominant nodes */
  struct FSM_trans *t;      /* outgoing edges */
  struct FSM_trans *p;      /* incoming edges, predecessors */
  struct FSM_state *nxt;    /* linked list of all states */
};

struct FSM_trans { /* used in pangen5.c - dataflow */
  int to;
  short relevant;         /* when sliced */
  short round;            /* ditto: iteration when marked */
  struct FSM_use *Val[2]; /* 0=reads, 1=writes */
  struct Element *step;
  struct FSM_trans *nxt;
};

struct FSM_use { /* used in pangen5.c - dataflow */
  models::Lextok *n;
  models::Symbol *var;
  int special;
  struct FSM_use *nxt;
};

struct Element {
  models::Lextok *n; /* defines the type & contents */
  int Seqno;         /* identifies this el within system */
  int seqno;         /* identifies this el within a proc */
  int merge;         /* set by -O if step can be merged */
  int merge_start;
  int merge_single;
  short merge_in;       /* nr of incoming edges */
  short merge_mark;     /* state was generated in merge sequence */
  unsigned int status;  /* used by analyzer generator  */
  struct FSM_use *dead; /* optional dead variable list */
  struct SeqList *sub;  /* subsequences, for compounds */
  struct SeqList *esc;  /* zero or more escape sequences */
  struct Element *Nxt;  /* linked list - for global lookup */
  struct Element *nxt;  /* linked list - program structure */
};

struct Sequence {
  Element *frst;
  Element *last;   /* links onto continuations */
  Element *extent; /* last element in original */
  int minel;       /* minimum Seqno, set and used only in guided.c */
  int maxel;       /* 1+largest id in sequence */
};

struct SeqList {
  struct Sequence *this_sequence; /* one sequence */
  struct SeqList *nxt;            /* linked list  */
};

struct Label {
  models::Symbol *s;
  models::Symbol *c;
  Element *e;
  int opt_inline_id; /* non-zero if label appears in an inline */
  int visible;       /* label referenced in claim (slice relevant) */
  struct Label *nxt;
};

struct Lbreak {
  models::Symbol *l;
  struct Lbreak *nxt;
};

struct L_List {
  models::Lextok *n;
  struct L_List *nxt;
};

struct RunList {
  models::Symbol *n;      /* name            */
  int tn;                 /* ordinal of type */
  int pid;                /* process id      */
  int priority;           /* for simulations only */
  models::btypes b;          /* the type of process */
  Element *pc;            /* current stmnt   */
  struct Sequence *ps;    /* used by analyzer generator */
  models::Lextok *prov;   /* provided clause */
  models::Symbol *symtab; /* local variables */
  struct RunList *nxt;    /* linked list */
};

struct ProcList {
  models::Symbol *n;      /* name       */
  models::Lextok *p;      /* parameters */
  Sequence *s;            /* body       */
  models::Lextok *prov;   /* provided clause */
  models::btypes b;          /* e.g., claim, trace, proc */
  short tn;               /* ordinal number */
  unsigned char det;      /* deterministic */
  unsigned char unsafe;   /* contains global var inits */
  unsigned char priority; /* process priority, if any */
  struct ProcList *nxt;   /* linked list */
};

struct QH {
  int n;
  struct QH *nxt;
};

typedef models::Lextok *Lexptr;

#define YYSTYPE Lexptr

#define ZN (models::Lextok *)0
#define ZS (models::Symbol *)0
#define ZE (Element *)0

#define DONE 1      /* status bits of elements */
#define ATOM 2      /* part of an atomic chain */
#define L_ATOM 4    /* last element in a chain */
#define I_GLOB 8    /* inherited global ref    */
#define DONE2 16    /* used in putcode and main*/
#define D_ATOM 32   /* deterministic atomic    */
#define ENDSTATE 64 /* normal endstate         */
#define CHECK2 128  /* status bits for remote ref check */
#define CHECK3 256  /* status bits for atomic jump check */

#define Nhash 255 /* slots in symbol hash-table */

#define XR 1 /* non-shared receive-only */
#define XS 2 /* non-shared send-only    */
#define XX 4 /* overrides XR or XS tag  */

#define CODE_FRAG 2 /* auto-numbered code-fragment */
#define CODE_DECL 4 /* auto-numbered c_decl */
#define PREDEF 3    /* predefined name: _p, _last */

#define UNSIGNED 5 /* val defines width in bits */
#define BIT 1      /* also equal to width in bits */
#define BYTE 8     /* ditto */
#define SHORT 16   /* ditto */
#define INT 32     /* ditto */
#define CHAN 64    /* not */
#define STRUCT 128 /* user defined structure name */

#define SOMETHINGBIG 65536
#define RATHERSMALL 512
#define MAXSCOPESZ 1024

#ifndef max
#define max(a, b) (((a) < (b)) ? (b) : (a))
#endif

#ifdef PC
#define MFLAGS "wb"
#else
#define MFLAGS "w"
#endif

/** NEW**/
int ismtype(const std::string &);
bool IsProctype(const std::string &value);
bool IsEqname(const std::string &value);
bool IsUtype(const std::string &value);
/***** prototype definitions *****/
Element *eval_sub(Element *);
Element *get_lab(models::Lextok *, int);
Element *huntele(Element *, unsigned int, int);
Element *huntstart(Element *);
Element *mk_skip(void);
Element *target(Element *);

models::Lextok *do_unless(models::Lextok *, models::Lextok *);
models::Lextok *expand(models::Lextok *, int);
models::Lextok *getuname(models::Symbol *);
models::Lextok *mk_explicit(models::Lextok *, int, int);
models::Lextok *nn(models::Lextok *, int, models::Lextok *, models::Lextok *);
models::Lextok *rem_lab(models::Symbol *, models::Lextok *, models::Symbol *);
models::Lextok *rem_var(models::Symbol *, models::Lextok *, models::Symbol *,
                        models::Lextok *);
models::Lextok *tail_add(models::Lextok *, models::Lextok *);
models::Lextok *return_statement(models::Lextok *);

ProcList *mk_rdy(models::Symbol *, models::Lextok *, Sequence *, int,
                 models::Lextok *, models::btypes);

SeqList *seqlist(Sequence *, SeqList *);
Sequence *close_seq(int);

models::Symbol *break_dest(void);
models::Symbol *findloc(models::Symbol *);
models::Symbol *has_lab(Element *, int);
models::Symbol *lookup(const std::string &s);
models::Symbol *prep_inline(models::Symbol *, models::Lextok *);

char *put_inline(FILE *, const std::string &);
char *emalloc(size_t);
char *erealloc(void *, size_t, size_t);
long Rand(void);

int any_oper(models::Lextok *, int);
int any_undo(models::Lextok *);
int c_add_sv(FILE *);
int cast_val(int, int, int);
int checkvar(models::Symbol *, int);
int check_track(models::Lextok *);
int Cnt_flds(models::Lextok *);
int cnt_mpars(models::Lextok *);
int complete_rendez(void);
int enable(models::Lextok *);
int Enabled0(Element *);
int eval(models::Lextok *);
int find_lab(models::Symbol *, models::Symbol *, int);
int find_maxel(models::Symbol *);
int full_name(FILE *, models::Lextok *, models::Symbol *, int);
int getlocal(models::Lextok *);
int getval(models::Lextok *);
int glob_inline(const std::string &);
int has_typ(models::Lextok *, int);
int in_bound(models::Symbol *, int);
int interprint(FILE *, models::Lextok *);
int printm(FILE *, models::Lextok *);
int is_inline(void);
int ismtype(char *);

int Lval_struct(models::Lextok *, models::Symbol *, int, int);
int main(int, char **);
int pc_value(models::Lextok *);
int pid_is_claim(int);
int proper_enabler(models::Lextok *);
int putcode(FILE *, Sequence *, Element *, int, int, int);
int q_is_sync(models::Lextok *);
int qlen(models::Lextok *);
int qfull(models::Lextok *);
int qmake(models::Symbol *);
int qrecv(models::Lextok *, int);
int qsend(models::Lextok *);
int remotelab(models::Lextok *);
int remotevar(models::Lextok *);
int Rval_struct(models::Lextok *, models::Symbol *, int);
int setlocal(models::Lextok *, int);
int setval(models::Lextok *, int);
int sputtype(std::string &, int);
int Sym_typ(models::Lextok *);
int tl_main(int, char *[]);
int Width_set(int *, int, models::Lextok *);
int yyparse(void);

void AST_track(models::Lextok *, int);
void add_seq(models::Lextok *);
void announce(char *);
void c_state(models::Symbol *, models::Symbol *, models::Symbol *);
void c_add_def(FILE *);
void c_add_loc(FILE *, const std::string &);
void c_add_locinit(FILE *, int, const std::string &);
void c_chandump(FILE *);
void c_preview(void);
void c_struct(FILE *, const std::string &, models::Symbol *);
void c_track(models::Symbol *, models::Symbol *, models::Symbol *);
void c_var(FILE *, const std::string &, models::Symbol *);
void c_wrapper(FILE *);
void chanaccess(void);
void check_param_count(int, models::Lextok *);
void checkrun(models::Symbol *, int);
void comment(FILE *, models::Lextok *, int);
void cross_dsteps(models::Lextok *, models::Lextok *);
void disambiguate(void);
void doq(models::Symbol *, int, RunList *);
void dotag(FILE *, char *);
void do_locinits(FILE *);
void do_var(FILE *, int, const std::string &, models::Symbol *,
            const std::string &, const std::string &, const std::string &);
void dump_struct(models::Symbol *, const std::string &, RunList *);
void dumpclaims(FILE *, int, const std::string &);
void dumpglobals(void);
void dumplabels(void);
void dumplocal(RunList *, int);
void dumpsrc(int, int);
void fix_dest(models::Symbol *, models::Symbol *);
void genaddproc(void);
void genaddqueue(void);
void gencodetable(FILE *);
void genheader(void);
void genother(void);
void gensrc(void);
void gensvmap(void);
void genunio(void);
void ini_struct(models::Symbol *);
void loose_ends(void);
void make_atomic(Sequence *, int);
void mark_last(void);
void match_trail(void);
void no_side_effects(const std::string &);
void nochan_manip(models::Lextok *, models::Lextok *, int);
void ntimes(FILE *, int, int, const char *c[]);
void open_seq(int);
void p_talk(Element *, int);
void pickup_inline(models::Symbol *, models::Lextok *, models::Lextok *);
void plunk_c_decls(FILE *);
void plunk_c_fcts(FILE *);
void plunk_expr(FILE *, const std::string &);
void plunk_inline(FILE *, const std::string &, int, int);
void prehint(models::Symbol *);
void preruse(FILE *, models::Lextok *);
void prune_opts(models::Lextok *);
void pstext(int, char *);
void pushbreak(void);
void putname(FILE *, const std::string &, models::Lextok *, int,
             const std::string &);
void putremote(FILE *, models::Lextok *, int);
void putskip(int);
void putsrc(Element *);
void putstmnt(FILE *, models::Lextok *, int);
void putunames(FILE *);
void rem_Seq(void);
void runnable(ProcList *, int, int);
void sched(void);
void setaccess(models::Symbol *, models::Symbol *, int, int);
void set_lab(models::Symbol *, Element *);
void setmtype(models::Lextok *, models::Lextok *);
void setpname(models::Lextok *);
void setptype(models::Lextok *, models::Lextok *, int, models::Lextok *);
void setuname(models::Lextok *);
void setutype(models::Lextok *, models::Symbol *, models::Lextok *);
void setxus(models::Lextok *, int);
void start_claim(int);
void struct_name(models::Lextok *, models::Symbol *, int, std::string &);
void symdump(void);
void symvar(models::Symbol *);
void sync_product(void);
void trackchanuse(models::Lextok *, models::Lextok *, int);
void trackvar(models::Lextok *, models::Lextok *);
void trackrun(models::Lextok *);
void trapwonly(models::Lextok * /* , char * */); /* spin.y and main.c */
void typ2c(models::Symbol *);
void typ_ck(int, int, const std::string &);
void undostmnt(models::Lextok *, int);
void unrem_Seq(void);
void unskip(int);
void whoruns(int);
void wrapup(int);
void yyerror(char *, ...);

extern int unlink(const char *);

#define TMP_FILE1 "._s_p_i_n_"
#define TMP_FILE2 "._n_i_p_s_"

#endif
