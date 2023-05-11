#ifndef SEEN_SPIN_H
#define SEEN_SPIN_H

#include "models/element.hpp"
#include "models/fms.hpp"
#include "models/label.hpp"
#include "models/lextok.hpp"
#include "models/models_fwd.hpp"
#include "models/mtypes.hpp"
#include "models/proc_list.hpp"
#include "models/qh.hpp"
#include "models/queue.hpp"
#include "models/run_list.hpp"
#include "models/sequence.hpp"
#include "models/slicer.hpp"
#include "models/symbol.hpp"
#include "utils/memory.hpp"
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

models::Element *huntele(models::Element *, unsigned int, int);
models::Element *huntstart(models::Element *);
models::Element *mk_skip(void);
models::Element *target(models::Element *);
char *erealloc(void *, size_t, size_t);
int any_oper(models::Lextok *, int);
int any_undo(models::Lextok *);
int check_track(models::Lextok *);
int has_typ(models::Lextok *, int);
int pid_is_claim(int);
int proper_enabler(models::Lextok *);
int tl_main(int, char *[]);
int yyparse(void);
void AST_track(models::Lextok *, int);
void c_chandump(FILE *);
void c_var(FILE *, const std::string &, models::Symbol *);
void c_wrapper(FILE *);
void comment(FILE *, models::Lextok *, int);
void do_var(FILE *, int, const std::string &, models::Symbol *,
            const std::string &, const std::string &, const std::string &);
void dumpsrc(int, int);
void genaddproc(void);
void genaddqueue(void);
void genheader(void);
void genother(void);
void gensrc(void);
void gensvmap(void);
void genunio(void);
void mark_last(void);
void ntimes(FILE *, int, int, const char *c[]);
void prehint(models::Symbol *);
void putname(FILE *, const std::string &, models::Lextok *, int,
             const std::string &);
void putremote(FILE *, models::Lextok *, int);
void putskip(int);
void putsrc(models::Element *);
void putstmnt(FILE *, models::Lextok *, int);
void sync_product(void);
void typ2c(models::Symbol *);
void undostmnt(models::Lextok *, int);
void unskip(int);
void yyerror(char *, ...);

extern int unlink(const char *);

#define TMP_FILE1 "._s_p_i_n_"
#define TMP_FILE2 "._n_i_p_s_"

#endif
