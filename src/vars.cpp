/***** spin: vars.c *****/

#include "fatal/fatal.hpp"
#include "main/launch_settings.hpp"
#include "models/symbol.hpp"
#include "spin.hpp"
#include "utils/verbose/verbose.hpp"
#include "y.tab.h"
#include <fmt/core.h>
#include <iostream>
extern LaunchSettings launch_settings;

extern char GBuf[];
extern int nproc, nstop;
extern int lineno, depth, verbose, limited_vis, Pid_nr;
extern Lextok *Xu_List;
extern Ordered *all_names;
extern RunList *X_lst, *LastX;
extern short no_arrays, Have_claim, terse;
extern models::Symbol *Fname;

extern void sr_buf(int, int, const std::string &);
extern void sr_mesg(FILE *, int, int, const std::string &);

static int getglobal(Lextok *);
static int setglobal(Lextok *, int);
static int maxcolnr = 1;

int getval(Lextok *sn) {
  models::Symbol *s = sn->sym;

  if (s->name == "_") {
    loger::non_fatal("attempt to read value of '_'");
    return 0;
  }
  if (s->name == "_last")
    return (LastX) ? LastX->pid : 0;
  if (s->name == "_p")
    return (X_lst && X_lst->pc) ? X_lst->pc->seqno : 0;
  if (s->name == "_pid") {
    if (!X_lst)
      return 0;
    return X_lst->pid - Have_claim;
  }
  if (s->name == "_priority") {
    if (!X_lst)
      return 0;

    if (launch_settings.need_revert_old_rultes_for_priority) {
      loger::non_fatal("cannot refer to _priority with -o6");
      return 1;
    }
    return X_lst->priority;
  }

  if (s->name == "_nr_pr") {
    return nproc - nstop; /* new 3.3.10 */
  }

  if (s->context && s->type) {
    return getlocal(sn);
  }

  if (!s->type) /* not declared locally */
  {
    s = lookup(s->name); /* try global */
    sn->sym = s;         /* fix it */
  }

  return getglobal(sn);
}

int setval(Lextok *v, int n) {
  if (v->sym->name == "_last" || v->sym->name == "_p" ||
      v->sym->name == "_pid" || v->sym->name == "_nr_qs" ||
      v->sym->name == "_nr_pr") {
    loger::non_fatal("illegal assignment to %s", v->sym->name.c_str());
  }
  if (v->sym->name == "_priority") {
    if (launch_settings.need_revert_old_rultes_for_priority) {
      loger::non_fatal("cannot refer to _priority with -o6");
      return 1;
    }
    if (!X_lst) {
      loger::non_fatal("no context for _priority");
      return 1;
    }
    X_lst->priority = n;
  }

  if (v->sym->context && v->sym->type)
    return setlocal(v, n);
  if (!v->sym->type)
    v->sym = lookup(v->sym->name);
  return setglobal(v, n);
}

void rm_selfrefs(models::Symbol *s, Lextok *i) {
  if (!i)
    return;

  if (i->ntyp == NAME && i->sym->name == s->name &&
      ((!i->sym->context && !s->context) ||
       (i->sym->context && s->context &&
        i->sym->context->name == s->context->name))) {
    lineno = i->ln;
    Fname = i->fn;
    loger::non_fatal("self-reference initializing '%s'", s->name);
    i->ntyp = CONST;
    i->val = 0;
  } else {
    rm_selfrefs(s, i->lft);
    rm_selfrefs(s, i->rgt);
  }
}

int checkvar(models::Symbol *s, int n) {
  int i, oln = lineno; /* calls on eval() change it */
  models::Symbol *ofnm = Fname;
  Lextok *z, *y;

  if (!in_bound(s, n))
    return 0;

  if (s->type == 0) {
    loger::non_fatal("undecl var %s (assuming int)", s->name.c_str());
    s->type = models::kInt;
  }
  /* not a STRUCT */
  if (s->value.empty()) /* uninitialized */
  {
    s->value.resize(s->value_type);
    z = s->init_value;
    for (i = 0; i < s->value_type; i++) {
      if (z && z->ntyp == ',') {
        y = z->lft;
        z = z->rgt;
      } else {
        y = z;
      }
      if (s->type != models::kChan) {
        rm_selfrefs(s, y);
        s->value[i] = eval(y);
      } else if (!launch_settings.need_to_analyze) {
        s->value[i] = qmake(s);
      }
    }
  }
  lineno = oln;
  Fname = ofnm;

  return 1;
}

static int getglobal(Lextok *sn) {
  models::Symbol *s = sn->sym;
  int i, n = eval(sn->lft);

  if (s->type == 0 && X_lst && (i = find_lab(s, X_lst->n, 0))) /* getglobal */
  {
    std::cout << fmt::format("findlab through getglobal on {}", s->name)
              << std::endl;
    return i; /* can this happen? */
  }
  if (s->type == STRUCT) {
    return Rval_struct(sn, s, 1); /* 1 = check init */
  }
  if (checkvar(s, n)) {
    return cast_val(s->type, s->value[n], (int)s->nbits.value_or(0));
  }
  return 0;
}

int cast_val(int t, int v, int w) {
  int i = 0;
  short s = 0;
  unsigned int u = 0;

  if (t == PREDEF || t == INT || t == CHAN)
    i = v; /* predef means _ */
  else if (t == SHORT)
    s = (short)v;
  else if (t == BYTE || t == MTYPE)
    u = (unsigned char)v;
  else if (t == BIT)
    u = (unsigned char)(v & 1);
  else if (t == UNSIGNED) {
    if (w == 0)
      loger::fatal("cannot happen, cast_val");
    /*	u = (unsigned)(v& ((1<<w)-1));		problem when w=32	*/
    u = (unsigned)(v & (~0u >> (8 * sizeof(unsigned) - w))); /* doug */
  }

  if (v != i + s + (int)u) {
    char buf[64];
    sprintf(buf, "%d->%d (%d)", v, i + s + (int)u, t);
    loger::non_fatal("value (%s) truncated in assignment", buf);
  }
  return (int)(i + s + (int)u);
}

static int setglobal(Lextok *v, int m) {
  if (v->sym->type == STRUCT) {
    (void)Lval_struct(v, v->sym, 1, m);
  } else {
    int n = eval(v->lft);
    if (checkvar(v->sym, n)) {
      int oval = v->sym->value[n];
      int nval = cast_val((int)v->sym->type, m, v->sym->nbits.value_or(0));
      v->sym->value[n] = nval;
      if (oval != nval) {
        v->sym->last_depth = depth;
      }
    }
  }
  return 1;
}

void dumpclaims(FILE *fd, int pid, const std::string &s) {
  Lextok *m;
  int cnt = 0;
  int oPid = Pid_nr;

  for (m = Xu_List; m; m = m->rgt)
    if (m->sym->name == s) {
      cnt = 1;
      break;
    }
  if (cnt == 0)
    return;

  Pid_nr = pid;
  fprintf(fd, "#ifndef XUSAFE\n");
  for (m = Xu_List; m; m = m->rgt) {
    if (m->sym->name != s)
      continue;
    no_arrays = 1;
    putname(fd, "\t\tsetq_claim(", m->lft, 0, "");
    no_arrays = 0;
    fprintf(fd, ", %d, ", m->val);
    terse = 1;
    putname(fd, "\"", m->lft, 0, "\", h, ");
    terse = 0;
    fprintf(fd, "\"%s\");\n", s.c_str());
  }
  fprintf(fd, "#endif\n");
  Pid_nr = oPid;
}

void dumpglobals(void) {
  Ordered *walk;
  static Lextok *dummy = ZN;
  models::Symbol *sp;
  int j;
  auto &verbose_flags = utils::verbose::Flags::getInstance();
  if (!dummy)
    dummy = nn(ZN, NAME, nn(ZN, CONST, ZN, ZN), ZN);

  for (walk = all_names; walk; walk = walk->next) {
    sp = walk->entry;
    if (!sp->type || sp->context || sp->owner_name || sp->type == PROCTYPE ||
        sp->type == PREDEF || sp->type == CODE_FRAG || sp->type == CODE_DECL ||
        (sp->type == MTYPE && ismtype(sp->name)))
      continue;

    if (sp->type == STRUCT) {
      if (verbose_flags.NeedToPrintAllProcessActions() &&
          !verbose_flags.NeedToPrintVeryVerbose() &&
          (sp->last_depth < depth &&
           launch_settings.count_of_skipping_steps != depth)) {
        continue;
      }
      dump_struct(sp, sp->name, 0);
      continue;
    }
    for (j = 0; j < sp->value_type; j++) {
      int prefetch;
      std::string s;
      if (sp->type == CHAN) {
        doq(sp, j, 0);
        continue;
      }
      if (verbose_flags.NeedToPrintAllProcessActions() &&
          !verbose_flags.NeedToPrintVeryVerbose() &&
          (sp->last_depth < depth &&
           launch_settings.count_of_skipping_steps != depth)) {
        continue;
      }

      dummy->sym = sp;
      dummy->lft->val = j;
      /* in case of cast_val warnings, do this first: */
      prefetch = getglobal(dummy);
      printf("\t\t%s", sp->name.c_str());
      if (sp->value_type > 1 || sp->is_array)
        printf("[%d]", j);
      printf(" = ");
      if (sp->type == MTYPE && sp->mtype_name) {
        s = sp->mtype_name->name;
      }
      sr_mesg(stdout, prefetch, sp->type == MTYPE, s);
      printf("\n");
      if (limited_vis && (sp->hidden_flags & 2)) {
        int colpos;
        GBuf[0] = '\0';
        if (launch_settings.need_generate_mas_flow_tcl_tk)
          sprintf(GBuf, "~G%s = ", sp->name.c_str());
        else
          sprintf(GBuf, "%s = ", sp->name.c_str());
        sr_buf(prefetch, sp->type == MTYPE, s);
        if (sp->color_number == 0) {
          sp->color_number = (unsigned char)maxcolnr;
          maxcolnr = 1 + (maxcolnr % 10);
        }
        colpos = nproc + sp->color_number - 1;
        if (launch_settings.need_generate_mas_flow_tcl_tk) {
          pstext(colpos, GBuf);
          continue;
        }
        printf("\t\t%s\n", GBuf);
        continue;
      }
    }
  }
}

void dumplocal(RunList *r, int final) {
  static Lextok *dummy = ZN;
  models::Symbol *z, *s;
  int i;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  if (!r)
    return;

  s = r->symtab;

  if (!dummy) {
    dummy = nn(ZN, NAME, nn(ZN, CONST, ZN, ZN), ZN);
  }

  for (z = s; z; z = z->next) {
    if (z->type == STRUCT) {
      dump_struct(z, z->name, r);
      continue;
    }
    for (i = 0; i < z->value_type; i++) {
      std::string t;
      if (z->type == CHAN) {
        doq(z, i, r);
        continue;
      }

      if (verbose_flags.NeedToPrintAllProcessActions() &&
          !verbose_flags.NeedToPrintVeryVerbose() && !final &&
          (z->last_depth < depth &&
           launch_settings.count_of_skipping_steps != depth)) {
        continue;
      }

      dummy->sym = z;
      dummy->lft->val = i;

      printf("\t\t%s(%d):%s", r->n->name.c_str(), r->pid - Have_claim,
             z->name.c_str());
      if (z->value_type > 1 || z->is_array)
        printf("[%d]", i);
      printf(" = ");

      if (z->type == MTYPE && z->mtype_name) {
        t = z->mtype_name->name;
      }
      sr_mesg(stdout, getval(dummy), z->type == MTYPE, t);
      printf("\n");
      if (limited_vis && (z->hidden_flags & 2)) {
        int colpos;
        GBuf[0] = '\0';
        if (launch_settings.need_generate_mas_flow_tcl_tk)
          sprintf(GBuf, "~G%s(%d):%s = ", r->n->name.c_str(), r->pid,
                  z->name.c_str());
        else
          sprintf(GBuf, "%s(%d):%s = ", r->n->name.c_str(), r->pid,
                  z->name.c_str());
        sr_buf(getval(dummy), z->type == MTYPE, t);
        if (z->color_number == 0) {
          z->color_number = (unsigned char)maxcolnr;
          maxcolnr = 1 + (maxcolnr % 10);
        }
        colpos = nproc + z->color_number - 1;
        if (launch_settings.need_generate_mas_flow_tcl_tk) {
          pstext(colpos, GBuf);
          continue;
        }
        printf("\t\t%s\n", GBuf);
        continue;
      }
    }
  }
}
