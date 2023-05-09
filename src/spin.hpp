#ifndef SEEN_SPIN_H
#define SEEN_SPIN_H

#include "models/fms.hpp"
#include "models/lextok.hpp"
#include "models/models_fwd.hpp"
#include "models/mtypes.hpp"
#include "models/queue.hpp"
#include "models/slicer.hpp"
#include "models/symbol.hpp"
#include "models/element.hpp"
#include "models/label.hpp"
#include "models/proc_list.hpp"
#include "models/run_list.hpp"
#include "models/sequence.hpp"
#include "models/qh.hpp"
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

typedef models::Lextok *Lexptr;

#define YYSTYPE Lexptr

#define ZN (models::Lextok *)0
#define ZS (models::Symbol *)0
#define ZE (models::Element *)0

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
bool IsProctype(const std::string &);
bool IsUtype(const std::string &);
/***** prototype definitions *****/
models::Element *eval_sub(models::Element *);
models::Element *get_lab(models::Lextok *, int);
models::Element *huntele(models::Element *, unsigned int, int);
models::Element *huntstart(models::Element *);
models::Element *mk_skip(void);
models::Element *target(models::Element *);

models::Lextok *do_unless(models::Lextok *, models::Lextok *);
models::Lextok *expand(models::Lextok *, int);
models::Lextok *getuname(models::Symbol *);
models::Lextok *mk_explicit(models::Lextok *, int, int);
models::Lextok *nn(models::Lextok *, int, models::Lextok *, models::Lextok *);
models::Lextok *rem_lab(models::Symbol *, models::Lextok *, models::Symbol *);
models::Lextok *rem_var(models::Symbol *, models::Lextok *, models::Symbol *,
                        models::Lextok *);
models::Lextok *tail_add(models::Lextok *, models::Lextok *);

models::ProcList *mk_rdy(models::Symbol *, models::Lextok *, models::Sequence *, int,
                 models::Lextok *, models::btypes);

models::SeqList *seqlist(models::Sequence *, models::SeqList *);
models::Sequence *close_seq(int);

models::Symbol *break_dest(void);
models::Symbol *findloc(models::Symbol *);
models::Symbol *has_lab(models::Element *, int);
models::Symbol *lookup(const std::string &s);

char *put_inline(FILE *, const std::string &);
char *emalloc(size_t);
char *erealloc(void *, size_t, size_t);

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
int Enabled0(models::Element *);
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
int ismtype(char *);

int Lval_struct(models::Lextok *, models::Symbol *, int, int);
int main(int, char **);
int pc_value(models::Lextok *);
int pid_is_claim(int);
int proper_enabler(models::Lextok *);
int putcode(FILE *, models::Sequence *, models::Element *, int, int, int);
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
void doq(models::Symbol *, int, models::RunList *);
void dotag(FILE *, char *);
void do_locinits(FILE *);
void do_var(FILE *, int, const std::string &, models::Symbol *,
            const std::string &, const std::string &, const std::string &);
void dump_struct(models::Symbol *, const std::string &, models::RunList *);
void dumpclaims(FILE *, int, const std::string &);
void dumpglobals(void);
void dumplabels(void);
void dumplocal(models::RunList *, int);
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
void make_atomic(models::Sequence *, int);
void mark_last(void);
void match_trail(void);
void no_side_effects(const std::string &);
void nochan_manip(models::Lextok *, models::Lextok *, int);
void ntimes(FILE *, int, int, const char *c[]);
void open_seq(int);
void p_talk(models::Element *, int);
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
void putsrc(models::Element *);
void putstmnt(FILE *, models::Lextok *, int);
void putunames(FILE *);
void rem_Seq(void);
void runnable(models::ProcList *, int, int);
void sched(void);
void setaccess(models::Symbol *, models::Symbol *, int, int);
void set_lab(models::Symbol *, models::Element *);
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
