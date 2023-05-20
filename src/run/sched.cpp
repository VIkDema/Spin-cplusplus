#include "sched.hpp"

#include "../fatal/fatal.hpp"
#include "../helpers/helpers.hpp"
#include "../lexer/lexer.hpp"
#include "../lexer/line_number.hpp"
#include "../main/launch_settings.hpp"
#include "../main/main_processor.hpp"
#include "../models/lextok.hpp"
#include "../models/symbol.hpp"
#include "../run/flow.hpp"
#include "../run/run.hpp"
#include "../spin.hpp"
#include "../structs/structs.hpp"
#include "../symbol/symbol.hpp"
#include "../trail/guided.hpp"
#include "../utils/seed/seed.hpp"
#include "../utils/verbose/verbose.hpp"
#include "../variable/variable.hpp"
#include "y.tab.h"
#include <stdlib.h>

extern char *claimproc, *eventmap, GBuf[];
extern models::Ordered *all_names;
extern models::Symbol *Fname;
extern int nr_errs;
extern int u_sync, Elcnt, TstOnly;
extern short has_enabled;
extern int nclaims;
extern int has_stdin;
extern lexer::Lexer lexer_;
extern LaunchSettings launch_settings;
extern char GBuf[];
extern int firstrow;
extern int Npars;
extern int prno;

constexpr int kMaxNrOfProcesses =
    255; /* matches max nr of processes in verifier */

models::RunList *X_lst = (models::RunList *)0;
models::RunList *run_lst = (models::RunList *)0;
models::RunList *LastX = (models::RunList *)0; /* previous executing proc */
models::ProcList *ready = (models::ProcList *)0;
models::Element *LastStep = ZE;
int nproc = 0, nstop = 0, Tval = 0, Priority_Sum = 0;
int Rvous = 0, depth = 0, nrRdy = 0, MadeChoice;
short Have_claim = 0, Skip_claim = 0;
int limited_vis;

namespace sched {
namespace {
// PrintExecutionDetails
void PrintExecutionDetails(models::RunList *r) {
  auto &verbose_flags = utils::verbose::Flags::getInstance();
  if (verbose_flags.NeedToPrintVerbose() ||
      verbose_flags.NeedToPrintAllProcessActions()) {
    sched::DisplayExecutionStatus(r->pc, 1);
    printf("\n");
    if (verbose_flags.NeedToPrintGlobalVariables()) {
      variable::DumpGlobals();
    }
    if (verbose_flags.NeedToPrintLocalVariables()) {
      variable::DumpLocal(r, 1);
    }
  }
}

void AddLocalSymbol(models::RunList *r, models::Symbol *s) {
  models::Symbol *t;

  for (t = r->symtab; t; t = t->next)
    if (t->name == s->name && (launch_settings.need_old_scope_rules ||
                               t->block_scope == s->block_scope))
      return; /* it's already there */

  t = (models::Symbol *)emalloc(sizeof(models::Symbol));
  t->name = s->name;
  t->type = s->type;
  t->hidden_flags = s->hidden_flags;
  t->is_array = s->is_array;
  t->nbits = s->nbits;
  t->value_type = s->value_type;
  t->init_value = s->init_value;
  t->last_depth = depth;
  t->context = r->n;

  t->block_scope = s->block_scope;

  if (s->type != models::SymbolType::kStruct) {
    if (!s->value.empty()) /* if already initialized, copy info */
    {
      t->value = s->value;
    } else {
      variable::CheckVar(t, 0); /* initialize it */
    }
  } else {
    if (s->Sval)
      loger::fatal("saw preinitialized struct %s", s->name);
    t->struct_template = s->struct_template;
    t->struct_name = s->struct_name;
    t->owner_name = s->owner_name;
    /*	t->context = r->n; */
  }
  t->next = r->symtab; /* add it */
  r->symtab = t;
}

void InitializeLocalSymbols(models::RunList *r) {
  models::Ordered *walk;
  models::Symbol *sp;
  models::RunList *oX = X_lst;

  X_lst = r;
  for (walk = all_names; walk; walk = walk->next) {
    sp = walk->entry;
    if (sp && sp->context && sp->context->name == r->n->name && sp->id >= 0 &&
        (sp->type == models::SymbolType::kUnsigned ||
         sp->type == models::SymbolType::kBit ||
         sp->type == models::SymbolType::kMtype ||
         sp->type == models::SymbolType::kByte ||
         sp->type == models::SymbolType::kChan ||
         sp->type == models::SymbolType::kShort ||
         sp->type == models::SymbolType::kInt ||
         sp->type == models::SymbolType::kStruct)) {
      if (!sched::FindOrCreateLocalSymbol(sp))
        loger::non_fatal("InitializeLocalSymbols: cannot happen '%s'",
                         sp->name);
    }
  }
  X_lst = oX;
}

void ProcessParameter(models::RunList *r, models::Lextok *t, models::Lextok *a,
                      models::ProcList *p) {
  int k;
  int at, ft;
  models::RunList *oX = X_lst;

  if (!a)
    loger::fatal("missing actual parameters: '%s'", p->n->name);
  if (t->symbol->value_type > 1 || t->symbol->is_array)
    loger::fatal("array in parameter list, %s", t->symbol->name);
  k = run::Eval(a->left);

  at = a->left->ResolveSymbolType();
  X_lst = r; /* switch context */
  ft = t->ResolveSymbolType();

  if (at != ft && (at == CHAN || ft == CHAN)) {
    std::string buf, tag1, tag2;
    helpers::PutType(tag1, ft);
    helpers::PutType(tag2, at);
    buf = "type-clash in params of " + p->n->name + "(..), (" + tag1 + " <-> " +
          tag2 + ")";
    loger::non_fatal("%s", buf);
  }
  t->node_type = NAME;
  AddLocalSymbol(r, t->symbol);
  variable::SetVal(t, k);

  X_lst = oX;
}

void SetProcedureParameters(models::RunList *r, models::ProcList *p,
                            models::Lextok *q) {
  models::Lextok *f, *a; /* formal and actual pars */
  models::Lextok *t;     /* list of pars of 1 type */

  if (q) {
    file::LineNumber::Set(q->line_number);
    Fname = q->file_name;
  }
  for (f = p->p, a = q; f; f = f->right) /* one type at a time */
    for (t = f->left; t; t = t->right, a = (a) ? a->right : a) {
      if (t->node_type != ',')
        ProcessParameter(r, t, a, p); /* plain var */
      else
        ProcessParameter(r, t->left, a, p); /* expanded struct */
    }
}

void HandleMultipleClaims(void) {
  models::ProcList *p, *q = NULL;

  if (nclaims > 1) {
    printf("  the model contains %d never claims:", nclaims);
    for (p = ready; p; p = p->next) {
      if (p->b == models::btypes::N_CLAIM) {
        printf("%s%s", q ? ", " : " ", p->n->name.c_str());
        q = p;
      }
    }
    printf("\n");
    printf("  only one claim is used in a verification run\n");
    printf("  choose which one with ./pan -a -N name (defaults to -N %s)\n",
           q ? q->n->name.c_str() : "--");
    printf("  or use e.g.: spin++ -search -ltl %s %s\n",
           q ? q->n->name.c_str() : "--",
           Fname ? Fname->name.c_str() : "filename");
  }
}

static char is_blocked[256];

int SetProcessBlocked(int p) {
  int i, j;

  is_blocked[p % 256] = 1;
  for (i = j = 0; i < nproc - nstop; i++)
    j += is_blocked[i];
  if (j >= nproc - nstop) {
    memset(is_blocked, 0, 256);
    return 1;
  }
  return 0;
}

models::Element *PerformSilentMoves(models::Element *e) {
  models::Element *f;

  if (e->n)
    switch (e->n->node_type) {
    case GOTO:
      if (Rvous)
        break;
      f = flow::GetLabel(e->n, 1);
      flow::CrossDsteps(e->n, f->n);
      return f; /* guard against goto cycles */
    case UNLESS:
      return PerformSilentMoves(e->sub->this_sequence->frst);
    case NON_ATOMIC:
    case ATOMIC:
    case D_STEP:
      e->n->seq_list->this_sequence->last->next = e->next;
      return PerformSilentMoves(e->n->seq_list->this_sequence->frst);
    case '.':
      return PerformSilentMoves(e->next);
    }
  return e;
}

int CanProcessRun(void) /* the currently selected process in X_lst can run */
{
  if (X_lst->prov && !run::Eval(X_lst->prov)) {
    if (0)
      printf("pid %d cannot run: not provided\n", X_lst->pid);
    return 0;
  }
  if (lexer_.GetHasPriority() &&
      !launch_settings.need_revert_old_rultes_for_priority) {
    models::Lextok *n = models::Lextok::nn(ZN, CONST, ZN, ZN);
    n->value = X_lst->pid;
    if (0)
      printf("pid %d %s run (priority)\n", X_lst->pid,
             run::PCHighest(n) ? "can" : "cannot");
    return run::PCHighest(n);
  }
  if (0)
    printf("pid %d can run\n", X_lst->pid);
  return 1;
}

models::RunList *pickProcess(models::RunList *Y) {
  auto &verbose_flags = utils::verbose::Flags::getInstance();
  models::SeqList *z;
  models::Element *has_else;
  short Choices[256];
  int j, k, nr_else = 0;

  if (nproc <= nstop + 1) {
    X_lst = run_lst;
    return NULL;
  }
  if (!launch_settings.need_to_run_in_interactive_mode ||
      depth < launch_settings.count_of_skipping_steps) {
    if (lexer_.GetHasPriority() &&
        !launch_settings.need_revert_old_rultes_for_priority) /* new 6.3.2 */
    {
      j = utils::seed::Seed::Rand() % (nproc - nstop);
      for (X_lst = run_lst; X_lst; X_lst = X_lst->next) {
        if (j-- <= 0)
          break;
      }
      if (X_lst == NULL) {
        loger::fatal("unexpected, pickProcess");
      }
      j = nproc - nstop;
      while (j-- > 0) {
        if (CanProcessRun()) {
          Y = X_lst;
          break;
        }
        X_lst = (X_lst->next) ? X_lst->next : run_lst;
      }
      return Y;
    }
    if (Priority_Sum < nproc - nstop)
      loger::fatal("cannot happen - weights");
    j = (int)utils::seed::Seed::Rand() % Priority_Sum;

    while (j - X_lst->priority >= 0) {
      j -= X_lst->priority;
      Y = X_lst;
      X_lst = X_lst->next;
      if (!X_lst) {
        Y = NULL;
        X_lst = run_lst;
      }
    }

  } else {
    int only_choice = -1;
    int no_choice = 0, proc_no_ch, proc_k;

    Tval = 0; /* new 4.2.6 */
  try_again:
    printf("Select a statement\n");
  try_more:
    for (X_lst = run_lst, k = 1; X_lst; X_lst = X_lst->next) {
      if (X_lst->pid > 255)
        break;

      Choices[X_lst->pid] = (short)k;

      if (!X_lst->pc || !CanProcessRun()) {
        if (X_lst == run_lst)
          Choices[X_lst->pid] = 0;
        continue;
      }
      X_lst->pc = PerformSilentMoves(X_lst->pc);
      if (!X_lst->pc->sub && X_lst->pc->n) {
        int unex;
        unex = !run::Enabled(X_lst->pc);
        if (unex)
          no_choice++;
        else
          only_choice = k;
        if (unex && !verbose_flags.NeedToPrintVerbose()) {
          k++;
          continue;
        }
        printf("\tchoice %d: ", k++);
        sched::DisplayExecutionStatus(X_lst->pc, 0);
        if (unex)
          printf(" unexecutable,");
        printf(" [");
        comment(stdout, X_lst->pc->n, 0);
        if (X_lst->pc->esc)
          printf(" + Escape");
        printf("]\n");
      } else {
        has_else = ZE;
        proc_no_ch = no_choice;
        proc_k = k;
        for (z = X_lst->pc->sub, j = 0; z; z = z->next) {
          models::Element *y = PerformSilentMoves(z->this_sequence->frst);
          int unex;
          if (!y)
            continue;

          if (y->n->node_type == ELSE) {
            has_else = (Rvous) ? ZE : y;
            nr_else = k++;
            continue;
          }

          unex = !run::Enabled(y);
          if (unex)
            no_choice++;
          else
            only_choice = k;
          if (unex && !verbose_flags.NeedToPrintVerbose()) {
            k++;
            continue;
          }
          printf("\tchoice %d: ", k++);
          sched::DisplayExecutionStatus(X_lst->pc, 0);
          if (unex)
            printf(" unexecutable,");
          printf(" [");
          comment(stdout, y->n, 0);
          printf("]\n");
        }
        if (has_else) {
          if (no_choice - proc_no_ch >= (k - proc_k) - 1) {
            only_choice = nr_else;
            printf("\tchoice %d: ", nr_else);
            sched::DisplayExecutionStatus(X_lst->pc, 0);
            printf(" [else]\n");
          } else {
            no_choice++;
            printf("\tchoice %d: ", nr_else);
            sched::DisplayExecutionStatus(X_lst->pc, 0);
            printf(" unexecutable, [else]\n");
          }
        }
      }
    }
    X_lst = run_lst;
    if (k - no_choice < 2 && Tval == 0) {
      Tval = 1;
      no_choice = 0;
      only_choice = -1;
      goto try_more;
    } else {
      if (k - no_choice < 2) {
        printf("no executable choices\n");
        MainProcessor::Exit(0);
      }
      printf("Select [1-%d]: ", k - 1);
    }
    if (k - no_choice == 2) {
      printf("%d\n", only_choice);
      j = only_choice;
    } else {
      char buf[256];
      fflush(stdout);
      if (scanf("%64s", buf) == 0) {
        printf("\tno input\n");
        goto try_again;
      }
      j = -1;
      if (isdigit((int)buf[0]))
        j = atoi(buf);
      else {
        if (buf[0] == 'q')
          MainProcessor::Exit(0);
      }
      if (j < 1 || j >= k) {
        printf("\tchoice is outside range\n");
        goto try_again;
      }
    }
    MadeChoice = 0;
    Y = NULL;
    for (X_lst = run_lst; X_lst; Y = X_lst, X_lst = X_lst->next) {
      if (!X_lst->next || X_lst->next->pid > 255 ||
          j < Choices[X_lst->next->pid]) {
        MadeChoice = 1 + j - Choices[X_lst->pid];
        break;
      }
    }
  }
  return Y;
}

int FindProcessID(const std::string &n) {
  models::RunList *r;
  int rval = -1;

  for (r = run_lst; r; r = r->next) {
    if (n == r->n->name) {
      if (rval >= 0) {
        printf("spin++: remote ref to proctype %s, ", n.c_str());
        printf("has more than one match: %d and %d\n", rval, r->pid);
      } else
        rval = r->pid;
    }
  }
  return rval;
}

void DumpFormalParameters() {
  models::ProcList *p;
  models::Lextok *f, *t;
  int count;

  for (p = ready; p; p = p->next) {
    if (!p->p)
      continue;
    count = -1;
    for (f = p->p; f; f = f->right)      /* types */
      for (t = f->left; t; t = t->right) /* formals */
      {
        if (t->node_type != ',')
          t->symbol->id = count--; /* overload id */
        else
          t->left->symbol->id = count--;
      }
  }
}

} // namespace

int ResolveRemoteVariableReference(models::Lextok *n) {
  int prno, i, added = 0;
  models::RunList *Y, *oX;
  models::Lextok *onl;
  models::Symbol *os;
  file::LineNumber::Set(n->line_number);
  Fname = n->file_name;

  if (!n->left->left)
    prno = FindProcessID(n->left->symbol->name);
  else {
    prno = run::Eval(n->left->left); /* pid - can cause recursive call */
    {
      prno += Have_claim;
      added = Have_claim;
    }
  }

  if (prno < 0) {
    return 0; /* non-existing process */
  }
  i = nproc - nstop + Skip_claim; /* 6.0: added Skip_claim */
  for (Y = run_lst; Y; Y = Y->next)
    if (--i == prno) {
      if (Y->n->name != n->left->symbol->name) {
        printf("spin++: remote reference error on '%s[%d]'\n",
               n->left->symbol->name.c_str(), prno - added);
        loger::non_fatal("refers to wrong proctype '%s'", Y->n->name.c_str());
      }
      if (n->symbol->name == "_p") {
        if (Y->pc) {
          return Y->pc->seqno;
        }
        /* harmless, can only happen with -t */
        return 0;
      }

      /* check remote variables */
      oX = X_lst;
      X_lst = Y;

      onl = n->left;
      n->left = n->right;

      os = n->symbol;
      if (!n->symbol->context) {
        n->symbol->context = Y->n;
      }
      {
        bool rs = launch_settings.need_old_scope_rules;
        launch_settings.need_old_scope_rules = true;
        n->symbol = sched::FindOrCreateLocalSymbol(n->symbol);
        launch_settings.need_old_scope_rules = rs;
      }
      i = variable::GetValue(n);

      n->symbol = os;
      n->left = onl;
      X_lst = oX;
      return i;
    }
  printf("remote ref: %s[%d] ", n->left->symbol->name.c_str(), prno - added);
  loger::non_fatal("%s not found", n->symbol->name);
  printf("have only:\n");
  i = nproc - nstop - 1;
  for (Y = run_lst; Y; Y = Y->next, i--)
    if (Y->n->name == n->left->symbol->name) {
      printf("\t%d\t%s\n", i, Y->n->name.c_str());
    }
  return 0;
}

int ResolveRemoteLabelReference(models::Lextok *n) {
  int i;

  file::LineNumber::Set(n->line_number);
  Fname = n->file_name;
  if (n->symbol->type != 0 && n->symbol->type != LABEL) {
    printf("spin++: error, type: %d\n", n->symbol->type);
    loger::fatal("not a labelname: '%s'", n->symbol->name);
  }
  if (n->index_step >= 0) {
    loger::fatal("remote ref to label '%s' inside d_step", n->symbol->name);
  }
  if ((i = flow::FindLabel(n->symbol, n->left->symbol, 1)) == 0) /* remotelab */
  {
    loger::fatal("unknown labelname: %s", n->symbol->name);
  }
  return i;
}

void DisplayExecutionStatus(models::Element *e, int lnr) {
  static int lastnever = -1;
  static std::string nbuf;
  int newnever = -1;

  if (e && e->n) {
    newnever = e->n->line_number;
  }

  if (Have_claim && X_lst && X_lst->pid == 0 && lastnever != newnever && e) {
    lastnever = newnever;
  }

  PrintCurrentProcessInfo(lnr);
  if (e) {
    if (e->n) {
      std::string ptr = e->n->file_name->name;
      std::string qtr;
      for (char c : ptr) {
        if (c != '"') {
          qtr += c;
        }
      }
      nbuf = qtr;
    } else {
      nbuf = "-";
    }
    printf("%s:%d (state %d)", nbuf.c_str(), e->n ? e->n->line_number : -1,
           e->seqno);
    if ((e->status & ENDSTATE) || flow::HasLabel(e, 2)) /* 2=end */
    {
      printf(" <valid end state>");
    }
  }
}

void PrintCurrentProcessInfo(int lnr) {
  if (!X_lst)
    return;

  if (lnr)
    printf("%3d:	", depth);
  printf("proc ");
  if (Have_claim && X_lst->pid == 0)
    printf(" -");
  else
    printf("%2d", X_lst->pid - Have_claim);
  if (launch_settings.need_revert_old_rultes_for_priority) {
    printf(" (%s) ", X_lst->n->name.c_str());
  } else {
    printf(" (%s:%d) ", X_lst->n->name.c_str(), X_lst->priority);
  }
}

int AssignLocalValue(models::Lextok *p, int m) {
  models::Symbol *r = sched::FindOrCreateLocalSymbol(p->symbol);
  int n = run::Eval(p->left);

  if (IsIndexInBounds(r, n)) {
    if (r->type == models::SymbolType::kStruct)
      structs::Lval_struct(p, r, 1, m); /* 1 = check init */
    else {
      r->value[n] = variable::CastValue(r->type, m, r->nbits.value_or(0));
      r->last_depth = depth;
    }
  }

  return 1;
}

int GetLocalValue(models::Lextok *sn) {
  models::Symbol *r, *s = sn->symbol;
  int n = run::Eval(sn->left);

  r = sched::FindOrCreateLocalSymbol(s);
  if (r && r->type == STRUCT) {
    return structs::Rval_struct(sn, r, 1); /* 1 = check init */
  }
  if (IsIndexInBounds(r, n)) {
    return variable::CastValue(r->type, r->value[n], r->nbits.value_or(0));
  }
  return 0;
}

int IsIndexInBounds(models::Symbol *r, int n) {
  if (!r)
    return 0;

  if (n >= r->value_type || n < 0) {
    printf("spin++: indexing %s[%d] - size is %d\n", r->name.c_str(), n,
           r->value_type);
    loger::non_fatal("indexing array \'%s\'", r->name);
    return 0;
  }
  return 1;
}

models::Symbol *FindOrCreateLocalSymbol(models::Symbol *s) {
  models::Symbol *r;

  if (!X_lst) { /* loger::fatal("error, cannot eval '%s' (no proc)", s->name);
                 */
    return ZS;
  }
  for (r = X_lst->symtab; r; r = r->next) {
    if (r->name == s->name && (launch_settings.need_old_scope_rules ||
                               r->block_scope == s->block_scope)) {
      break;
    }
  }
  if (!r) {
    AddLocalSymbol(X_lst, s);
    r = X_lst->symtab;
  }
  return r;
}

int PerformRendezvousCompletion(void) {
  models::RunList *orun = X_lst, *tmp;
  models::Element *s_was = LastStep;
  models::Element *e;
  auto &verbose_flags = utils::verbose::Flags::getInstance();
  int j;
  bool ointer = launch_settings.need_to_run_in_interactive_mode;

  if (launch_settings.need_save_trail)
    return 1;
  if (orun->pc->status & D_ATOM)
    loger::fatal("rv-attempt in d_step sequence");
  Rvous = 1;
  launch_settings.need_to_run_in_interactive_mode = false;

  j = (int)utils::seed::Seed::Rand() % Priority_Sum; /* randomize start point */
  X_lst = run_lst;
  while (j - X_lst->priority >= 0) {
    j -= X_lst->priority;
    X_lst = X_lst->next;
    if (!X_lst)
      X_lst = run_lst;
  }
  for (j = nproc - nstop; j > 0; j--) {
    if (X_lst != orun && (!X_lst->prov || run::Eval(X_lst->prov)) &&
        (e = run::EvalSub(X_lst->pc))) {
      if (TstOnly) {
        X_lst = orun;
        Rvous = 0;
        goto out;
      }
      if ((verbose_flags.NeedToPrintVerbose()) ||
          (verbose_flags.NeedToPrintAllProcessActions())) {
        tmp = orun;
        orun = X_lst;
        X_lst = tmp;
        if (!s_was)
          s_was = X_lst->pc;
        sched::DisplayExecutionStatus(s_was, 1);
        printf("	[");
        comment(stdout, s_was->n, 0);
        printf("]\n");
        tmp = orun; /* orun = X_lst; */
        X_lst = tmp;
        if (!LastStep)
          LastStep = X_lst->pc;
        sched::DisplayExecutionStatus(LastStep, 1);
        printf("	[");
        comment(stdout, LastStep->n, 0);
        printf("]\n");
      }
      Rvous = 0; /* before PerformSilentMoves */
      X_lst->pc = PerformSilentMoves(e);
    out:
      launch_settings.need_to_run_in_interactive_mode = ointer;
      return 1;
    }

    X_lst = X_lst->next;
    if (!X_lst)
      X_lst = run_lst;
  }
  Rvous = 0;
  X_lst = orun;
  launch_settings.need_to_run_in_interactive_mode = ointer;
  return 0;
}

void ScheduleProcesses() {
  models::Element *e;
  models::RunList *Y = NULL; /* previous process in run queue */
  models::RunList *oX;
  int go, notbeyond = 0;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  if (launch_settings.need_produce_symbol_table_information) {
    DumpFormalParameters();
    symbol::DumpSymbols();
    flow::DumpLabels();
    return;
  }
  if (lexer_.GetHasCode() && !launch_settings.need_to_analyze) {
    printf("spin++: warning: c_code fragments remain uninterpreted\n");
    printf("      in random simulations with spin++; use ./pan -r instead\n");
  }

  if (has_enabled && u_sync > 0) {
    printf("spin++: error, cannot use 'enabled()' in ");
    printf("models with synchronous channels.\n");
    nr_errs++;
  }
  if (launch_settings.need_compute_synchronous_product_multiple_never_claims) {
    sync_product();
    MainProcessor::Exit(0);
  }
  if (launch_settings.need_to_analyze &&
      (!launch_settings.need_to_replay || lexer_.GetHasCode())) {
    gensrc();
    HandleMultipleClaims();
    return;
  }
  if (launch_settings.need_to_replay && !lexer_.GetHasCode()) {
    return;
  }
  if (launch_settings.need_save_trail) {
    trail::ProcessTrailFile();
    return;
  }

  if (claimproc)
    printf("warning: never claim not used in random simulation\n");
  if (eventmap)
    printf("warning: trace assertion not used in random simulation\n");

  X_lst = run_lst;
  Y = pickProcess(Y);

  while (X_lst) {
    models::Symbol::SetContext(X_lst->n);
    if (X_lst->pc && X_lst->pc->n) {
      file::LineNumber::Set(X_lst->pc->n->line_number);
      Fname = X_lst->pc->n->file_name;
    }
    if (launch_settings.count_of_steps > 0 &&
        depth >= launch_settings.count_of_steps) {
      printf("-------------\n");
      printf("depth-limit (-u%d steps) reached\n",
             launch_settings.count_of_steps);
      break;
    }
    depth++;
    LastStep = ZE;
    oX = X_lst; /* a rendezvous could change it */
    go = 1;
    if (X_lst->pc && !(X_lst->pc->status & D_ATOM) && !CanProcessRun()) {
      if (((verbose_flags.NeedToPrintVerbose()) ||
           (verbose_flags.NeedToPrintAllProcessActions()))) {
        sched::DisplayExecutionStatus(X_lst->pc, 1);
        printf("\t<<Not Enabled>>\n");
      }
      go = 0;
    }
    if (go && (e = run::EvalSub(X_lst->pc))) {
      if (depth >= launch_settings.count_of_skipping_steps &&
          ((verbose_flags.NeedToPrintVerbose()) ||
           (verbose_flags.NeedToPrintAllProcessActions()))) {
        if (X_lst == oX)
          if (!(e->status & D_ATOM) ||
              verbose_flags.NeedToPrintVerbose()) /* no talking in d_steps */
          {
            if (!LastStep)
              LastStep = X_lst->pc;
            /* A. Tanaka, changed order */
            sched::DisplayExecutionStatus(LastStep, 1);
            printf("	[");
            comment(stdout, LastStep->n, 0);
            printf("]\n");
          }
        if (verbose_flags.NeedToPrintGlobalVariables()) {
          variable::DumpGlobals();
        }

        if (verbose_flags.NeedToPrintLocalVariables()) {
          variable::DumpLocal(X_lst, 0);
        }
      }
      if (oX != X_lst || (X_lst->pc->status & (ATOM | D_ATOM))) /* new 5.0 */
      {
        e = PerformSilentMoves(e);
        notbeyond = 0;
      }
      oX->pc = e;
      LastX = X_lst;

      if (!launch_settings.need_to_run_in_interactive_mode)
        Tval = 0;
      memset(is_blocked, 0, 256);

      if (X_lst->pc && (X_lst->pc->status & (ATOM | L_ATOM)) &&
          (notbeyond == 0 || oX != X_lst)) {
        if ((X_lst->pc->status & L_ATOM))
          notbeyond = 1;
        continue; /* no process switch */
      }
    } else {
      depth--;
      if (oX->pc && (oX->pc->status & D_ATOM)) {
        loger::non_fatal("stmnt in d_step blocks");
      }
      if (X_lst->pc && X_lst->pc->n && X_lst->pc->n->node_type == '@' &&
          X_lst->pid == (nproc - nstop - 1)) {
        if (X_lst != run_lst && Y != NULL)
          Y->next = X_lst->next;
        else
          run_lst = X_lst->next;
        nstop++;
        Priority_Sum -= X_lst->priority;
        if (verbose_flags.NeedToPrintAllProcessActions()) {
          PrintCurrentProcessInfo(1);
          DoTag(stdout, "terminates\n");
        }
        LastX = X_lst;
        if (!launch_settings.need_to_run_in_interactive_mode)
          Tval = 0;
        if (nproc == nstop)
          break;
        memset(is_blocked, 0, 256);
        /* proc X_lst is no longer in runlist */
        X_lst = (X_lst->next) ? X_lst->next : run_lst;
      } else {
        if (SetProcessBlocked(X_lst->pid)) {
          if (Tval && !has_stdin) {
            break;
          }
          if (!Tval && depth >= launch_settings.count_of_skipping_steps) {
            oX = X_lst;
            X_lst = (models::RunList *)0; /* to suppress indent */
            DoTag(stdout, "timeout\n");
            X_lst = oX;
            Tval = 1;
          }
        }
      }
    }

    if (!run_lst || !X_lst)
      break; /* new 5.0 */

    Y = pickProcess(X_lst);
    notbeyond = 0;
  }
  models::Symbol::SetContext(ZS);
  RenameWrapup(0);
}

void RenameWrapup(int fini) {
  auto &verbose_flags = utils::verbose::Flags::getInstance();
  limited_vis = 0;
  if (launch_settings.need_columnated_output) {
    if (!launch_settings.need_disable_final_state_reporting) {
      printf("-------------\nfinal state:\n-------------\n");
    }
  }

  if (launch_settings.need_disable_final_state_reporting) {
    goto short_cut;
  }
  if (nproc != nstop) {
    printf("#processes: %d\n", nproc - nstop - Have_claim + Skip_claim);
    verbose_flags.Clean();
    variable::DumpGlobals();
    for (X_lst = run_lst; X_lst; X_lst = X_lst->next)
      PrintExecutionDetails(X_lst);
    verbose_flags.Activate();
  }
  printf("%d process%s created\n", nproc - Have_claim + Skip_claim,
         (nproc != 1) ? "es" : "");
short_cut:
  if (launch_settings.need_save_trail)
    MainProcessor::Exit(0);
  if (fini)
    MainProcessor::Exit(1);
}

// InitializeClaimExecution
void InitializeClaimExecution(int n) {
  models::ProcList *p;
  models::RunList *r, *q = (models::RunList *)0;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  for (p = ready; p; p = p->next)
    if (p->tn == n && p->b == models::btypes::N_CLAIM) {
      InitializeRunnableProcess(p, 1, 1);
      goto found;
    }
  printf("spin++: couldn't find claim %d (ignored)\n", n);
  if (verbose_flags.NeedToPrintVerbose()) {
    for (p = ready; p; p = p->next) {
      printf("\t%d = %s\n", p->tn, p->n->name.c_str());
    }
  }

  Skip_claim = 1;
  goto done;
found:
  if (run_lst->pid == 0)
    return; /* it is the first process started */

  q = run_lst;
  run_lst = run_lst->next;
  q->pid = 0;
  q->next = (models::RunList *)0; /* remove */
done:
  Have_claim = 1;
  for (r = run_lst; r; r = r->next) {
    r->pid = r->pid + Have_claim; /* adjust */
    if (!r->next) {
      r->next = q;
      break;
    }
  }
}

void ValidateParameterCount(int i, models::Lextok *m) {
  models::ProcList *p;
  models::Symbol *s = m->symbol; /* proctype name */
  models::Lextok *f, *t;         /* formal pars */
  int count = 0;

  for (p = ready; p; p = p->next) {
    if (s->name == p->n->name) {
      if (m->left) /* actual param list */
      {
        file::LineNumber::Set(m->left->line_number);
        Fname = m->left->file_name;
      }
      for (f = p->p; f; f = f->right)      /* one type at a time */
        for (t = f->left; t; t = t->right) /* count formal params */
        {
          count++;
        }
      if (i != count) {
        printf("spin++: saw %d parameters, expected %d\n", i, count);
        loger::non_fatal("wrong number of parameters");
      }
      break;
    }
  }
}

int ActivateProcess(models::Lextok *m) {
  models::ProcList *p;
  models::Symbol *s = m->symbol; /* proctype name */
  models::Lextok *n = m->left;   /* actual parameters */

  if (m->value < 1) {
    m->value = 1; /* minimum priority */
  }
  for (p = ready; p; p = p->next) {
    if (s->name == p->n->name) {
      if (nproc - nstop >= kMaxNrOfProcesses) {
        printf("spin++: too many processes (%d max)\n", kMaxNrOfProcesses);
        break;
      }
      InitializeRunnableProcess(p, m->value, 0);
      sched::DisplayProcessCreation(std::string{});
      SetProcedureParameters(run_lst, p, n);
      InitializeLocalSymbols(run_lst); /* after SetProcedureParameters */
      ValidateMTypeArguments(m, m->left);
      return run_lst->pid - Have_claim + Skip_claim; /* effective simu pid */
    }
  }
  return 0; /* process not found */
}

void DisplayProcessCreation(const std::string &w) {
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  if (launch_settings.need_columnated_output) {

    firstrow = 1;

    printf("proc %d = %s\n", run_lst->pid - Have_claim,
           run_lst->n->name.c_str());
    return;
  }

  if (launch_settings.need_produce_symbol_table_information ||
      launch_settings.need_to_analyze ||
      launch_settings.need_compute_synchronous_product_multiple_never_claims ||
      launch_settings.need_save_trail ||
      !verbose_flags.NeedToPrintAllProcessActions())
    return;

  if (!w.empty())
    printf("  0:	proc  - (%s) ", w.c_str());
  else
    PrintCurrentProcessInfo(1);
  printf("creates proc %2d (%s)", run_lst->pid - Have_claim,
         run_lst->n->name.c_str());
  if (run_lst->priority > 1)
    printf(" priority %d", run_lst->priority);
  printf("\n");
}

int IncrementSymbolMaxElement(models::Symbol *s) {
  models::ProcList *p;

  for (p = ready; p; p = p->next) {
    if (p->n == s) {
      return p->s->maxel++;
    }
  }

  return Elcnt++;
}

void ValidateMTypeArguments(
    models::Lextok *pnm,
    models::Lextok *args) /* proctype name, actual params */
{
  models::ProcList *p = nullptr;
  models::Lextok *fp, *fpt, *at;
  std::string s, t;

  if (pnm && pnm->symbol) {
    for (p = ready; p; p = p->next) {
      if (pnm->symbol->name == p->n->name) { /* found */
        break;
      }
    }
  }

  if (!p) {
    std::string pnm_sym_name_default = "?";
    loger::fatal("cannot find proctype '%s'",
                 (pnm && pnm->symbol) ? pnm->symbol->name
                                      : pnm_sym_name_default.c_str());
  }

  for (fp = p->p, at = args; fp; fp = fp->right)
    for (fpt = fp->left; at && fpt; fpt = fpt->right, at = at->right) {
      if (fp->left->value != MTYPE) {
        continue;
      }
      if (!at->left->symbol) {
        printf("spin++:%d unrecognized mtype value\n", pnm->line_number);
        continue;
      }
      s = "_unnamed_";
      if (fp->left->symbol->mtype_name) {
        t = fp->left->symbol->mtype_name->name;
      } else {
        t = "_unnamed_";
      }
      if (at->left->node_type != CONST) {
        loger::fatal("wrong arg type '%s'", at->left->symbol->name);
      }
      s = symbol::GetMtypeName(at->left->symbol->name);
      if (s != "" && s != t) {
        printf(
            "spin++: %s:%d, Error: '%s' is type '%s', but should be type '%s'\n",
            pnm->file_name->name.c_str(), pnm->line_number,
            at->left->symbol->name.c_str(), s.c_str(), t.c_str());
        loger::fatal("wrong arg type '%s'", at->left->symbol->name);
      }
    }
}

models::ProcList *CreateProcessEntry(models::Symbol *n, models::Lextok *p,
                                     models::Sequence *s, int det,
                                     models::Lextok *prov, models::btypes b)
/* n=name, p=formals, s=body det=deterministic prov=provided */
{
  models::ProcList *r = (models::ProcList *)emalloc(sizeof(models::ProcList));
  models::Lextok *fp, *fpt;
  int j;

  r->n = n;
  r->p = p;
  r->s = s;
  r->b = b;
  r->prov = prov;
  r->tn = (short)nrRdy++;
  n->sc = lexer::ScopeProcessor::GetCurrSegment(); /* scope_level should be 0 */

  if (det != 0 && det != 1) {
    fprintf(stderr, "spin++: bad value for det (cannot happen)\n");
  }
  r->det = (unsigned char)det;
  r->next = ready;
  ready = r;

  for (fp = p, j = 0; fp; fp = fp->right)
    for (fpt = fp->left; fpt; fpt = fpt->right) {
      j++; /* count # of parameters */
    }
  Npars = max(Npars, j);

  return ready;
}

void InitializeRunnableProcess(models::ProcList *p, int weight, int noparams) {
  models::RunList *r = (models::RunList *)emalloc(sizeof(models::RunList));
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  r->n = p->n;
  r->tn = p->tn;
  r->b = p->b;
  r->pid = nproc++ - nstop + Skip_claim;
  r->priority = weight;
  p->priority = (unsigned char)weight; /* not quite the best place of course */

  if (!noparams && (verbose_flags.NeedToPrintAllProcessActions() ||
                    verbose_flags.NeedToPrintVerbose())) {
    printf("Starting %s with pid %d", p->n ? p->n->name.c_str() : "--", r->pid);
    if (lexer_.GetHasPriority())
      printf(" priority %d", r->priority);
    printf("\n");
  }
  if (!p->s) {
    std::string p_n_name = "--";
    loger::fatal("parsing error, no sequence %s", p->n ? p->n->name : p_n_name);
  }

  r->pc = huntele(p->s->frst, p->s->frst->status, -1);
  r->ps = p->s;

  if (p->s->last)
    p->s->last->status |= ENDSTATE; /* normal end state */

  r->next = run_lst;
  r->prov = p->prov;
  if (weight < 1 || weight > 255) {
    loger::fatal("bad process priority, valid range: 1..255");
  }

  if (noparams)
    InitializeLocalSymbols(r);
  Priority_Sum += weight;

  run_lst = r;
}

void DoTag(FILE *fd, char *s) {
  int i = (!strncmp(s, "MSC: ", 5)) ? 5 : 0;
  int pid = launch_settings.need_save_trail ? (prno - Have_claim)
                                            : (X_lst ? X_lst->pid : 0);

  if (pid < 0) {
    pid = 0;
  }

  if (!launch_settings.need_to_tabs) {
    printf("  ");
    for (i = 0; i <= pid; i++) {
      printf("    ");
    }
  }
  fprintf(fd, "%s", s);
  fflush(fd);
}
} // namespace sched
