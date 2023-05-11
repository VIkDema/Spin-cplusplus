#include "guided.hpp"

#include "../fatal/fatal.hpp"
#include "../lexer/lexer.hpp"
#include "../lexer/line_number.hpp"
#include "../main/launch_settings.hpp"
#include "../main/main_processor.hpp"
#include "../models/lextok.hpp"
#include "../run/run.hpp"
#include "../run/sched.hpp"
#include "../spin.hpp"
#include "../utils/verbose/verbose.hpp"
#include "../variable/variable.hpp"
#include "y.tab.h"
#include <filesystem>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>

extern LaunchSettings launch_settings;

extern models::RunList *run_lst, *X_lst;
extern models::Element *Al_El;
extern models::Symbol *Fname, *oFname;
extern int depth;
extern int nproc, nstop, Tval;
extern short Have_claim, Skip_claim;
extern lexer::Lexer lexer_;
extern void ana_src(int, int);

int TstOnly = 0, prno;

static int lastclaim = -1;
static FILE *fd;

int globmin = INT_MAX;
int globmax = 0;

namespace trail {
namespace {

void PrintProc(int p) {
  models::RunList *oX;

  for (oX = run_lst; oX; oX = oX->next)
    if (oX->pid == p) {
      printf("(%s) ", oX->n->name.c_str());
      break;
    }
}

void ProcessLostTrail() {
  int d, p, n, l;

  while (fscanf(fd, "%d:%d:%d:%d\n", &d, &p, &n, &l) == 4) {
    printf("step %d: proc  %d ", d, p);
    PrintProc(p);
    printf("(state %d) - d %d\n", n, l);
  }
  sched::RenameWrapup(1); /* no return */
}

int IsNotClaimProcess() { return (!Have_claim || !X_lst || X_lst->pid != 0); }

void SetupAtomicElements() {
  models::Element *e;

  for (e = Al_El; e; e = e->Nxt)
    if (e->n && (e->n->node_type == ATOMIC || e->n->node_type == NON_ATOMIC ||
                 e->n->node_type == D_STEP))
      (void)huntstart(e);
}

bool IsFileNewer(const std::string &f1, const std::string &f2) {
  std::filesystem::path path1(f1);
  std::filesystem::path path2(f2);

  if (!std::filesystem::exists(path1))
    return false;
  if (!std::filesystem::exists(path2))
    return true;
  if (std::filesystem::last_write_time(path1) <
      std::filesystem::last_write_time(path2))
    return false;

  return true;
}

} // namespace

void ProcessTrailFile() {
  int i, a, nst;
  models::Element *dothis;

  auto &verbose_flags = utils::verbose::Flags::getInstance();

  if (lexer_.GetHasCode()) {
    printf("spin: important:\n");
    printf("  =======================================warning====\n");
    printf("  this model contains embedded c code statements\n");
    printf("  these statements will not be executed when the trail\n");
    printf("  is replayed in this way -- they are just printed,\n");
    printf("  which will likely lead to inaccurate variable values.\n");
    printf("  for an accurate replay use: ./pan -r\n");
    printf("  =======================================warning====\n\n");
  }

  /*
   * if source model name is leader.pml
   * look for the trail file under these names:
   *	leader.pml.trail
   *	leader.pml.tra
   *	leader.trail
   *	leader.tra
   */
  std::string snap;
  std::string q;

  if (!launch_settings.trail_file_name.empty()) {
    if (launch_settings.trail_file_name.front().length() < snap.capacity()) {
      snap = launch_settings.trail_file_name.front();
    } else {
      loger::fatal("filename %s too long",
                   launch_settings.trail_file_name.front().c_str());
    }
  } else {
    if (launch_settings.nubmer_trail)
      snap = oFname->name + std::to_string(launch_settings.nubmer_trail) +
             ".trail";
    else
      snap = oFname->name + ".trail";
  }

  FILE *fd;
  if ((fd = fopen(snap.c_str(), "r")) == NULL) {
    snap.resize(snap.size() - 2); /* .tra */
    if ((fd = fopen(snap.c_str(), "r")) == NULL) {
      size_t dotPos = oFname->name.find('.');
      if (dotPos != std::string::npos) {
        q = oFname->name.substr(dotPos);
        oFname->name[dotPos] = '\0';
        if (launch_settings.nubmer_trail)
          snap = oFname->name + std::to_string(launch_settings.nubmer_trail) +
                 ".trail";
        else
          snap = oFname->name + ".trail";
        oFname->name[dotPos] = '.';

        if ((fd = fopen(snap.c_str(), "r")) != NULL)
          goto okay;

        snap.resize(snap.size() - 2); /* last try */
        if ((fd = fopen(snap.c_str(), "r")) != NULL)
          goto okay;
      }
      printf("spin: cannot find trail file\n");
      MainProcessor::Exit(1);
    }
  }

  // Rest of the code...

okay:
  if (IsFileNewer(oFname->name, snap)) {
    printf("spin: warning, \"%s\" is IsFileNewer than %s\n",
           oFname->name.c_str(), snap.c_str());
  }
  Tval = 1;

  /*
   * sets Tval because timeouts may be part of trail
   * this used to also set m_loss to 1, but that is
   * better handled with the runtime -m flag
   */

  SetupAtomicElements();

  while (fscanf(fd, "%d:%d:%d\n", &depth, &prno, &nst) == 3) {
    if (depth == -2) {
      if (verbose_flags.Active()) {
        printf("starting claim %d\n", prno);
      }
      sched::InitializeClaimExecution(prno);
      continue;
    }
    if (depth == -4) {
      if (verbose_flags.NeedToPrintVerbose()) {
        printf("using statement merging\n");
      }
      launch_settings.need_statemate_merging = true;
      ana_src(0, 1);
      continue;
    }
    if (depth == -1) {
      sched::DoTag(stdout, "<<<<<START OF CYCLE>>>>>\n");
      continue;
    }
    if (depth <= -5 && depth >= -8) {
      printf("spin: used search permutation, replay with ./pan -r\n");
      return; /* permuted: -5, -6, -7, -8 */
    }

    if (launch_settings.count_of_steps > 0 &&
        depth >= launch_settings.count_of_steps) {
      printf("-------------\n");
      printf("depth-limit (-u%d steps) reached\n",
             launch_settings.count_of_steps);
      break;
    }

    if (Skip_claim && prno == 0)
      continue;

    for (dothis = Al_El; dothis; dothis = dothis->Nxt) {
      if (dothis->Seqno == nst)
        break;
    }
    if (!dothis) {
      printf("%3d: proc %d, no matching stmnt %d\n", depth, prno - Have_claim,
             nst);
      ProcessLostTrail();
    }

    i = nproc - nstop + Skip_claim;

    if (dothis->n->node_type == '@') {
      if (prno == i - 1) {
        run_lst = run_lst->next;
        nstop++;
        if (verbose_flags.NeedToPrintAllProcessActions()) {
          if (Have_claim && prno == 0)
            printf("%3d: claim terminates\n", depth);
          else
            printf("%3d: proc %d terminates\n", depth, prno - Have_claim);
        }
        continue;
      }
      if (prno <= 1)
        continue; /* init dies before never */
      printf("%3d: stop error, ", depth);
      printf("proc %d (i=%d) trans %d, %c\n", prno - Have_claim, i, nst,
             dothis->n->node_type);
      ProcessLostTrail();
    }

    for (X_lst = run_lst; X_lst; X_lst = X_lst->next) {
      if (--i == prno)
        break;
    }

    if (!X_lst) {
      if (verbose_flags.NeedToPrintVerbose()) {
        printf("%3d: no process %d (stmnt %d)\n", depth, prno - Have_claim,
               nst);
        printf(" max %d (%d - %d + %d) claim %d ", nproc - nstop + Skip_claim,
               nproc, nstop, Skip_claim, Have_claim);
        printf("active processes:\n");
        for (X_lst = run_lst; X_lst; X_lst = X_lst->next) {
          printf("\tpid %d\tproctype %s\n", X_lst->pid, X_lst->n->name.c_str());
        }
        printf("\n");
        continue;
      } else {
        printf("%3d:\tproc  %d (?) ", depth, prno);
        ProcessLostTrail();
      }
    } else {
      int min_seq = FindMinSequence(X_lst->ps);
      int max_seq = FindMaxSequence(X_lst->ps);

      if (nst < min_seq || nst > max_seq) {
        printf("%3d: error: invalid statement", depth);
        if (verbose_flags.NeedToPrintVerbose()) {
          printf(": pid %d:%d (%s:%d:%d) stmnt %d (valid range %d .. %d)", prno,
                 X_lst->pid, X_lst->n->name.c_str(), X_lst->tn, X_lst->b, nst,
                 min_seq, max_seq);
        }
        printf("\n");
        continue;
        /* ProcessLostTrail(); */
      }
      X_lst->pc = dothis;
    }
    file::LineNumber::Set(dothis->n->line_number);
    Fname = dothis->n->file_name;

    if (dothis->n->node_type == D_STEP) {
      models::Element *g, *og = dothis;
      do {
        g = run::EvalSub(og);
        if (g && depth >= launch_settings.count_of_skipping_steps &&
            (verbose_flags.NeedToPrintVerbose() ||
             (verbose_flags.NeedToPrintAllProcessActions() &&
              IsNotClaimProcess()))) {
          sched::DisplayExecutionStatus(og, 1);

          if (og->n->node_type == D_STEP)
            og = og->n->seq_list->this_sequence->frst;

          printf("\t[");
          comment(stdout, og->n, 0);
          printf("]\n");
          if (verbose_flags.NeedToPrintGlobalVariables()) {
            variable::DumpGlobals();
          }
          if (verbose_flags.NeedToPrintLocalVariables()) {
            variable::DumpLocal(X_lst, 0);
          }
        }
        og = g;
      } while (g && g != dothis->next);
      if (X_lst != NULL) {
        X_lst->pc = g ? huntele(g, 0, -1) : g;
      }
    } else {
    keepgoing:
      if (dothis->merge_start)
        a = dothis->merge_start;
      else
        a = dothis->merge;

      if (X_lst != NULL) {
        X_lst->pc = run::EvalSub(dothis);
        if (X_lst->pc)
          X_lst->pc = huntele(X_lst->pc, 0, a);
      }

      if (depth >= launch_settings.count_of_skipping_steps &&
          (verbose_flags.NeedToPrintVerbose() ||
           (verbose_flags.NeedToPrintAllProcessActions() &&
            IsNotClaimProcess()))) /* -v or -p */
      {
        sched::DisplayExecutionStatus(dothis, 1);

        if (dothis->n->node_type == D_STEP)
          dothis = dothis->n->seq_list->this_sequence->frst;

        printf("\t[");
        comment(stdout, dothis->n, 0);
        printf("]");
        if (a && verbose_flags.NeedToPrintVerbose()) {
          printf("\t<merge %d now @%d>", dothis->merge,
                 (X_lst && X_lst->pc) ? X_lst->pc->seqno : -1);
        }
        printf("\n");
        if (verbose_flags.NeedToPrintGlobalVariables()) {
          variable::DumpGlobals();
        }
        if (verbose_flags.NeedToPrintLocalVariables()) {
          variable::DumpLocal(X_lst, 0);
        }

        if (X_lst && !X_lst->pc) {
          X_lst->pc = dothis;
          printf("\ttransition failed\n");
          a = 0; /* avoid inf loop */
        }
      }
      if (a && X_lst && X_lst->pc && X_lst->pc->seqno != a) {
        dothis = X_lst->pc;
        goto keepgoing;
      }
    }

    if (Have_claim && X_lst && X_lst->pid == 0 && dothis->n &&
        lastclaim != dothis->n->line_number) {
      lastclaim = dothis->n->line_number;
      printf("Never claim moves to line %d\t[", lastclaim);
      comment(stdout, dothis->n, 0);
      printf("]\n");
    }
  }
  printf("spin: trail ends after %d steps\n", depth);
  sched::RenameWrapup(0);
}

int GetProgramCounterValue(models::Lextok *n) {
  int i = nproc - nstop;
  int pid = run::Eval(n);
  models::RunList *Y;

  for (Y = run_lst; Y; Y = Y->next) {
    if (--i == pid)
      return Y->pc->seqno;
  }
  return 0;
}

int FindMinSequence(models::Sequence *s) {
  models::SeqList *l;
  models::Element *e;

  if (s->minel < 0) {
    s->minel = INT_MAX;
    for (e = s->frst; e; e = e->next) {
      if (e->status & 512) {
        continue;
      }
      e->status |= 512;

      if (e->n->node_type == ATOMIC || e->n->node_type == NON_ATOMIC ||
          e->n->node_type == D_STEP) {
        int n = FindMinSequence(e->n->seq_list->this_sequence);
        if (n < s->minel) {
          s->minel = n;
        }
      } else if (e->Seqno < s->minel) {
        s->minel = e->Seqno;
      }
      for (l = e->sub; l; l = l->next) {
        int n = FindMinSequence(l->this_sequence);
        if (n < s->minel) {
          s->minel = n;
        }
      }
    }
  }
  if (s->minel < globmin) {
    globmin = s->minel;
  }
  return s->minel;
}

int FindMaxSequence(models::Sequence *s) {
  if (s->last->Seqno > globmax) {
    globmax = s->last->Seqno;
  }
  return s->last->Seqno;
}

} // namespace trail
