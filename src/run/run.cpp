#include "run.hpp"

#include "../codegen/codegen.hpp"
#include "../fatal/fatal.hpp"
#include "../lexer/line_number.hpp"
#include "../main/launch_settings.hpp"
#include "../main/main_processor.hpp"
#include "../models/lextok.hpp"
#include "../run/sched.hpp"
#include "../trail/mesg.hpp"
#include "../trail/guided.hpp"
#include "../spin.hpp"
#include "../trail/mesg.hpp"
#include "../utils/seed/seed.hpp"
#include "../utils/verbose/verbose.hpp"
#include "../variable/variable.hpp"
#include "flow.hpp"
#include "y.tab.h"
#include <stdlib.h>
#include <sys/resource.h>

extern models::RunList *X_lst, *run_lst;
extern models::Symbol *Fname;
extern models::Element *LastStep;
extern int Rvous, Tval, MadeChoice, Priority_Sum;
extern int TstOnly, verbose, depth;
extern int nproc, nstop;
extern short Have_claim;
extern char GBuf[]; /* global, size 4096 */
extern LaunchSettings launch_settings;
static int E_Check = 0, Escape_Check = 0;


namespace run {
namespace {

void SetPriority(models::Lextok *n, models::Lextok *p) {
  int i = nproc - nstop - Have_claim;
  int pid = Eval(n);
  models::RunList *Y;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  if (launch_settings.need_revert_old_rultes_for_priority) {
    return;
  }
  for (Y = run_lst; Y; Y = Y->next) {
    if (--i == pid) {
      Priority_Sum -= Y->priority;
      Y->priority = Eval(p);
      Priority_Sum += Y->priority;
      if (1) {
        printf("%3d: setting priority of proc %d (%s) to %d\n", depth, pid,
               Y->n->name.c_str(), Y->priority);
      }
    }
  }
  if (verbose_flags.NeedToPrintVerbose()) {
    printf("\tPid\tName\tPriority\n");
    for (Y = run_lst; Y; Y = Y->next) {
      printf("\t%d\t%s\t%d\n", Y->pid, Y->n->name.c_str(), Y->priority);
    }
  }
}

int GetPriority(models::Lextok *n) {
  int i = nproc - nstop;
  int pid = Eval(n);
  models::RunList *Y;

  if (launch_settings.need_revert_old_rultes_for_priority) {
    return 1;
  }

  for (Y = run_lst; Y; Y = Y->next) {
    if (--i == pid) {
      return Y->priority;
    }
  }
  return 0;
}

int PCEnabled(models::Lextok *n) {
  int i = nproc - nstop;
  int pid = Eval(n);
  int result = 0;
  models::RunList *Y, *oX;

  if (pid == X_lst->pid)
    loger::fatal("used: enabled(pid=thisproc) [%s]", X_lst->n->name);

  for (Y = run_lst; Y; Y = Y->next)
    if (--i == pid) {
      oX = X_lst;
      X_lst = Y;
      result = Enabled(X_lst->pc);
      X_lst = oX;
      break;
    }
  return result;
}

int Enabled(models::Lextok *n) {
  int i;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  if (n)
    switch (n->node_type) {
    case 'c':
      if (has_typ(n->left, RUN))
        return 1; /* conservative */
                  /* else fall through */
    default:      /* side-effect free */
      verbose_flags.Clean();
      E_Check++;
      i = Eval(n);
      E_Check--;
      verbose_flags.Activate();
      return i;

    case SET_P:
    case C_CODE:
    case C_EXPR:
    case PRINT:
    case PRINTM:
    case ASGN:
    case ASSERT:
      return 1;

    case 's':
      if (mesg::QIsSync(n)) {
        if (Rvous)
          return 0;
        TstOnly = 1;
        verbose_flags.Clean();
        E_Check++;
        i = Eval(n);
        E_Check--;
        TstOnly = 0;
        verbose_flags.Activate();
        return i;
      }
      return (!mesg::QFull(n));
    case 'r':
      if (mesg::QIsSync(n))
        return 0; /* it's never a user-choice */
      n->node_type = 'R';
      verbose_flags.Clean();
      E_Check++;
      i = Eval(n);
      E_Check--;
      n->node_type = 'r';
      verbose_flags.Activate();
      return i;
    }
  return 0;
}

int InterPrint(FILE *fd, models::Lextok *n) {
  models::Lextok *tmp = n->left;
  std::string s = n->symbol->name;
  std::string t;
  int i, j;
  char lbuf[512];  /* matches value in sr_buf() */
  char tBuf[4096]; /* match size of global GBuf[] */
  char c;

  GBuf[0] = '\0';
  if (!launch_settings.need_dont_execute_printfs_in_sumulation)
    if (!launch_settings.need_save_trail ||
        depth >= launch_settings.count_of_skipping_steps) {
      for (i = 0; i < s.length(); i++)
        switch (s[i]) {
        case '\"':
          break; /* ignore */
        case '\\':
          switch (s[++i]) {
          case 't':
            strcat(GBuf, "\t");
            break;
          case 'n':
            strcat(GBuf, "\n");
            break;
          default:
            goto onechar;
          }
          break;
        case '%':
          if ((c = s[++i]) == '%') {
            strcat(GBuf, "%"); /* literal */
            break;
          }
          if (!tmp) {
            loger::non_fatal("too few print args %s", s.c_str());
            break;
          }
          j = Eval(tmp->left);

          if (c == 'e' && tmp->left && tmp->left->symbol &&
              tmp->left->symbol->mtype_name) {
            t = tmp->left->symbol->mtype_name->name;
          }

          tmp = tmp->right;
          switch (c) {
          case 'c':
            sprintf(lbuf, "%c", j);
            break;
          case 'd':
            sprintf(lbuf, "%d", j);
            break;

          case 'e':
            strcpy(tBuf, GBuf); /* event name */
            GBuf[0] = '\0';

            mesg::FormatMessage(j, 1, t.c_str());

            strcpy(lbuf, GBuf);
            strcpy(GBuf, tBuf);
            break;

          case 'o':
            sprintf(lbuf, "%o", j);
            break;
          case 'u':
            sprintf(lbuf, "%u", (unsigned)j);
            break;
          case 'x':
            sprintf(lbuf, "%x", j);
            break;
          default:
            loger::non_fatal("bad print cmd: '%s'", &s[i - 1]);
            lbuf[0] = '\0';
            break;
          }
          goto append;
        default:
        onechar:
          lbuf[0] = s[i];
          lbuf[1] = '\0';
        append:
          strcat(GBuf, lbuf);
          break;
        }
      sched::DoTag(fd, GBuf);
    }
  if (strlen(GBuf) >= 4096)
    loger::fatal("printf string too long");
  return 1;
}

int PrintM(FILE *fd, models::Lextok *n) {
  std::string s;
  int j;

  GBuf[0] = '\0';
  if (!launch_settings.need_dont_execute_printfs_in_sumulation) {
    if (!launch_settings.need_save_trail ||
        depth >= launch_settings.count_of_skipping_steps) {
      if (n->left->symbol && n->left->symbol->mtype_name) {
        s = n->left->symbol->mtype_name->name;
      }

      if (n->left->is_mtype_token) {
        j = n->left->value;
      } else /* constant */
      {
        j = Eval(n->left);
      }
      mesg::FormatMessage(j, 1, s.c_str());
      sched::DoTag(fd, GBuf);
    }
  }
  return 1;
}

int EvalSync(models::Element *e) { /* allow only synchronous receives
                                      and related node types    */
  models::Lextok *now = (e) ? e->n : ZN;

  if (!now || now->node_type != 'r' || now->value >= 2 /* no rv with a poll */
      || !mesg::QIsSync(now)) {
    return 0;
  }

  LastStep = e;
  return Eval(now);
}

int Assign(models::Lextok *now) {
  int t;

  if (TstOnly)
    return 1;

  switch (now->right->node_type) {
  case FULL:
  case NFULL:
  case EMPTY:
  case NEMPTY:
  case RUN:
  case LEN:
    t = BYTE;
    break;
  default:
    t = now->right->ResolveSymbolType();
    break;
  }
  mesg::CheckTypeClash(now->left->ResolveSymbolType(), t, "assignment");

  return variable::SetVal(now->left, Eval(now->right));
}

int NonProgress(void) /* np_ */
{
  models::RunList *r;

  for (r = run_lst; r; r = r->next) {
    if (flow::HasLabel(r->pc, 4)) /* 4=progress */
      return 0;
  }
  return 1;
}

models::Element *RevEscape(models::SeqList *e) {
  models::Element *r = (models::Element *)0;

  if (e) {
    if ((r = RevEscape(e->next)) == ZE) /* reversed order */
    {
      r = EvalSub(e->this_sequence->frst);
    }
  }

  return r;
}

} // namespace

int PCHighest(models::Lextok *n) {
  int i = nproc - nstop;
  int pid = Eval(n);
  int target = 0, result = 1;
  models::RunList *Y, *oX;

  if (X_lst->prov && !Eval(X_lst->prov)) {
    return 0; /* can't be highest unless fully enabled */
  }

  for (Y = run_lst; Y; Y = Y->next) {
    if (--i == pid) {
      target = Y->priority;
      break;
    }
  }
  if (0)
    printf("highest for pid %d @ priority = %d\n", pid, target);

  oX = X_lst;
  i = nproc - nstop;
  for (Y = run_lst; Y; Y = Y->next) {
    i--;
    if (0)
      printf("	pid %d @ priority %d\t", Y->pid, Y->priority);
    if (Y->priority > target) {
      X_lst = Y;
      if (0)
        printf("enabled: %s\n", Enabled(X_lst->pc) ? "yes" : "nope");
      if (0)
        printf("provided: %s\n", Eval(X_lst->prov) ? "yes" : "nope");
      if (Enabled(X_lst->pc) && (!X_lst->prov || Eval(X_lst->prov))) {
        result = 0;
        break;
      }
    } else if (0)
      printf("\n");
  }
  X_lst = oX;

  return result;
}

int Enabled(models::Element *e) {
  models::SeqList *z;

  if (!e || !e->n)
    return 0;

  switch (e->n->node_type) {
  case '@':
    return X_lst->pid == (nproc - nstop - 1);
  case '.':
  case SET_P:
    return 1;
  case GOTO:
    if (Rvous)
      return 0;
    return 1;
  case UNLESS:
    return Enabled(e->sub->this_sequence->frst);
  case ATOMIC:
  case D_STEP:
  case NON_ATOMIC:
    return Enabled(e->n->seq_list->this_sequence->frst);
  }
  if (e->sub) /* true for IF, DO, and UNLESS */
  {
    for (z = e->sub; z; z = z->next)
      if (Enabled(z->this_sequence->frst))
        return 1;
    return 0;
  }
  for (z = e->esc; z; z = z->next) {
    if (Enabled(z->this_sequence->frst))
      return 1;
  }
  return Enabled(e->n);
}

int Eval(models::Lextok *now) {
  int temp;

  if (now) {
    file::LineNumber::Set(now->line_number);
    Fname = now->file_name;
#ifdef DEBUG
    printf("eval ");
    comment(stdout, now, 0);
    printf("\n");
#endif
    switch (now->node_type) {
    case CONST:
      return now->value;
    case '!':
      return !Eval(now->left);
    case UMIN:
      return -Eval(now->left);
    case '~':
      return ~Eval(now->left);

    case '/':
      temp = Eval(now->right);
      if (temp == 0) {
        loger::fatal("division by zero");
      }
      return (Eval(now->left) / temp);
    case '*':
      return (Eval(now->left) * Eval(now->right));
    case '-':
      return (Eval(now->left) - Eval(now->right));
    case '+':
      return (Eval(now->left) + Eval(now->right));
    case '%':
      temp = Eval(now->right);
      if (temp == 0) {
        loger::fatal("taking modulo of zero");
      }
      return (Eval(now->left) % temp);
    case LT:
      return (Eval(now->left) < Eval(now->right));
    case GT:
      return (Eval(now->left) > Eval(now->right));
    case '&':
      return (Eval(now->left) & Eval(now->right));
    case '^':
      return (Eval(now->left) ^ Eval(now->right));
    case '|':
      return (Eval(now->left) | Eval(now->right));
    case LE:
      return (Eval(now->left) <= Eval(now->right));
    case GE:
      return (Eval(now->left) >= Eval(now->right));
    case NE:
      return (Eval(now->left) != Eval(now->right));
    case EQ:
      return (Eval(now->left) == Eval(now->right));
    case OR:
      return (Eval(now->left) || Eval(now->right));
    case AND:
      return (Eval(now->left) && Eval(now->right));
    case LSHIFT:
      return (Eval(now->left) << Eval(now->right));
    case RSHIFT:
      return (Eval(now->left) >> Eval(now->right));
    case '?':
      return (Eval(now->left) ? Eval(now->right->left)
                              : Eval(now->right->right));

    case 'p':
      return sched::ResolveRemoteVariableReference(
          now); /* _p for remote reference */
    case 'q':
      return sched::ResolveRemoteLabelReference(now);
    case 'R':
      return mesg::QReceive(now, 0); /* test only    */
    case LEN:
      return mesg::QLen(now);
    case FULL:
      return (mesg::QFull(now));
    case EMPTY:
      return (mesg::QLen(now) == 0);
    case NFULL:
      return (!mesg::QFull(now));
    case NEMPTY:
      return (mesg::QLen(now) > 0);
    case ENABLED:
      if (launch_settings.need_save_trail)
        return 1;
      return PCEnabled(now->left);

    case GET_P:
      return GetPriority(now->left);
    case SET_P:
      SetPriority(now->left->left, now->left->right);
      return 1;

    case EVAL:
      if (now->left->node_type == ',') {
        models::Lextok *fix = now->left;
        do {                        /* new */
          if (Eval(fix->left) == 0) /* usertype6 */
          {
            return 0;
          }
          fix = fix->right;
        } while (fix && fix->node_type == ',');
        return 1;
      }
      return Eval(now->left);

    case PC_VAL:
      return trail::GetProgramCounterValue(now->left);
    case NONPROGRESS:
      return NonProgress();
    case NAME:
      return variable::GetValue(now);

    case TIMEOUT:
      return Tval;
    case RUN:
      return TstOnly ? 1 : sched::ActivateProcess(now);

    case 's':
      return mesg::QSend(now); /* send         */
    case 'r':
      return mesg::QReceive(now, 1); /* receive or poll */
    case 'c':
      return Eval(now->left); /* condition    */
    case PRINT:
      return TstOnly ? 1 : InterPrint(stdout, now);
    case PRINTM:
      return TstOnly ? 1 : PrintM(stdout, now);
    case ASGN:
      if (check_track(now) == STRUCT) {
        return 1;
      }
      return Assign(now);

    case C_CODE:
      if (!launch_settings.need_to_analyze) {
        printf("%s:\t", now->symbol->name.c_str());
        codegen::PlunkInline(stdout, now->symbol->name, 0, 1);
      }
      return 1; /* uninterpreted */

    case C_EXPR:
      if (!!launch_settings.need_to_analyze) {
        printf("%s:\t", now->symbol->name.c_str());
        codegen::PlunkExpr(stdout, now->symbol->name);
        printf("\n");
      }
      return 1; /* uninterpreted */

    case ASSERT:
      if (TstOnly || Eval(now->left))
        return 1;
      loger::non_fatal("assertion violated");
      printf("spin: text of failed assertion: assert(");
      comment(stdout, now->left, 0);
      printf(")\n");
      if (launch_settings.need_save_trail)
        return 1;
      sched::RenameWrapup(1); /* doesn't return */

    case IF:
    case DO:
    case BREAK:
    case UNLESS: /* compound */
    case '.':
      return 1; /* return label for compound */
    case '@':
      return 0; /* stop state */
    case ELSE:
      return 1; /* only hit here in guided trails */

    case ',': /* reached through option -A with array initializer */
    case 0:
      return 0; /* not great, but safe */

    default:
      printf("spin: bad node type %d (run)\n", now->node_type);
      if (launch_settings.need_save_trail)
        printf("spin: trail file doesn't match spec?\n");
      loger::fatal("aborting");
    }
  }
  return 0;
}

models::Element *EvalSub(models::Element *e) {
  models::Element *f, *g;
  models::SeqList *z;
  int i, j, k, only_pos;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  if (!e || !e->n)
    return ZE;
  if (e->n->node_type == GOTO) {
    if (Rvous)
      return ZE;
    LastStep = e;
    f = flow::GetLabel(e->n, 1);
    f = huntele(f, e->status, -1); /* 5.2.3: was missing */
    flow::CrossDsteps(e->n, f->n);
    return f;
  }
  if (e->n->node_type == UNLESS) { /* escapes were distributed into sequence */
    return EvalSub(e->sub->this_sequence->frst);
  } else if (e->sub) /* true for IF, DO, and UNLESS */
  {
    models::Element *has_else = ZE;
    models::Element *bas_else = ZE;
    int nr_else = 0, nr_choices = 0;
    only_pos = -1;

    if (launch_settings.need_to_run_in_interactive_mode && !MadeChoice &&
        !E_Check && !Escape_Check && !(e->status & (D_ATOM)) &&
        depth >= launch_settings.count_of_skipping_steps) {
      printf("Select stmnt (");
      sched::PrintCurrentProcessInfo(0);
      printf(")\n");
      if (nproc - nstop > 1) {
        printf("\tchoice 0: other process\n");
        nr_choices++;
        only_pos = 0;
      }
    }
    for (z = e->sub, j = 0; z; z = z->next) {
      j++;
      if (launch_settings.need_to_run_in_interactive_mode && !MadeChoice &&
          !E_Check && !Escape_Check && !(e->status & (D_ATOM)) &&
          depth >= launch_settings.count_of_skipping_steps &&
          z->this_sequence->frst &&
          (verbose_flags.NeedToPrintVerbose() ||
           Enabled(z->this_sequence->frst))) {
        if (z->this_sequence->frst->n->node_type == ELSE) {
          has_else = (Rvous) ? ZE : z->this_sequence->frst->next;
          nr_else = j;
          continue;
        }
        printf("\tchoice %d: ", j);
        if (!Enabled(z->this_sequence->frst))
          printf("unexecutable, ");
        else {
          nr_choices++;
          only_pos = j;
        }
        comment(stdout, z->this_sequence->frst->n, 0);
        printf("\n");
      }
    }

    if (nr_choices == 0 && has_else) {
      printf("\tchoice %d: (else)\n", nr_else);
      only_pos = nr_else;
    }

    if (nr_choices <= 1 && only_pos != -1 && !MadeChoice) {
      MadeChoice = only_pos;
    }

    if (launch_settings.need_to_run_in_interactive_mode &&
        depth >= launch_settings.count_of_skipping_steps && !Escape_Check &&
        !(e->status & (D_ATOM)) && !E_Check) {
      if (!MadeChoice) {
        char buf[256];
        printf("Select [0-%d]: ", j);
        fflush(stdout);
        if (scanf("%64s", buf) <= 0) {
          printf("no input\n");
          return ZE;
        }
        if (isdigit((int)buf[0]))
          k = atoi(buf);
        else {
          if (buf[0] == 'q') {
            MainProcessor::Exit(1);
          }
          k = -1;
        }
      } else {
        k = MadeChoice;
        MadeChoice = 0;
      }
      if (k < 1 || k > j) {
        if (k != 0)
          printf("\tchoice outside range\n");
        return ZE;
      }
      k--;
    } else {
      if (e->n && e->n->index_step >= 0)
        k = 0; /* select 1st executable guard */
      else {
        k = utils::seed::Seed::Rand() % j; /* nondeterminism */
      }
    }

    has_else = ZE;
    bas_else = ZE;
    for (i = 0, z = e->sub; i < j + k; i++) {
      if (z->this_sequence->frst &&
          z->this_sequence->frst->n->node_type == ELSE) {
        bas_else = z->this_sequence->frst;
        has_else = (Rvous) ? ZE : bas_else->next;
        if (!launch_settings.need_to_run_in_interactive_mode ||
            depth < launch_settings.count_of_skipping_steps || Escape_Check ||
            (e->status & (D_ATOM))) {
          z = (z->next) ? z->next : e->sub;
          continue;
        }
      }
      if (z->this_sequence->frst &&
          ((z->this_sequence->frst->n->node_type == ATOMIC ||
            z->this_sequence->frst->n->node_type == D_STEP) &&
           z->this_sequence->frst->n->seq_list->this_sequence->frst->n
                   ->node_type == ELSE)) {
        bas_else = z->this_sequence->frst->n->seq_list->this_sequence->frst;
        has_else = (Rvous) ? ZE : bas_else->next;
        if (!launch_settings.need_to_run_in_interactive_mode ||
            depth < launch_settings.count_of_skipping_steps || Escape_Check ||
            (e->status & (D_ATOM))) {
          z = (z->next) ? z->next : e->sub;
          continue;
        }
      }
      if (i >= k) {
        if ((f = EvalSub(z->this_sequence->frst)) != ZE)
          return f;
        else if (launch_settings.need_to_run_in_interactive_mode &&
                 depth >= launch_settings.count_of_skipping_steps &&
                 !(e->status & (D_ATOM))) {
          if (!E_Check && !Escape_Check)
            printf("\tunexecutable\n");
          return ZE;
        }
      }
      z = (z->next) ? z->next : e->sub;
    }
    LastStep = bas_else;
    return has_else;
  } else {
    if (e->n->node_type == ATOMIC || e->n->node_type == D_STEP) {
      f = e->n->seq_list->this_sequence->frst;
      g = e->n->seq_list->this_sequence->last;
      g->next = e->next;
      if (!(g = EvalSub(f))) /* atomic guard */
        return ZE;
      return g;
    } else if (e->n->node_type == NON_ATOMIC) {
      f = e->n->seq_list->this_sequence->frst;
      g = e->n->seq_list->this_sequence->last;
      g->next = e->next; /* close it */
      return EvalSub(f);
    } else if (e->n->node_type == '.') {
      if (!Rvous)
        return e->next;
      return EvalSub(e->next);
    } else {
      models::SeqList *x;
      if (!(e->status & (D_ATOM)) && e->esc &&
          verbose_flags.NeedToPrintVerbose()) {
        printf("Stmnt [");
        comment(stdout, e->n, 0);
        printf("] has escape(s): ");
        for (x = e->esc; x; x = x->next) {
          printf("[");
          g = x->this_sequence->frst;
          if (g->n->node_type == ATOMIC || g->n->node_type == NON_ATOMIC)
            g = g->n->seq_list->this_sequence->frst;
          comment(stdout, g->n, 0);
          printf("] ");
        }
        printf("\n");
      }
      if (!launch_settings
               .need_save_trail) /* trail determines selections, new 5.2.5 */
      {
        Escape_Check++;
        if (launch_settings.reverse_eval_order_of_nested_unlesses) {
          if ((g = RevEscape(e->esc)) != ZE) {
            if (verbose_flags.NeedToPrintAllProcessActions()) {
              printf("\tEscape taken (-J) ");
              if (g->n && g->n->file_name)
                printf("%s:%d", g->n->file_name->name.c_str(),
                       g->n->line_number);
              printf("\n");
            }
            Escape_Check--;
            return g;
          }
        } else {
          for (x = e->esc; x; x = x->next) {
            if ((g = EvalSub(x->this_sequence->frst)) != ZE) {
              if (verbose_flags.NeedToPrintAllProcessActions()) {
                printf("\tEscape taken ");
                if (g->n && g->n->file_name)
                  printf("%s:%d", g->n->file_name->name.c_str(),
                         g->n->line_number);
                printf("\n");
              }
              Escape_Check--;
              return g;
            }
          }
        }
        Escape_Check--;
      }
      switch (e->n->node_type) {
      case ASGN:
        if (check_track(e->n) == STRUCT) {
          break;
        }
        /* else fall thru */
      case TIMEOUT:
      case RUN:
      case PRINT:
      case PRINTM:
      case C_CODE:
      case C_EXPR:
      case ASSERT:
      case 's':
      case 'r':
      case 'c':
        /* toplevel statements only */
        LastStep = e;
      default:
        break;
      }
      if (Rvous) {
        return (EvalSync(e)) ? e->next : ZE;
      }
      return (Eval(e->n)) ? e->next : ZE;
    }
  }
  return ZE; /* not reached */
}

} // namespace run
