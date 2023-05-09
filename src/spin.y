%{
#include "/Users/vikdema/Desktop/projects/Spin/src++/src/spin.hpp"
#include "/Users/vikdema/Desktop/projects/Spin/src++/src/fatal/fatal.hpp"
#include "/Users/vikdema/Desktop/projects/Spin/src++/src/lexer/lexer.hpp"
#include "/Users/vikdema/Desktop/projects/Spin/src++/src/lexer/inline_processor.hpp"
#include "/Users/vikdema/Desktop/projects/Spin/src++/src/lexer/scope.hpp"
#include "/Users/vikdema/Desktop/projects/Spin/src++/src/models/symbol.hpp"
#include "/Users/vikdema/Desktop/projects/Spin/src++/src/models/lextok.hpp"
#include "/Users/vikdema/Desktop/projects/Spin/src++/src/lexer/yylex.hpp"
#include "/Users/vikdema/Desktop/projects/Spin/src++/src/main/launch_settings.hpp"
#include <sys/types.h>
#include <iostream>
#ifndef PC
#include <unistd.h>
#endif
#include <stdarg.h>
#include <stdlib.h>

#define YYMAXDEPTH	20000	/* default is 10000 */
#define YYDEBUG		0
#define Stop	models::Lextok::nn(ZN,'@',ZN,ZN)
#define PART0	"place initialized declaration of "
#define PART1	"place initialized chan decl of "
#define PART2	" at start of proctype "

extern lexer::Lexer lexer_;

static	models::Lextok *ltl_to_string(models::Lextok *);

extern  models::Symbol	*context, *owner;
extern	models::Lextok *for_body(models::Lextok *, int);
extern	void for_setup(models::Lextok *, models::Lextok *, models::Lextok *);
extern	models::Lextok *for_index(models::Lextok *, models::Lextok *);
extern	models::Lextok *sel_index(models::Lextok *, models::Lextok *, models::Lextok *);
extern  void    keep_track_off(models::Lextok *);
extern	void	safe_break(void);
extern	void	restore_break(void);
extern  int	u_sync, u_async;
extern	int	initialization_ok;
extern	short	has_sorted, has_random, has_enabled, has_pcvalue, has_np;
extern	short	has_state, has_io;
extern	void	check_mtypes(models::Lextok *, models::Lextok *);
extern	void	count_runs(models::Lextok *);
extern	void	no_internals(models::Lextok *);
extern	void	any_runs(models::Lextok *);
extern	void	validref(models::Lextok *, models::Lextok *);
extern	std::string yytext;
extern LaunchSettings launch_settings;

void ltl_list(const std::string& , const std::string& );

int	Mpars = 0;	/* max nr of message parameters  */
int	nclaims = 0;	/* nr of never claims */
int	Expand_Ok = 0, realread = 1, need_arguments = 0, NamesNotAdded = 0;
int	dont_simplify = 0;
char	*claimproc = (char *) 0;
char	*eventmap = (char *) 0;

static	char *ltl_name;
static	int  Embedded = 0, inEventMap = 0, has_ini = 0;

%}

%token	ASSERT PRINT PRINTM PREPROC
%token	C_CODE C_DECL C_EXPR C_STATE C_TRACK
%token	RUN LEN ENABLED SET_P GET_P EVAL PC_VAL
%token	TYPEDEF MTYPE INLINE RETURN LABEL OF
%token	GOTO BREAK ELSE SEMI ARROW
%token	IF FI DO OD FOR SELECT IN SEP DOTDOT
%token	ATOMIC NON_ATOMIC D_STEP UNLESS
%token  TIMEOUT NONPROGRESS
%token	ACTIVE PROCTYPE D_PROCTYPE
%token	HIDDEN SHOW ISLOCAL
%token	PRIORITY PROVIDED
%token	FULL EMPTY NFULL NEMPTY
%token	CONST TYPE XU			/* val */
%token	NAME UNAME PNAME INAME		/* sym */
%token	STRING CLAIM TRACE INIT	LTL	/* sym */

%right	ASGN
%left	SND O_SND RCV R_RCV /* SND doubles as boolean negation */
%left	IMPLIES EQUIV			/* ltl */
%left	OR
%left	AND
%left	ALWAYS EVENTUALLY		/* ltl */
%left	UNTIL WEAK_UNTIL RELEASE	/* ltl */
%right	NEXT				/* ltl */
%left	'|'
%left	'^'
%left	'&'
%left	EQ NE
%left	GT LT GE LE
%left	LSHIFT RSHIFT
%left	'+' '-'
%left	'*' '/' '%'
%left	INCR DECR
%right	'~' UMIN NEG
%left	DOT
%%

/** PROMELA Grammar Rules **/

program	: units		{ yytext.clear(); }
	;

units	: unit
	| units unit
	;

unit	: proc		/* proctype { }       */
	| init		/* init { }           */
	| claim		/* never claim        */
	| ltl		/* ltl formula        */
	| events	/* event assertions   */
	| one_decl	/* variables, chans   */
	| utype		/* user defined types */
	| c_fcts	/* c functions etc.   */
	| ns		/* named sequence     */
	| semi		/* optional separator */
	| error
	;

l_par	: '('		{ lexer_.inc_parameter_count();}
	;

r_par	: ')'		{ lexer_.des_parameter_count(); }
	;


proc	: inst		/* optional instantiator */
	  proctype NAME	{ 
			  setptype(ZN, $3, PROCTYPE, ZN);
			  setpname($3);
			  context = $3->symbol;
			  context->init_value = $2; /* linenr and file */
			  Expand_Ok++; /* expand struct names in decl */
			  has_ini = 0;
			}
	  l_par decl r_par	{ Expand_Ok--;
			  if (has_ini)
			  loger::fatal("initializer in parameter list" );
			}
	  Opt_priority
	  Opt_enabler
	  body		{ models::ProcList *rl;
			  if ($1 != ZN && $1->value > 0)
			  {	int j;
				rl = mk_rdy($3->symbol, $6, $11->sequence, $2->value, $10, models::btypes::A_PROC);
			  	for (j = 0; j < $1->value; j++)
				{	runnable(rl, $9?$9->value:1, 1);
					announce(":root:");
				}
				if (launch_settings.need_produce_symbol_table_information) {
					$3->symbol->init_value = $1;
				}
			  } else
			  {	rl = mk_rdy($3->symbol, $6, $11->sequence, $2->value, $10, models::btypes::P_PROC);
			  }
			  if (rl && has_ini == 1) /* global initializations, unsafe */
			  {	/* printf("proctype %s has initialized data\n",
					$3->symbol->name);
				 */
				rl->unsafe = 1;
			  }
			  context = ZS;
			}
	;

proctype: PROCTYPE	{ $$ = models::Lextok::nn(ZN,CONST,ZN,ZN); $$->value = 0; }
	| D_PROCTYPE	{ $$ = models::Lextok::nn(ZN,CONST,ZN,ZN); $$->value = 1; }
	;

inst	: /* empty */	{ $$ = ZN; }
	| ACTIVE	{ $$ = models::Lextok::nn(ZN,CONST,ZN,ZN); $$->value = 1; }
	| ACTIVE '[' const_expr ']' {
			  $$ = models::Lextok::nn(ZN,CONST,ZN,ZN); 
			  $$->value = $3->value;
			  if ($3->value > 255)
				loger::non_fatal("max nr of processes is 255\n");
			}
	| ACTIVE '[' NAME ']' {
			  $$ = models::Lextok::nn(ZN,CONST,ZN,ZN);
			  $$->value = 0;
			  if (!$3->symbol->type)
				loger::fatal("undeclared variable %s",
					$3->symbol->name);
			  else if ($3->symbol->init_value->node_type != CONST)
				loger::fatal("need constant initializer for %s\n",
					$3->symbol->name);
			  else
				$$->value = $3->symbol->init_value->value;
			}
	;

init	: INIT		{ 
	context = $1->symbol;
	 }
	  Opt_priority
	  body		{ models::ProcList *rl;
			  rl = mk_rdy(context, ZN, $4->sequence, 0, ZN,    models::btypes::I_PROC);
			  runnable(rl, $3?$3->value:1, 1);
			  announce(":root:");
			  context = ZS;
        		}
	;

ltl	: LTL optname2	{ 
	lexer_.SetLtlMode(true);  
ltl_name = new char[$2->symbol->name.length() + 1];
strcpy(ltl_name, $2->symbol->name.c_str());
}
	  ltl_body	{ if ($4) ltl_list($2->symbol->name, $4->symbol->name);
			  lexer_.SetLtlMode(false);
			}
	;

ltl_body: '{' full_expr OS '}' { $$ = ltl_to_string($2); }
	| error		{ $$ = NULL; }
	;

claim	: CLAIM	optname	{ if ($2 != ZN)
			  {	$1->symbol = $2->symbol;	/* new 5.3.0 */
			  }
			  nclaims++;
			  context = $1->symbol;
			  if (claimproc && !strcmp(claimproc, $1->symbol->name.c_str()))
			  {	loger::fatal("claim %s redefined", claimproc);
			  }
			  claimproc = new char[$1->symbol->name.length() + 1];
			  strcpy(claimproc, $1->symbol->name.c_str());
			}
	  body		{ (void) mk_rdy($1->symbol, ZN, $4->sequence, 0, ZN, models::btypes::N_CLAIM);
        		  context = ZS;
        		}
	;

optname : /* empty */	{ char tb[32];
			  memset(tb, 0, 32);
			  sprintf(tb, "never_%d", nclaims);
			  $$ = models::Lextok::nn(ZN, NAME, ZN, ZN);
			  $$->symbol = lookup(tb);
			}
	| NAME		{ $$ = $1; }
	;

optname2 : /* empty */ { char tb[32]; static int nltl = 0;
			  memset(tb, 0, 32);
			  sprintf(tb, "ltl_%d", nltl++);
			  $$ = models::Lextok::nn(ZN, NAME, ZN, ZN);
			  $$->symbol = lookup(tb);
			}
	| NAME		{ $$ = $1; }
	;

events : TRACE		{ 
	context = $1->symbol;
			  if (eventmap)
				loger::non_fatal("trace %s redefined", std::string(eventmap));
			  eventmap = new char[$1->symbol->name.length() + 1];
			  strcpy(eventmap, $1->symbol->name.c_str());
			  inEventMap++;
			}
	  body		{
			  if ($1->symbol->name ==  ":trace:")
			  {	(void) mk_rdy($1->symbol, ZN, $3->sequence, 0, ZN, models::btypes::E_TRACE);
			  } else
			  {	(void) mk_rdy($1->symbol, ZN, $3->sequence, 0, ZN, models::btypes::N_TRACE);
			  }
        		  context = ZS;
			  inEventMap--;
			}
	;

utype	: TYPEDEF NAME '{' 	{  if (context)
				   { loger::fatal("typedef %s must be global",
					$2->symbol->name);
				   }
				   owner = $2->symbol;
				   lexer_.SetInSeq($1->line_number);
				}
	  decl_lst '}'		{ setuname($5);
				  owner = ZS;
				  lexer_.SetInSeq(0);
				}
	;

nm	: NAME			{ $$ = $1; }
	| INAME			{ $$ = $1;
				  if (need_arguments)
				  loger::fatal("invalid use of '%s'", $1->symbol->name);
				}
	;

ns	: INLINE nm l_par		{ NamesNotAdded++; }
	  args r_par		{ lexer_.HandleInline($2->symbol, $5);
				  NamesNotAdded--;
				}
	;

c_fcts	: ccode			{ /* leaves pseudo-inlines with sym of
				   * type CODE_FRAG or CODE_DECL in global context
				   */
				}
	| cstate
	;

cstate	: C_STATE STRING STRING	{
				  c_state($2->symbol, $3->symbol, ZS);
				  lexer_.SetHasCode(1);
				  has_state = 1;
				}
	| C_TRACK STRING STRING {
				  c_track($2->symbol, $3->symbol, ZS);
				  lexer_.SetHasCode(1);
				  has_state = 1;
				}
	| C_STATE STRING STRING	STRING {
				  c_state($2->symbol, $3->symbol, $4->symbol);
				  lexer_.SetHasCode(1);
				  has_state = 1;
				}
	| C_TRACK STRING STRING STRING {
				  c_track($2->symbol, $3->symbol, $4->symbol);
				   lexer_.SetHasCode(1);
				   has_state = 1;
				}
	;

ccode	: C_CODE		{ models::Symbol *s;
				  NamesNotAdded++;
				  s = lexer_.HandleInline(ZS, ZN);
				  NamesNotAdded--;
				  $$ = models::Lextok::nn(ZN, C_CODE, ZN, ZN);
				  $$->symbol = s;
				  $$->line_number = $1->line_number;
				  $$->file_name = $1->file_name;
				  lexer_.SetHasCode(1);
				}
	| C_DECL		{ models::Symbol *s;
				  NamesNotAdded++;
				  s = lexer_.HandleInline(ZS, ZN);
				  NamesNotAdded--;
				  s->type = models::SymbolType::kCodeDecl;
				  $$ = models::Lextok::nn(ZN, C_CODE, ZN, ZN);
				  $$->symbol = s;
				  $$->line_number = $1->line_number;
				  $$->file_name = $1->file_name;
				  lexer_.SetHasCode(1);
				}
	;
cexpr	: C_EXPR		{ models::Symbol *s;
				  NamesNotAdded++;
				  s = lexer_.HandleInline(ZS, ZN);
/* if context is 0 this was inside an ltl formula
   mark the last inline added to seqnames */
				  if (!context)
				  {	
					lexer::InlineProcessor::SetIsExpr();
				  }
				  NamesNotAdded--;
				  $$ = models::Lextok::nn(ZN, C_EXPR, ZN, ZN);
				  $$->symbol = s;
				  $$->line_number = $1->line_number;
				  $$->file_name = $1->file_name;
				  no_side_effects(s->name);
				  lexer_.SetHasCode(1);
				}
	;

body	: '{'			{ open_seq(1); lexer_.SetInSeq($1->line_number); }
          sequence OS		{ add_seq(Stop); }
          '}'			{ $$->sequence = close_seq(0); lexer_.SetInSeq(0);
				  if (lexer::ScopeProcessor::GetCurrScopeLevel())
				  {	loger::non_fatal("missing '}' ?");
				  	lexer::ScopeProcessor::SetCurrScopeLevel(0);
				  }
				}
	;

sequence: step			{ if ($1) add_seq($1); }
	| sequence MS step	{ if ($3) add_seq($3); }
	;

step    : one_decl		{ $$ = ZN; }
	| XU vref_lst		{ setxus($2, $1->value); $$ = ZN; }
	| NAME ':' one_decl	{ loger::fatal("label preceding declaration,"); }
	| NAME ':' XU		{ loger::fatal("label preceding xr/xs claim,"); }
	| stmnt			{ $$ = $1; }
	| stmnt UNLESS		{ if ($1->node_type == DO) { safe_break(); } }
	  stmnt			{ if ($1->node_type == DO) { restore_break(); }
				  $$ = do_unless($1, $4);
				}
	| error
	;

vis	: /* empty */		{ $$ = ZN; }
	| HIDDEN		{ $$ = $1; }
	| SHOW			{ $$ = $1; }
	| ISLOCAL		{ $$ = $1; }
	;

asgn	: /* empty */		{ $$ = ZN; }
	| ':' NAME ASGN		{ $$ = $2; /* mtype decl */ }
	| ASGN			{ $$ = ZN; /* mtype decl */ }
	;

osubt	: /* empty */		{ $$ = ZN; }
	| ':' NAME		{ $$ = $2; }
	;

one_decl: vis TYPE osubt var_list {
				  setptype($3, $4, $2->value, $1);
				  $4->value = $2->value;
				  $$ = $4;
				}
	| vis UNAME var_list	{ setutype($3, $2->symbol, $1);
				  $$ = expand($3, Expand_Ok);
				}
	| vis TYPE asgn '{' nlst '}' {
				  if ($2->value != MTYPE)
					loger::fatal("malformed declaration");
				  setmtype($3, $5);
				  if ($1)
					loger::non_fatal("cannot %s mtype (ignored)",
						$1->symbol->name);
				  if (context != ZS)
					loger::fatal("mtype declaration must be global");
				}
	;

decl_lst: one_decl       	{ $$ = models::Lextok::nn(ZN, ',', $1, ZN); }
	| one_decl SEMI
	  decl_lst		{ $$ = models::Lextok::nn(ZN, ',', $1, $3); }
	;

decl    : /* empty */		{ $$ = ZN; }
	| decl_lst      	{ $$ = $1; }
	;

vref_lst: varref		{ $$ = models::Lextok::nn($1, XU, $1, ZN); }
	| varref ',' vref_lst	{ $$ = models::Lextok::nn($1, XU, $1, $3); }
	;

var_list: ivar           	{ $$ = models::Lextok::nn($1, TYPE, ZN, ZN); }
	| ivar ',' var_list	{ $$ = models::Lextok::nn($1, TYPE, ZN, $3); }
	;

c_list	: CONST			{ $1->node_type = CONST; $$ = $1; }
	| CONST ',' c_list	{ $1->node_type = CONST; $$ = models::Lextok::nn($1, ',', $1, $3); }
	;

ivar    : vardcl           	{ $$ = $1;
				  $1->symbol->init_value = models::Lextok::nn(ZN,CONST,ZN,ZN);
				  $1->symbol->init_value->value = 0;
				  if (!initialization_ok)
				  {	models::Lextok *zx, *xz;
					zx = models::Lextok::nn(ZN, NAME, ZN, ZN);
					zx->symbol = $1->symbol;
					xz = models::Lextok::nn(zx, ASGN, zx, $1->symbol->init_value);
					keep_track_off(xz);
					/* make sure zx doesnt turn out to be a STRUCT later */
					add_seq(xz);
				  }
				}
	| vardcl ASGN '{' c_list '}'	{	/* array initialization */
				  if (!$1->symbol->is_array)
					loger::fatal("%s must be an array", $1->symbol->name);
				  $$ = $1;
				  $1->symbol->init_value = $4;
				  has_ini = 1;
				  $1->symbol->hidden_flags |= (4|8);	/* conservative */
				  if (!initialization_ok)
				  {	models::Lextok *zx = models::Lextok::nn(ZN, NAME, ZN, ZN);
					zx->symbol = $1->symbol;
					add_seq(models::Lextok::nn(zx, ASGN, zx, $4));
				  }
				}
	| vardcl ASGN expr   	{ $$ = $1;	/* initialized scalar */
				  $1->symbol->init_value = $3;
				  if ($3->node_type == CONST
				  || ($3->node_type == NAME && $3->symbol->context))
				  {	has_ini = 2; /* local init */
				  } else
				  {	has_ini = 1; /* possibly global */
				  }
				  trackvar($1, $3);
				  if (any_oper($3, RUN))
				  {	loger::fatal("cannot use 'run' in var init, saw" );
				  }
				  nochan_manip($1, $3, 0);
				  no_internals($1);
				  if (!initialization_ok)
				  {	if ($1->symbol->is_array)
					{	fprintf(stderr, "warning: %s:%d initialization of %s[] ",
							$1->file_name->name.c_str(), $1->line_number,
							$1->symbol->name.c_str());
						fprintf(stderr, "could fail if placed here\n");
					} else
					{	models::Lextok *zx = models::Lextok::nn(ZN, NAME, ZN, ZN);
						zx->symbol = $1->symbol;
						add_seq(models::Lextok::nn(zx, ASGN, zx, $3));
						$1->symbol->init_value = 0;	/* Patrick Trentlin */
				  }	}
				}
	| vardcl ASGN ch_init	{ $1->symbol->init_value = $3;	/* channel declaration */
				  $$ = $1; has_ini = 1;
				  if (!initialization_ok)
				  {	loger::non_fatal(PART1 "'%s'" PART2, $1->symbol->name);
				  }
				}
	;

ch_init : '[' const_expr ']' OF
	  '{' typ_list '}'	{ 
				  if ($2->value){
					u_async++;
				  }else{
					u_sync++;
				  }
				{
					int i = cnt_mpars($6);
					Mpars = max(Mpars, i);
				}
				  $$ = models::Lextok::nn(ZN, CHAN, ZN, $6);
				  $$->value = $2->value;
				  $$->line_number = $1->line_number;
				  $$->file_name = $1->file_name;
        			}
	;

vardcl  : NAME  		{ $1->symbol->value_type = 1; $$ = $1; }
	| NAME ':' CONST	{ $1->symbol->nbits = $3->value;
				  if ($3->value >= 8*sizeof(long))
				  {	loger::non_fatal("width-field %s too large",
						$1->symbol->name);
					$3->value = 8*sizeof(long)-1;
				  }
				  $1->symbol->value_type = 1; $$ = $1;
				}
	| NAME '[' const_expr ']'	{ $1->symbol->value_type = $3->value; $1->symbol->is_array = 1; $$ = $1; }
	| NAME '[' NAME ']'	{	/* make an exception for an initialized scalars */
					$$ = models::Lextok::nn(ZN, CONST, ZN, ZN);
					fprintf(stderr, "spin: %s:%d, warning: '%s' in array bound ",
						$1->file_name->name.c_str(), $1->line_number, $3->symbol->name.c_str());
					if ($3->symbol->init_value
					&&  $3->symbol->init_value->value > 0)
					{	fprintf(stderr, "evaluated as %d\n", $3->symbol->init_value->value);
						$$->value = $3->symbol->init_value->value;
					} else
					{	fprintf(stderr, "evaluated as 1 by default (to avoid zero)\n");
						$$->value = 1;
					}
					$1->symbol->value_type = $$->value;
					$1->symbol->is_array = 1;
					$$ = $1;
				}
	;

varref	: cmpnd			{ $$ = mk_explicit($1, Expand_Ok, NAME); }
	;

pfld	: NAME			{ $$ = models::Lextok::nn($1, NAME, ZN, ZN);
				  if ($1->symbol->is_array && !lexer_.GetInFor() && !need_arguments)
				  {	loger::non_fatal("missing array index for '%s'",
						$1->symbol->name);
				  }
				}
	| NAME			{ owner = ZS; }
	  '[' expr ']'		{ $$ = models::Lextok::nn($1, NAME, $4, ZN); }
	;

cmpnd	: pfld			{ Embedded++;
				  if ($1->symbol->type ==  models::SymbolType::kStruct)
					owner = $1->symbol->struct_name;
				}
	  sfld			{ $$ = $1; $$->right = $3;
				  if ($3 && $1->symbol->type !=  models::SymbolType::kStruct)
					$1->symbol->type = models::SymbolType::kStruct;
				  Embedded--;
				  if (!Embedded && !NamesNotAdded
				  &&  !$1->symbol->type)
				   loger::fatal("undeclared variable: %s",
						$1->symbol->name);
				  if ($3) validref($1, $3->left);
				  owner = ZS;
				}
	;

sfld	: /* empty */		{ $$ = ZN; }
	| '.' cmpnd %prec DOT	{ $$ = models::Lextok::nn(ZN, '.', $2, ZN); }
	;

stmnt	: Special		{ $$ = $1; initialization_ok = 0; }
	| Stmnt			{ $$ = $1; initialization_ok = 0;
				  if (inEventMap) loger::non_fatal("not an event");
				}
	;

for_pre : FOR l_par		{ lexer_.SetInFor(1);}
	  varref		{ $4->ProcessSymbolForRead();
				  pushbreak(); /* moved up */
				  $$ = $4;
				}
	;

for_post: '{' sequence OS '}'
	| SEMI '{' sequence OS '}'

Special : varref RCV		{ Expand_Ok++; }
	  rargs			{ Expand_Ok--; has_io++;
				  $$ = models::Lextok::nn($1,  'r', $1, $4);
				  trackchanuse($4, ZN, 'R');
				}
	| varref SND		{ Expand_Ok++; }
	  margs			{ Expand_Ok--; has_io++;
				  $$ = models::Lextok::nn($1, 's', $1, $4);
				  $$->value=0; trackchanuse($4, ZN, 'S');
				  any_runs($4);
				}
	| for_pre ':' expr DOTDOT expr r_par	{
				  for_setup($1, $3, $5); lexer_.SetInFor(0);
				}
	  for_post		{ $$ = for_body($1, 1);
				}
	| for_pre IN varref r_par	{ $$ = for_index($1, $3);  lexer_.SetInFor(0);
				}
	  for_post		{ $$ = for_body($5, 1);
				}
	| SELECT l_par varref ':' expr DOTDOT expr r_par {
				  $3->ProcessSymbolForRead();
				  $$ = sel_index($3, $5, $7);
				}
	| IF options FI 	{ $$ = models::Lextok::nn($1, IF, ZN, ZN);
        			  $$->seq_list = $2->seq_list;
				  $$->line_number = $1->line_number;
				  $$->file_name = $1->file_name;
				  prune_opts($$);
        			}
	| DO    		{ pushbreak(); }
          options OD    	{ $$ = models::Lextok::nn($1, DO, ZN, ZN);
        			  $$->seq_list = $3->seq_list;
				  $$->line_number = $1->line_number;
				  $$->file_name = $1->file_name;
				  prune_opts($$);
        			}
	| BREAK  		{ $$ = models::Lextok::nn(ZN, GOTO, ZN, ZN);
				  $$->symbol = break_dest();
				}
	| GOTO NAME		{ $$ = models::Lextok::nn($2, GOTO, ZN, ZN);
				  if ($2->symbol->type != 0
				  &&  $2->symbol->type != models::SymbolType::kLabel) {
				  	loger::non_fatal("bad label-name %s",
					$2->symbol->name);
				  }
				  $2->symbol->type = models::SymbolType::kLabel;
				}
	| NAME ':' stmnt	{ $$ = models::Lextok::nn($1, ':',$3, ZN);
				  if ($1->symbol->type != 0
				  &&  $1->symbol->type != models::SymbolType::kLabel) {
				  	loger::non_fatal("bad label-name %s",
					$1->symbol->name);
				  }
				  $1->symbol->type = models::SymbolType::kLabel;
				}
	| NAME ':'		{ $$ = models::Lextok::nn($1, ':',ZN,ZN);
				  if ($1->symbol->type != 0
				  &&  $1->symbol->type != models::SymbolType::kLabel) {
				  	loger::non_fatal("bad label-name %s",
					$1->symbol->name);
				  }
				  $$->left = models::Lextok::nn(ZN, 'c', models::Lextok::nn(ZN,CONST,ZN,ZN), ZN);
				  $$->left->left->value = 1; /* skip */
				  $1->symbol->type = models::SymbolType::kLabel;
				}
	| error			{ $$ = models::Lextok::nn(ZN, 'c', models::Lextok::nn(ZN,CONST,ZN,ZN), ZN);
				  $$->left->value = 1; /* skip */
				}
	;

Stmnt	: varref ASGN full_expr	{ $$ = models::Lextok::nn($1, ASGN, $1, $3);	/* assignment */
				  trackvar($1, $3);
				  nochan_manip($1, $3, 0);
				  no_internals($1);
				}
	| varref INCR		{ $$ = models::Lextok::nn(ZN,CONST, ZN, ZN); $$->value = 1;
				  $$ = models::Lextok::nn(ZN,  '+', $1, $$);
				  $$ = models::Lextok::nn($1, ASGN, $1, $$);
				  trackvar($1, $1);
				  no_internals($1);
				  if ($1->symbol->type == CHAN)
				   loger::fatal("arithmetic on chan");
				}
	| varref DECR		{ $$ = models::Lextok::nn(ZN,CONST, ZN, ZN); $$->value = 1;
				  $$ = models::Lextok::nn(ZN,  '-', $1, $$);
				  $$ = models::Lextok::nn($1, ASGN, $1, $$);
				  trackvar($1, $1);
				  no_internals($1);
				  if ($1->symbol->type == CHAN)
				   loger::fatal("arithmetic on chan id's");
				}
	| SET_P l_par two_args r_par	{ $$ = models::Lextok::nn(ZN, SET_P, $3, ZN); lexer_.IncHasPriority(); }
	| PRINT	l_par STRING	{ realread = 0; }
	  prargs r_par		{ $$ = models::Lextok::nn($3, PRINT, $5, ZN); realread = 1; }
	| PRINTM l_par varref r_par	{ $$ = models::Lextok::nn(ZN, PRINTM, $3, ZN); }
	| PRINTM l_par CONST r_par	{ $$ = models::Lextok::nn(ZN, PRINTM, $3, ZN); }
	| ASSERT full_expr    	{ $$ = models::Lextok::nn(ZN, ASSERT, $2, ZN); AST_track($2, 0); }
	| ccode			{ $$ = $1; }
	| varref R_RCV		{ Expand_Ok++; }
	  rargs			{ Expand_Ok--; has_io++;
				  $$ = models::Lextok::nn($1,  'r', $1, $4);
				  $$->value = has_random = 1;
				  trackchanuse($4, ZN, 'R');
				}
	| varref RCV		{ Expand_Ok++; }
	  LT rargs GT		{ Expand_Ok--; has_io++;
				  $$ = models::Lextok::nn($1, 'r', $1, $5);
				  $$->value = 2;	/* fifo poll */
				  trackchanuse($5, ZN, 'R');
				}
	| varref R_RCV		{ Expand_Ok++; }
	  LT rargs GT		{ Expand_Ok--; has_io++;	/* rrcv poll */
				  $$ = models::Lextok::nn($1, 'r', $1, $5);
				  $$->value = 3; has_random = 1;
				  trackchanuse($5, ZN, 'R');
				}
	| varref O_SND		{ Expand_Ok++; }
	  margs			{ Expand_Ok--; has_io++;
				  $$ = models::Lextok::nn($1, 's', $1, $4);
				  $$->value = has_sorted = 1;
				  trackchanuse($4, ZN, 'S');
				  any_runs($4);
				}
	| full_expr		{ $$ = models::Lextok::nn(ZN, 'c', $1, ZN); count_runs($$); }
	| ELSE  		{ $$ = models::Lextok::nn(ZN,ELSE,ZN,ZN);
				}
	| ATOMIC   '{'   	{ open_seq(0); }
          sequence OS '}'   	{ $$ = models::Lextok::nn($1, ATOMIC, ZN, ZN);
        			  $$->seq_list = seqlist(close_seq(3), 0);
				  $$->line_number = $1->line_number;
				  $$->file_name = $1->file_name;
				  make_atomic($$->seq_list->this_sequence, 0);
        			}
	| D_STEP '{'		{ open_seq(0);
				  rem_Seq();
				}
          sequence OS '}'   	{ $$ = models::Lextok::nn($1, D_STEP, ZN, ZN);
        			  $$->seq_list = seqlist(close_seq(4), 0);
				  $$->line_number = $1->line_number;
				  $$->file_name = $1->file_name;
        			  make_atomic($$->seq_list->this_sequence, D_ATOM);
				  unrem_Seq();
        			}
	| '{'			{ open_seq(0); }
	  sequence OS '}'	{ $$ = models::Lextok::nn(ZN, NON_ATOMIC, ZN, ZN);
        			  $$->seq_list = seqlist(close_seq(5), 0);
				  $$->line_number = $1->line_number;
				  $$->file_name = $1->file_name;
        			}
	| INAME			{ need_arguments++; }
	  l_par args r_par	{ initialization_ok = 0;
	  			  lexer_.PickupInline($1->symbol, $4, ZN);
				  need_arguments--;
				}
	  Stmnt			{ $$ = $7; }

	| varref ASGN INAME	{ need_arguments++; /* inline call */ }
	  l_par args r_par	{ initialization_ok = 0;
				  lexer_.PickupInline($3->symbol, $6, $1);
				  need_arguments--;
				}
	  Stmnt			{ $$ = $9; }
	| RETURN full_expr	{ $$ = lexer_.ReturnStatement($2); }	
	;

options : option		{ $$->seq_list = seqlist($1->sequence, 0); }
	| option options	{ $$->seq_list = seqlist($1->sequence, $2->seq_list); }
	;

option  : SEP   		{ open_seq(0); }
          sequence OS		{ $$ = models::Lextok::nn(ZN,0,ZN,ZN);
				  $$->sequence = close_seq(6);
				  $$->line_number = $1->line_number;
				  $$->file_name = $1->file_name;
				}
	;

OS	: /* empty */
	| semi			{ /* redundant semi at end of sequence */ }
	;

semi	: SEMI
	| ARROW
	;

MS	: semi			{ /* at least one semi-colon */ }
	| MS semi		{ /* but more are okay too   */ }
	;

aname	: NAME			{ $$ = $1; }
	| PNAME			{ $$ = $1; }
	;

const_expr:	CONST			{ $$ = $1; }
	| '-' const_expr %prec UMIN	{ $$ = $2; $$->value = -($2->value); }
	| l_par const_expr r_par		{ $$ = $2; }
	| const_expr '+' const_expr	{ $$ = $1; $$->value = $1->value + $3->value; }
	| const_expr '-' const_expr	{ $$ = $1; $$->value = $1->value - $3->value; }
	| const_expr '*' const_expr	{ $$ = $1; $$->value = $1->value * $3->value; }
	| const_expr '/' const_expr	{ $$ = $1;
					  if ($3->value == 0)
					  { loger::fatal("division by zero" );
					  }
					  $$->value = $1->value / $3->value;
					}
	| const_expr '%' const_expr	{ $$ = $1;
					  if ($3->value == 0)
					  { loger::fatal("attempt to take modulo of zero" );
					  }
					  $$->value = $1->value % $3->value;
					}
	;

expr    : l_par expr r_par		{ $$ = $2; }
	| expr '+' expr		{ $$ = models::Lextok::nn(ZN, '+', $1, $3); }
	| expr '-' expr		{ $$ = models::Lextok::nn(ZN, '-', $1, $3); }
	| expr '*' expr		{ $$ = models::Lextok::nn(ZN, '*', $1, $3); }
	| expr '/' expr		{ $$ = models::Lextok::nn(ZN, '/', $1, $3); }
	| expr '%' expr		{ $$ = models::Lextok::nn(ZN, '%', $1, $3); }
	| expr '&' expr		{ $$ = models::Lextok::nn(ZN, '&', $1, $3); }
	| expr '^' expr		{ $$ = models::Lextok::nn(ZN, '^', $1, $3); }
	| expr '|' expr		{ $$ = models::Lextok::nn(ZN, '|', $1, $3); }
	| expr GT expr		{ $$ = models::Lextok::nn(ZN,  GT, $1, $3); }
	| expr LT expr		{ $$ = models::Lextok::nn(ZN,  LT, $1, $3); }
	| expr GE expr		{ $$ = models::Lextok::nn(ZN,  GE, $1, $3); }
	| expr LE expr		{ $$ = models::Lextok::nn(ZN,  LE, $1, $3); }
	| expr EQ expr		{ $$ = models::Lextok::nn(ZN,  EQ, $1, $3); }
	| expr NE expr		{ $$ = models::Lextok::nn(ZN,  NE, $1, $3); }
	| expr AND expr		{ $$ = models::Lextok::nn(ZN, AND, $1, $3); }
	| expr OR  expr		{ $$ = models::Lextok::nn(ZN,  OR, $1, $3); }
	| expr LSHIFT expr	{ $$ = models::Lextok::nn(ZN, LSHIFT,$1, $3); }
	| expr RSHIFT expr	{ $$ = models::Lextok::nn(ZN, RSHIFT,$1, $3); }
	| '~' expr		{ $$ = models::Lextok::nn(ZN, '~', $2, ZN); }
	| '-' expr %prec UMIN	{ $$ = models::Lextok::nn(ZN, UMIN, $2, ZN); }
	| SND expr %prec NEG	{ $$ = models::Lextok::nn(ZN, '!', $2, ZN); }
	| l_par expr ARROW expr ':' expr r_par {
				  $$ = models::Lextok::nn(ZN,  OR, $4, $6);
				  $$ = models::Lextok::nn(ZN, '?', $2, $$);
				}
	| RUN aname		{ Expand_Ok++;
				  if (!context)
				   loger::fatal("used 'run' outside proctype" );
				}
	  l_par args r_par
	  Opt_priority		{ Expand_Ok--;
				  $$ = models::Lextok::nn($2, RUN, $5, ZN);
				  $$->value = ($7) ? $7->value : 0;
				  trackchanuse($5, $2, 'A'); trackrun($$);
				}
	| LEN l_par varref r_par	{ $$ = models::Lextok::nn($3, LEN, $3, ZN); }
	| ENABLED l_par expr r_par	{ $$ = models::Lextok::nn(ZN, ENABLED, $3, ZN); has_enabled++; }
	| GET_P l_par expr r_par	{ $$ = models::Lextok::nn(ZN, GET_P, $3, ZN); lexer_.IncHasPriority(); }
	| varref RCV		{ Expand_Ok++; }
	  '[' rargs ']'		{ Expand_Ok--; has_io++;
				  $$ = models::Lextok::nn($1, 'R', $1, $5);
				}
	| varref R_RCV		{ Expand_Ok++; }
	  '[' rargs ']'		{ Expand_Ok--; has_io++;
				  $$ = models::Lextok::nn($1, 'R', $1, $5);
				  $$->value = has_random = 1;
				}
	| varref		{ $$ = $1; 
	$1->ProcessSymbolForRead(); }
	| cexpr			{ $$ = $1; }
	| CONST 		{ $$ = models::Lextok::nn(ZN,CONST,ZN,ZN);
				  $$->is_mtype_token = $1->is_mtype_token;
				  $$->symbol = $1->symbol;
				  $$->value = $1->value;
				}
	| TIMEOUT		{ $$ = models::Lextok::nn(ZN,TIMEOUT, ZN, ZN); }
	| NONPROGRESS		{ $$ = models::Lextok::nn(ZN,NONPROGRESS, ZN, ZN);
				  has_np++;
				}
	| PC_VAL l_par expr r_par	{ $$ = models::Lextok::nn(ZN, PC_VAL, $3, ZN);
				  has_pcvalue++;
				}
	| PNAME '[' expr ']' '@' NAME
	  			{ $$ = models::Lextok::CreateRemoteLabelAssignment($1->symbol, $3, $6->symbol); }
	| PNAME '[' expr ']' ':' pfld
	  			{ $$ = models::Lextok::CreateRemoteVariableAssignment($1->symbol, $3, $6->symbol, $6->left); }
	| PNAME '@' NAME	{ $$ = models::Lextok::CreateRemoteLabelAssignment($1->symbol, ZN, $3->symbol); }
	| PNAME ':' pfld	{ $$ = models::Lextok::CreateRemoteVariableAssignment($1->symbol, ZN, $3->symbol, $3->left); }
	| ltl_expr		{ $$ = $1; }
	;

Opt_priority:	/* none */	{ $$ = ZN; }
	| PRIORITY CONST	{ $$ = $2; lexer_.IncHasPriority(); }
	;

full_expr:	expr		{ $$ = $1; }
	| Expr			{ $$ = $1; }
	;

ltl_expr: expr UNTIL expr	{ $$ = models::Lextok::nn(ZN, UNTIL,   $1, $3); }
	| expr RELEASE expr	{ $$ = models::Lextok::nn(ZN, RELEASE, $1, $3); }
	| expr WEAK_UNTIL expr	{ $$ = models::Lextok::nn(ZN, ALWAYS, $1, ZN);
				  $$ = models::Lextok::nn(ZN, OR, $$, models::Lextok::nn(ZN, UNTIL, $1, $3));
				}
	| expr IMPLIES expr	{ $$ = models::Lextok::nn(ZN, '!', $1, ZN);
				  $$ = models::Lextok::nn(ZN, OR,  $$, $3);
				}
	| expr EQUIV expr	{ $$ = models::Lextok::nn(ZN, EQUIV,   $1, $3); }
	| NEXT expr       %prec NEG { $$ = models::Lextok::nn(ZN, NEXT,  $2, ZN); }
	| ALWAYS expr     %prec NEG { $$ = models::Lextok::nn(ZN, ALWAYS,$2, ZN); }
	| EVENTUALLY expr %prec NEG { $$ = models::Lextok::nn(ZN, EVENTUALLY, $2, ZN); }
	;

	/* an Expr cannot be negated - to protect Probe expressions */
Expr	: Probe			{ $$ = $1; }
	| l_par Expr r_par	{ $$ = $2; }
	| Expr AND Expr		{ $$ = models::Lextok::nn(ZN, AND, $1, $3); }
	| Expr AND expr		{ $$ = models::Lextok::nn(ZN, AND, $1, $3); }
	| expr AND Expr		{ $$ = models::Lextok::nn(ZN, AND, $1, $3); }
	| Expr OR  Expr		{ $$ = models::Lextok::nn(ZN,  OR, $1, $3); }
	| Expr OR  expr		{ $$ = models::Lextok::nn(ZN,  OR, $1, $3); }
	| expr OR  Expr		{ $$ = models::Lextok::nn(ZN,  OR, $1, $3); }
	;

Probe	: FULL l_par varref r_par	{ $$ = models::Lextok::nn($3,  FULL, $3, ZN); }
	| NFULL l_par varref r_par	{ $$ = models::Lextok::nn($3, NFULL, $3, ZN); }
	| EMPTY l_par varref r_par	{ $$ = models::Lextok::nn($3, EMPTY, $3, ZN); }
	| NEMPTY l_par varref r_par	{ $$ = models::Lextok::nn($3,NEMPTY, $3, ZN); }
	;

Opt_enabler:	/* none */	{ $$ = ZN; }
	| PROVIDED l_par full_expr r_par {
				   if (!proper_enabler($3))
				   { loger::non_fatal("invalid PROVIDED clause");
				     $$ = ZN;
				   } else
				   { $$ = $3;
				 } }
	| PROVIDED error	 { $$ = ZN;
				   loger::non_fatal("usage: provided ( ..expr.. )");
				 }
	;

oname	: /* empty */		{ $$ = ZN; }
	| ':' NAME		{ $$ = $2; }
	;

basetype: TYPE oname		{ if ($2)
				  {	if ($1->value != MTYPE)
					{	
						std::cout << loger::explainToString($1->value);
						loger::fatal("unexpected type" );
				  }	}
				  $$->symbol = $2 ? $2->symbol : ZS;
				  $$->value = $1->value;
				  if ($$->value == UNSIGNED)
				  loger::fatal("unsigned cannot be used as mesg type");
				}
	| UNAME			{ $$->symbol = $1->symbol;
				  $$->value = STRUCT;
				}
	| error			/* e.g., unsigned ':' const */
	;

typ_list: basetype		{ $$ = models::Lextok::nn($1, $1->value, ZN, ZN); }
	| basetype ',' typ_list	{ $$ = models::Lextok::nn($1, $1->value, ZN, $3); }
	;

two_args:	expr ',' expr	{ $$ = models::Lextok::nn(ZN, ',', $1, $3); }
	;

args    : /* empty */		{ $$ = ZN; }
	| arg			{ $$ = $1; }
	;

prargs  : /* empty */		{ $$ = ZN; }
	| ',' arg		{ $$ = $2; }
	;

margs   : arg			{ $$ = $1; }
	| expr l_par arg r_par	{ if ($1->node_type == ',')
					$$ = tail_add($1, $3);
				  else
				  	$$ = models::Lextok::nn(ZN, ',', $1, $3);
				}
	;

arg     : expr			{ if ($1->node_type == ',')
					$$ = $1;
				  else
				  	$$ = models::Lextok::nn(ZN, ',', $1, ZN);
				}
	| expr ',' arg		{ if ($1->node_type == ',')
					$$ = tail_add($1, $3);
				  else
				  	$$ = models::Lextok::nn(ZN, ',', $1, $3);
				}
	;

rarg	: varref  { 
					$$ = $1; 
					trackvar($1, $1);
					$1->ProcessSymbolForRead();
				  }
	| EVAL l_par expr r_par	{ 
				$$ = models::Lextok::nn(ZN,EVAL,$3,ZN);
				$1->ProcessSymbolForRead();
				}
	| CONST 		{ $$ = models::Lextok::nn(ZN,CONST,ZN,ZN);
				  $$->is_mtype_token = $1->is_mtype_token;
				  $$->symbol = $1->symbol;
				  $$->value = $1->value;
				}
	| '-' CONST %prec UMIN	{ $$ = models::Lextok::nn(ZN,CONST,ZN,ZN);
				  $$->value = - ($2->value);
				}
	;

rargs	: rarg			{ if ($1->node_type == ',')
					$$ = $1;
				  else
				  	$$ = models::Lextok::nn(ZN, ',', $1, ZN);
				}
	| rarg ',' rargs	{ if ($1->node_type == ',')
					$$ = tail_add($1, $3);
				  else
				  	$$ = models::Lextok::nn(ZN, ',', $1, $3);
				}
	| rarg l_par rargs r_par	{ if ($1->node_type == ',')
					$$ = tail_add($1, $3);
				  else
				  	$$ = models::Lextok::nn(ZN, ',', $1, $3);
				}
	| l_par rargs r_par		{ $$ = $2; }
	;

nlst	: NAME			{ $$ = models::Lextok::nn($1, NAME, ZN, ZN);
				  $$ = models::Lextok::nn(ZN, ',', $$, ZN); }
	| nlst NAME 		{ $$ = models::Lextok::nn($2, NAME, ZN, ZN);
				  $$ = models::Lextok::nn(ZN, ',', $$, $1);
				}
	| nlst ','		{ $$ = $1; /* commas optional */ }
	;
%%

#define binop(n, sop)	fprintf(fd, "("); recursive(fd, n->left); \
			fprintf(fd, ") %s (", sop); recursive(fd, n->right); \
			fprintf(fd, ")");
#define unop(n, sop)	fprintf(fd, "%s (", sop); recursive(fd, n->left); \
			fprintf(fd, ")");

static void
recursive(FILE *fd, models::Lextok *n)
{
	if (n)
	switch (n->node_type) {
	case NEXT:
		unop(n, "X");
		break;
	case ALWAYS:
		unop(n, "[]");
		break;
	case EVENTUALLY:
		unop(n, "<>");
		break;
	case '!':
		unop(n, "!");
		break;
	case UNTIL:
		binop(n, "U");
		break;
	case WEAK_UNTIL:
		binop(n, "W");
		break;
	case RELEASE: /* see http://en.wikipedia.org/wiki/Linear_temporal_logic */
		binop(n, "V");
		break;
	case OR:
		binop(n, "||");
		break;
	case AND:
		binop(n, "&&");
		break;
	case IMPLIES:
		binop(n, "->");
		break;
	case EQUIV:
		binop(n, "<->");
		break;
	case C_EXPR:
		fprintf(fd, "c_expr { %s }", put_inline(fd, n->symbol->name));
		break;
	default:
		comment(fd, n, 0);
		break;
	}
}

static models::Lextok *
ltl_to_string(models::Lextok *n)
{	models::Lextok *m = models::Lextok::nn(ZN, 0, ZN, ZN);
	ssize_t retval;
	char *ltl_formula = NULL;
	FILE *tf = fopen(TMP_FILE1, "w+"); /* tmpfile() fails on Windows 7 */

	/* convert the parsed ltl to a string
	   by writing into a file, using existing functions,
	   and then passing it to the existing interface for
	   conversion into a never claim
	  (this means parsing everything twice, which is
	   a little redundant, but adds only miniscule overhead)
	 */

	if (!tf)
	{	loger::fatal("cannot create temporary file" );
	}
	dont_simplify = 1;
	recursive(tf, n);
	dont_simplify = 0;
	(void) fseek(tf, 0L, SEEK_SET);

	size_t linebuffsize = 0;
	retval = getline(&ltl_formula, &linebuffsize, tf);
	fclose(tf);

	(void) unlink(TMP_FILE1);

	if (!retval)
	{	printf("%ld\n", (long int) retval);
		loger::fatal("could not translate ltl ltl_formula");
	}

	if (1) printf("ltl %s: %s\n", ltl_name, ltl_formula);

	m->symbol = lookup(ltl_formula);
#ifndef __MINGW32__
	free(ltl_formula);
#endif
	return m;
}

int
is_temporal(int t)
{
	return (t == EVENTUALLY || t == ALWAYS || t == UNTIL
	     || t == WEAK_UNTIL || t == RELEASE);
}

int
is_boolean(int t)
{
	return (t == AND || t == OR || t == IMPLIES || t == EQUIV);
}

void
yyerror(char *fmt, ...)
{
	loger::non_fatal(fmt );
}
//TODO: 
void ltl_list(const std::string &, const std::string &) {
  if (true
      // s_trail || launch_settings.need_to_analyze ||
      //       dumptab
      ) /* when generating pan.c or replaying a trace */
  {
   /* if (!ltl_claims) {
      ltl_claims = "_spin_nvr.tmp";
       if ((fd_ltl = fopen(ltl_claims, MFLAGS)) == NULL) {
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

    //fflush(tl_out);
    /* should read this file after the main file is read */
 // }
}