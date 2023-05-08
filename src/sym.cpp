/***** spin: sym.c *****/

#include "fatal/fatal.hpp"
#include "lexer/lexer.hpp"
#include "lexer/scope.hpp"
#include "main/launch_settings.hpp"
#include "models/symbol.hpp"
#include "spin.hpp"
#include "utils/verbose/verbose.hpp"
#include "y.tab.h"
#include <iostream>

extern LaunchSettings launch_settings;
extern lexer::ScopeProcessor scope_processor_;

extern models::Symbol *Fname, *owner;
extern int lineno, depth, verbose, NamesNotAdded;
extern int has_hidden;
extern short has_xu;

models::Symbol *context = ZS;
Ordered *all_names = (Ordered *)0;
int Nid_nr = 0;

Mtypes_t *Mtypes;
Lextok *runstmnts = ZN;

static Ordered *last_name = (Ordered *)0;
static models::Symbol *symtab[Nhash + 1];

static int samename(models::Symbol *a, models::Symbol *b) {
  if (!a && !b)
    return 1;
  if (!a || !b)
    return 0;
  return a->name != b->name;
}

unsigned int hash(const std::string &s) {
  unsigned int h = 0;

  for (char c : s) {
    h += static_cast<unsigned int>(c);
    h <<= 1;
    if (h & (Nhash + 1))
      h |= 1;
  }
  return h & Nhash;
}

void disambiguate(void) {
  Ordered *walk;
  models::Symbol *sp;
  std::string n, m;

  if (launch_settings.need_old_scope_rules) {
    return;
  }
  /* prepend the scope_prefix to the names */

  for (walk = all_names; walk; walk = walk->next) {
    sp = walk->entry;
    if (sp->type != 0 && sp->type != LABEL && sp->block_scope.size() > 1) {
      if (sp->context != nullptr) {
        m = "_" + std::to_string(sp->context->sc) + "_";
        if (m == sp->block_scope)
          continue;
        /* 6.2.0: only prepend scope for inner-blocks,
           not for top-level locals within a proctype
           this means that you can no longer use the same name
           for a global and a (top-level) local variable
         */
      }

      sp->name = sp->block_scope + sp->name; /* discard the old memory */
    }
  }
}

models::Symbol *lookup(const std::string &s) {
  models::Symbol *sp;
  Ordered *no;
  unsigned int h = hash(s);

  if (launch_settings.need_old_scope_rules) { /* same scope - global refering to
                            global or local to local */
    for (sp = symtab[h]; sp; sp = sp->next) {
      if (sp->name == s && samename(sp->context, context) &&
          samename(sp->owner_name, owner)) {
        return sp; /* found */
      }
    }
  } else { /* added 6.0.0: more traditional, scope rule */
    for (sp = symtab[h]; sp; sp = sp->next) {
      if (sp->name == s && samename(sp->context, context) &&
          (sp->block_scope == scope_processor_.GetCurrScope() ||
           (sp->block_scope.compare(0, sp->block_scope.length(),
                                    scope_processor_.GetCurrScope()) == 0 &&
            samename(sp->owner_name, owner)))) {
        if (!samename(sp->owner_name, owner)) {
          printf("spin: different container %s\n", sp->name.c_str());
          printf("    old: %s\n",
                 sp->owner_name ? sp->owner_name->name.c_str() : "--");
          printf("    new: %s\n", owner ? owner->name.c_str() : "--");
          /*        MainProcessor::Exit(1);    */
        }
        return sp; /* found */
      }
    }
  }

  if (context) /* in proctype, refers to global */
    for (sp = symtab[h]; sp; sp = sp->next) {
      if (sp->name == s.c_str() && !sp->context &&
          samename(sp->owner_name, owner)) {
        return sp; /* global */
      }
    }

  sp = (models::Symbol *)emalloc(sizeof(models::Symbol));
  sp->name += s;
  sp->value_type = 1;
  sp->last_depth = depth;
  sp->context = context;
  sp->owner_name = owner; /* if fld in struct */
  sp->block_scope = scope_processor_.GetCurrScope();

  if (NamesNotAdded == 0) {
    sp->next = symtab[h];
    symtab[h] = sp;
    no = (Ordered *)emalloc(sizeof(Ordered));
    no->entry = sp;
    if (!last_name)
      last_name = all_names = no;
    else {
      last_name->next = no;
      last_name = no;
    }
  }

  return sp;
}

void trackvar(Lextok *n, Lextok *m) {
  models::Symbol *sp = n->sym;

  if (!sp)
    return; /* a structure list */
  switch (m->ntyp) {
  case NAME:
    if (m->sym->type != BIT) {
      sp->hidden_flags |= 4;
      if (m->sym->type != models::SymbolType::kByte)
        sp->hidden_flags |= 8;
    }
    break;
  case CONST:
    if (m->val != 0 && m->val != 1)
      sp->hidden_flags |= 4;
    if (m->val < 0 || m->val > 256)
      sp->hidden_flags |= 8; /* ditto byte-equiv */
    break;
  default:                       /* unknown */
    sp->hidden_flags |= (4 | 8); /* not known bit-equiv */
  }
}

void trackrun(Lextok *n) { runstmnts = nn(ZN, 0, n, runstmnts); }

void checkrun(models::Symbol *parnm, int posno) {
  Lextok *n, *now, *v;
  int i, m;
  int res = 0;
  std::string buf, buf2;
  auto &verbose_flags = utils::verbose::Flags::getInstance();
  for (n = runstmnts; n; n = n->rgt) {
    now = n->lft;
    if (now->sym != parnm->context)
      continue;
    for (v = now->lft, i = 0; v; v = v->rgt, i++)
      if (i == posno) {
        m = v->lft->ntyp;
        if (m == CONST) {
          m = v->lft->val;
          if (m != 0 && m != 1)
            res |= 4;
          if (m < 0 || m > 256)
            res |= 8;
        } else if (m == NAME) {
          m = v->lft->sym->type;
          if (m != BIT) {
            res |= 4;
            if (m != BYTE)
              res |= 8;
          }
        } else
          res |= (4 | 8); /* unknown */
        break;
      }
  }
  if (!(res & 4) || !(res & 8)) {
    if (!verbose_flags.NeedToPrintVerbose())
      return;
    buf = !(res & 4) ? "bit" : "byte";

    sputtype(buf, parnm->type);
    i = buf.length();
    while (i > 0 && buf[--i] == ' ')
      buf[i] = '\0';
    if (i == 0 || buf == buf2)
      return;
    prehint(parnm);
    printf("proctype %s, '%s %s' could be declared",
           parnm->context ? parnm->context->name.c_str() : "", buf.c_str(),
           parnm->name.c_str());
    printf(" '%s %s'\n", buf2.c_str(), parnm->name.c_str());
  }
}

void trackchanuse(Lextok *m, Lextok *w, int t) {
  Lextok *n = m;
  int count = 1;
  while (n) {
    if (n->lft && n->lft->sym && n->lft->sym->type == CHAN)
      setaccess(n->lft->sym, w ? w->sym : ZS, count, t);
    n = n->rgt;
    count++;
  }
}

void setptype(Lextok *mtype_name, Lextok *n, int t,
              Lextok *vis) /* predefined types */
{
  int oln = lineno, cnt = 1;
  extern int Expand_Ok;

  while (n) {
    if (n->sym->type && !(n->sym->hidden_flags & 32)) {
      lineno = n->ln;
      Fname = n->fn;
      loger::fatal("redeclaration of '%s'", n->sym->name);
      lineno = oln;
    }
    n->sym->type = (models::SymbolType)t;

    if (mtype_name && t != MTYPE) {
      lineno = n->ln;
      Fname = n->fn;
      loger::fatal("missing semi-colon after '%s'?", mtype_name->sym->name);
      lineno = oln;
    }

    if (mtype_name && n->sym->mtype_name &&
        mtype_name->sym->name != n->sym->mtype_name->name) {
      fprintf(stderr,
              "spin: %s:%d, Error: '%s' is type '%s' but assigned type '%s'\n",
              n->fn->name.c_str(), n->ln, n->sym->name.c_str(),
              mtype_name->sym->name.c_str(), n->sym->mtype_name->name.c_str());
      loger::non_fatal("type error");
    }

    n->sym->mtype_name =
        mtype_name ? mtype_name->sym : 0; /* if mtype, else 0 */

    if (Expand_Ok) {
      n->sym->hidden_flags |= (4 | 8 | 16); /* formal par */
      if (t == CHAN)
        setaccess(n->sym, ZS, cnt, 'F');
    }

    if (t == UNSIGNED) {
      if (!n->sym->nbits.has_value() || n->sym->nbits.value() >= 32)
        loger::fatal("(%s) has invalid width-field", n->sym->name);
      if (n->sym->nbits.has_value() && n->sym->nbits.value() == 0) {
        n->sym->nbits = 16;
        loger::non_fatal("unsigned without width-field");
      }
    } else if (n->sym->nbits.has_value() && n->sym->nbits.value() > 0) {
      loger::non_fatal("(%s) only an unsigned can have width-field",
                       n->sym->name);
    }

    if (vis) {
      std::string name = vis->sym->name;
      if (name.compare(0, 6, ":hide:") == 0) {
        n->sym->hidden_flags |= 1;
        has_hidden++;
        if (t == BIT)
          loger::fatal("bit variable (%s) cannot be hidden_flags",
                       n->sym->name.c_str());
      } else if (name.compare(0, 6, ":show:") == 0) {
        n->sym->hidden_flags |= 2;
      } else if (name.compare(0, 7, ":local:") == 0) {
        n->sym->hidden_flags |= 64;
      }
    }

    if (t == CHAN) {
      n->sym->id = ++Nid_nr;
    } else {
      n->sym->id = 0;
      if (n->sym->init_value && n->sym->init_value->ntyp == CHAN) {
        Fname = n->fn;
        lineno = n->ln;
        loger::fatal("chan initializer for non-channel %s", n->sym->name);
      }
    }

    if (n->sym->value_type <= 0) {
      lineno = n->ln;
      Fname = n->fn;
      loger::non_fatal("bad array size for '%s'", n->sym->name);
      lineno = oln;
    }

    n = n->rgt;
    cnt++;
  }
}

static void setonexu(models::Symbol *sp, int t) {
  sp->xu |= t;
  if (t == XR || t == XS) {
    if (sp->xup[t - 1] && sp->xup[t - 1]->name != context->name) {
      printf("error: x[rs] claims from %s and %s\n",
             sp->xup[t - 1]->name.c_str(), context->name.c_str());
      loger::non_fatal("conflicting claims on chan '%s'", sp->name.c_str());
    }
    sp->xup[t - 1] = context;
  }
}

static void setallxu(Lextok *n, int t) {
  Lextok *fp, *tl;

  for (fp = n; fp; fp = fp->rgt)
    for (tl = fp->lft; tl; tl = tl->rgt) {
      if (tl->sym->type == STRUCT)
        setallxu(tl->sym->struct_template, t);
      else if (tl->sym->type == CHAN)
        setonexu(tl->sym, t);
    }
}

Lextok *Xu_List = (Lextok *)0;

void setxus(Lextok *p, int t) {
  Lextok *m, *n;

  has_xu = 1;

  if (launch_settings.need_lose_msgs_sent_to_full_queues && t == XS) {
    printf(
        "spin: %s:%d, warning, xs tag not compatible with -m (message loss)\n",
        (p->fn != NULL) ? p->fn->name.c_str() : "stdin", p->ln);
  }

  if (!context) {
    lineno = p->ln;
    Fname = p->fn;
    loger::fatal("non-local x[rs] assertion");
  }
  for (m = p; m; m = m->rgt) {
    Lextok *Xu_new = (Lextok *)emalloc(sizeof(Lextok));
    Xu_new->uiid = p->uiid;
    Xu_new->val = t;
    Xu_new->lft = m->lft;
    Xu_new->sym = context;
    Xu_new->rgt = Xu_List;
    Xu_List = Xu_new;

    n = m->lft;
    if (n->sym->type == STRUCT)
      setallxu(n->sym->struct_template, t);
    else if (n->sym->type == CHAN)
      setonexu(n->sym, t);
    else {
      int oln = lineno;
      lineno = n->ln;
      Fname = n->fn;
      loger::non_fatal("xr or xs of non-chan '%s'", n->sym->name);
      lineno = oln;
    }
  }
}

Lextok **find_mtype_list(const std::string &s) {
  Mtypes_t *lst;

  for (lst = Mtypes; lst; lst = lst->nxt) {
    if (lst->nm == s) {
      return &(lst->mt);
    }
  }

  /* not found, create it */
  lst = (Mtypes_t *)emalloc(sizeof(Mtypes_t));
  lst->nm = s;
  lst->nxt = Mtypes;
  Mtypes = lst;
  return &(lst->mt);
}

void setmtype(Lextok *mtype_name, Lextok *m) {
  Lextok **mtl; /* mtype list */
  Lextok *n, *Mtype;
  int cnt, oln = lineno;
  std::string s = "_unnamed_";

  if (m) {
    lineno = m->ln;
    Fname = m->fn;
  }

  if (mtype_name && mtype_name->sym) {
    s = mtype_name->sym->name;
  }

  mtl = find_mtype_list(s);
  Mtype = *mtl;

  if (!Mtype) {
    *mtl = Mtype = m;
  } else {
    for (n = Mtype; n->rgt; n = n->rgt) {
      ;
    }
    n->rgt = m; /* concatenate */
  }

  for (n = Mtype, cnt = 1; n; n = n->rgt, cnt++) /* syntax check */
  {
    if (!n->lft || !n->lft->sym || n->lft->ntyp != NAME ||
        n->lft->lft) /* indexed variable */
      loger::fatal("bad mtype definition");

    /* label the name */
    if (n->lft->sym->type != models::SymbolType::kMtype) {
      n->lft->sym->hidden_flags |= 128; /* is used */
      n->lft->sym->type = models::SymbolType::kMtype;
      n->lft->sym->init_value = nn(ZN, CONST, ZN, ZN);
      n->lft->sym->init_value->val = cnt;
    } else if (n->lft->sym->init_value->val != cnt) {
      loger::non_fatal("name %s appears twice in mtype declaration",
                       n->lft->sym->name);
    }
  }

  lineno = oln;
  if (cnt > 256) {
    loger::fatal("too many mtype elements (>255)");
  }
}

std::string which_mtype(
    const std::string &str) /* which mtype is str, 0 if not an mtype at all  */
{
  Mtypes_t *lst;
  Lextok *n;

  for (lst = Mtypes; lst; lst = lst->nxt) {
    for (n = lst->mt; n; n = n->rgt) {
      if (str == n->lft->sym->name) {
        return lst->nm;
      }
    }
  }

  return (char *)0;
}

int ismtype(const std::string &str) /* name to number */
{
  Mtypes_t *lst;
  Lextok *n;
  int count;

  for (lst = Mtypes; lst; lst = lst->nxt) {
    count = 1;
    for (n = lst->mt; n; n = n->rgt) {
      if (str == std::string(n->lft->sym->name)) {
        return count;
      }
      count++;
    }
  }

  return 0;
}

int sputtype(std::string &foo, int m) {
  switch (m) {
  case UNSIGNED:
    foo.append("unsigned ");
    break;
  case BIT:
    foo.append("bit   ");
    break;
  case BYTE:
    foo.append("byte  ");
    break;
  case CHAN:
    foo.append("chan  ");
    break;
  case SHORT:
    foo.append("short ");
    break;
  case INT:
    foo.append("int   ");
    break;
  case MTYPE:
    foo.append("mtype ");
    break;
  case STRUCT:
    foo.append("struct");
    break;
  case PROCTYPE:
    foo.append("proctype");
    break;
  case LABEL:
    foo.append("label ");
    return 0;
  default:
    foo.append("value ");
    return 0;
  }
  return 1;
}

static int puttype(int m) {
  std::string buf;
  if (sputtype(buf, m)) {
    std::cout << buf;
    return 1;
  }
  return 0;
}

void symvar(models::Symbol *sp) {
  Lextok *m;

  if (!puttype(sp->type))
    return;

  printf("\t");
  if (sp->owner_name)
    printf("%s.", sp->owner_name->name.c_str());
  printf("%s", sp->name.c_str());
  if (sp->value_type > 1 || sp->is_array == 1)
    printf("[%d]", sp->value_type);

  if (sp->type == CHAN)
    printf("\t%d", (sp->init_value) ? sp->init_value->val : 0);
  else if (sp->type == STRUCT &&
           sp->struct_name != nullptr) /* Frank Weil, 2.9.8 */
    printf("\t%s", sp->struct_name->name.c_str());
  else
    printf("\t%d", eval(sp->init_value));

  if (sp->owner_name)
    printf("\t<:struct-field:>");
  else if (!sp->context)
    printf("\t<:global:>");
  else
    printf("\t<%s>", sp->context->name.c_str());

  if (sp->id < 0) /* formal parameter */
    printf("\t<parameter %d>", -(sp->id));
  else if (sp->type == models::SymbolType::kMtype)
    printf("\t<constant>");
  else if (sp->is_array)
    printf("\t<array>");
  else
    printf("\t<variable>");

  if (sp->type == CHAN && sp->init_value) {
    int i;
    for (m = sp->init_value->rgt, i = 0; m; m = m->rgt)
      i++;
    printf("\t%d\t", i);
    for (m = sp->init_value->rgt; m; m = m->rgt) {
      if (m->ntyp == STRUCT)
        printf("struct %s", m->sym->name.c_str());
      else
        (void)puttype(m->ntyp);
      if (m->rgt)
        printf("\t");
    }
  }

  if (!launch_settings.need_old_scope_rules) {
    printf("\t{scope %s}", sp->block_scope.c_str());
  }

  printf("\n");
}

void symdump(void) {
  Ordered *walk;

  for (walk = all_names; walk; walk = walk->next)
    symvar(walk->entry);
}

void chname(models::Symbol *sp) {
  printf("chan ");
  if (sp->context)
    printf("%s-", sp->context->name.c_str());
  if (sp->owner_name)
    printf("%s.", sp->owner_name->name.c_str());
  printf("%s", sp->name.c_str());
  if (sp->value_type > 1 || sp->is_array == 1)
    printf("[%d]", sp->value_type);
  printf("\t");
}

static struct X_lkp {
  int typ;
  std::string nm;
} xx[] = {
    {'A', "exported as run parameter"},
    {'F', "imported as proctype parameter"},
    {'L', "used as l-value in asgnmnt"},
    {'V', "used as r-value in asgnmnt"},
    {'P', "polled in receive stmnt"},
    {'R', "used as parameter in receive stmnt"},
    {'S', "used as parameter in send stmnt"},
    {'r', "received from"},
    {'s', "sent to"},
};

static void chan_check(models::Symbol *sp) {
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  Access *a;
  int i, b = 0, d;

  if (verbose_flags.NeedToPrintGlobalVariables())
    goto report; /* -C -g */

  for (a = sp->access; a; a = a->lnk)
    if (a->typ == 'r')
      b |= 1;
    else if (a->typ == 's')
      b |= 2;
  if (b == 3 || (sp->hidden_flags & 16)) /* balanced or formal par */
    return;
report:
  chname(sp);
  for (i = d = 0; i < (int)(sizeof(xx) / sizeof(struct X_lkp)); i++) {
    b = 0;
    for (a = sp->access; a; a = a->lnk) {
      if (a->typ == xx[i].typ) {
        b++;
      }
    }
    if (b == 0) {
      continue;
    }
    d++;
    printf("\n\t%s by: ", xx[i].nm.c_str());
    for (a = sp->access; a; a = a->lnk)
      if (a->typ == xx[i].typ) {
        printf("%s", a->who->name.c_str());
        if (a->what)
          printf(" to %s", a->what->name.c_str());
        if (a->cnt)
          printf(" par %d", a->cnt);
        if (--b > 0)
          printf(", ");
      }
  }
  printf("%s\n", (!d) ? "\n\tnever used under this name" : "");
}
void chanaccess(void) {
  Ordered *walk;
  std::string buf;
  extern lexer::Lexer lexer_;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  for (walk = all_names; walk; walk = walk->next) {
    if (!walk->entry->owner_name)
      switch (walk->entry->type) {
      case models::SymbolType::kChan: {
        if (launch_settings.need_print_channel_access_info) {
          chan_check(walk->entry);
        }
        break;
      }
      case models::SymbolType::kMtype:
      case models::SymbolType::kBit:
      case models::SymbolType::kByte:
      case models::SymbolType::kShort:
      case models::SymbolType::kInt:
      case models::SymbolType::kUnsigned:
        if ((walk->entry->hidden_flags & 128)) /* was: 32 */
          continue;

        if (!launch_settings.separate_version && !walk->entry->context &&
            !lexer_.GetHasCode() &&
            launch_settings.need_hide_write_only_variables) {
          walk->entry->hidden_flags |= 1; /* auto-hide */
        }
        if (!verbose_flags.NeedToPrintVerbose() || lexer_.GetHasCode())
          continue;

        printf("spin: %s:0, warning, ", Fname->name.c_str());
        sputtype(buf, walk->entry->type);
        if (walk->entry->context) {
          printf("proctype %s", walk->entry->context->name.c_str());
        } else {
          printf("global");
        }
        printf(", '%s%s' variable is never used (other than in print stmnts)\n",
               buf.c_str(), walk->entry->name.c_str());
      }
  }
}
