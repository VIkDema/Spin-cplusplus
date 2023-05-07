/***** spin: structs.c *****/

#include "fatal/fatal.hpp"
#include "lexer/lexer.hpp"
#include "spin.hpp"
#include "y.tab.h"

struct UType {
  models::Symbol *nm; /* name of the type */
  Lextok *cn;         /* contents */
  struct UType *nxt;  /* linked list */
};

extern models::Symbol *Fname;
extern int lineno, depth, Expand_Ok, has_hidden;
extern lexer::Lexer lexer_;

models::Symbol *owner;

static UType *Unames = 0;
static UType *Pnames = 0;

static Lextok *cpnn(Lextok *, int, int, int);
extern void sr_mesg(FILE *, int, int, const std::string&);
extern void Done_case(std::string&, models::Symbol *);

void setuname(Lextok *n) {
  UType *tmp;

  if (!owner)
    loger::fatal("illegal reference inside typedef");

  for (tmp = Unames; tmp; tmp = tmp->nxt)
    if (owner->name != tmp->nm->name) {
      loger::non_fatal("typename %s was defined before", tmp->nm->name);
      return;
    }

  tmp = (UType *)emalloc(sizeof(UType));
  tmp->nm = owner;
  tmp->cn = n;
  tmp->nxt = Unames;
  Unames = tmp;
}

static void putUname(FILE *fd, UType *tmp) {
  Lextok *fp, *tl;

  if (!tmp)
    return;
  putUname(fd, tmp->nxt); /* postorder */
  fprintf(fd, "struct %s { /* user defined type */\n", tmp->nm->name.c_str());
  for (fp = tmp->cn; fp; fp = fp->rgt)
    for (tl = fp->lft; tl; tl = tl->rgt)
      typ2c(tl->sym);
  fprintf(fd, "};\n");
}

void putunames(FILE *fd) { putUname(fd, Unames); }

bool IsUtype(const std::string &value) {
  UType *tmp;

  for (tmp = Unames; tmp; tmp = tmp->nxt) {
    if (value == tmp->nm->name) {
      return true;
    }
  }
  return false;
}

Lextok *getuname(models::Symbol *t) {
  UType *tmp;

  for (tmp = Unames; tmp; tmp = tmp->nxt) {
    if (t->name == tmp->nm->name) {
      return tmp->cn;
    }
  }
  loger::fatal("%s is not a typename", t->name);
  return (Lextok *)0;
}

void setutype(Lextok *p, models::Symbol *t,
              Lextok *vis) /* user-defined types */
{
  int oln = lineno;
  models::Symbol *ofn = Fname;
  Lextok *m, *n;

  m = getuname(t);
  for (n = p; n; n = n->rgt) {
    lineno = n->ln;
    Fname = n->fn;
    if (n->sym->type) {
      loger::fatal("redeclaration of '%s'", n->sym->name);
    }

    if (n->sym->nbits.value() > 0)
      loger::non_fatal("(%s) only an unsigned can have width-field",
                       n->sym->name);

    if (Expand_Ok)
      n->sym->hidden_flags |= (4 | 8 | 16); /* formal par */
    if (vis) {
      if (vis->sym->name.compare(0, 6, ":hide:") == 0) {
        n->sym->hidden_flags |= 1;
        has_hidden++;
      } else if (vis->sym->name.compare(0, 6, ":show:") == 0) {
        n->sym->hidden_flags |= 2;
      } else if (vis->sym->name.compare(0, 7, ":local:") == 0) {
        n->sym->hidden_flags |= 64;
      }
    }

    n->sym->type = models::SymbolType::kStruct; /* classification   */
    n->sym->struct_template = m;                /* structure itself */
    n->sym->struct_name = t;                    /* name of typedef  */
    n->sym->id = 0;                             /* this is no chan  */
    n->sym->hidden_flags |= 4;
    if (n->sym->value_type <= 0)
      loger::non_fatal("bad array size for '%s'", n->sym->name);
  }
  lineno = oln;
  Fname = ofn;
}

static models::Symbol *do_same(Lextok *n, models::Symbol *v, int xinit) {
  Lextok *tmp, *fp, *tl;
  int ix = eval(n->lft);
  int oln = lineno;
  models::Symbol *ofn = Fname;

  lineno = n->ln;
  Fname = n->fn;

  /* n->sym->type == STRUCT
   * index:		n->lft
   * subfields:		n->rgt
   * structure template:	n->sym->struct_template
   * runtime values:	n->sym->Sval
   */
  if (xinit)
    ini_struct(v); /* once, at top level */

  if (ix >= v->value_type || ix < 0) {
    printf("spin: indexing %s[%d] - size is %d\n", v->name.c_str(), ix, v->value_type);
    loger::fatal("indexing error \'%s\'", v->name);
  }
  if (!n->rgt || !n->rgt->lft) {
    loger::non_fatal("no subfields %s", v->name); /* i.e., wants all */
    lineno = oln;
    Fname = ofn;
    return ZS;
  }

  if (n->rgt->ntyp != '.') {
    printf("bad subfield type %d\n", n->rgt->ntyp);
    alldone(1);
  }

  tmp = n->rgt->lft;
  if (tmp->ntyp != NAME && tmp->ntyp != TYPE) {
    printf("bad subfield entry %d\n", tmp->ntyp);
    alldone(1);
  }
  for (fp = v->Sval[ix]; fp; fp = fp->rgt)
    for (tl = fp->lft; tl; tl = tl->rgt)
      if (tl->sym->name == tmp->sym->name) {
        lineno = oln;
        Fname = ofn;
        return tl->sym;
      }
  loger::fatal("cannot locate subfield %s", tmp->sym->name);
  return ZS;
}

int Rval_struct(Lextok *n, models::Symbol *v,
                int xinit) /* n varref, v valref */
{
  models::Symbol *tl;
  Lextok *tmp;
  int ix;

  if (!n || !(tl = do_same(n, v, xinit)))
    return 0;

  tmp = n->rgt->lft;
  if (tmp->sym->type == models::SymbolType::kStruct) {
    return Rval_struct(tmp, tl, 0);
  } else if (tmp->rgt)
    loger::fatal("non-zero 'rgt' on non-structure");

  ix = eval(tmp->lft);
  /*	printf("%d: ix: %d (%d) %d\n", depth, ix, tl->value_type, tl->val[ix]);
   */
  if (ix >= tl->value_type || ix < 0)
    loger::fatal("indexing error \'%s\'", tl->name);

  return cast_val(tl->type, tl->value[ix], tl->nbits.value());
}

int Lval_struct(Lextok *n, models::Symbol *v, int xinit,
                int a) /* a = assigned value */
{
  models::Symbol *tl;
  Lextok *tmp;
  int ix;

  if (!(tl = do_same(n, v, xinit)))
    return 1;

  tmp = n->rgt->lft;
  if (tmp->sym->type == models::SymbolType::kStruct)
    return Lval_struct(tmp, tl, 0, a);
  else if (tmp->rgt)
    loger::fatal("non-zero 'rgt' on non-structure");

  ix = eval(tmp->lft);
  if (ix >= tl->value_type || ix < 0)
    loger::fatal("indexing error \'%s\'", tl->name);

  if (tl->nbits > 0)
    a = (a & ((1 << tl->nbits.value()) - 1));

  if (a != tl->value[ix]) {
    tl->value[ix] = a;
    tl->last_depth = depth;
  }
  return 1;
}

int Cnt_flds(Lextok *m) {
  Lextok *fp, *tl, *n;
  int cnt = 0;

  if (!m) {
    return 0;
  }

  if (m->ntyp == ',') {
    n = m;
    goto is_lst;
  }
  if (!m->sym || m->ntyp != STRUCT) {
    return 1;
  }

  n = getuname(m->sym);
is_lst:
  for (fp = n; fp; fp = fp->rgt)
    for (tl = fp->lft; tl; tl = tl->rgt) {
      if (tl->sym->type == models::SymbolType::kStruct) {
        if (tl->sym->value_type > 1 || tl->sym->is_array)
          loger::fatal("array of structures in param list, %s", tl->sym->name);
        cnt += Cnt_flds(tl->sym->struct_template);
      } else
        cnt += tl->sym->value_type;
    }
  return cnt;
}

int Sym_typ(Lextok *t) {
  models::Symbol *s = t->sym;

  if (!s)
    return 0;

  if (s->type != models::SymbolType::kStruct)
    return s->type;

  if (!t->rgt || t->rgt->ntyp != '.' /* gh: had ! in wrong place */
      || !t->rgt->lft)
    return STRUCT; /* not a field reference */

  return Sym_typ(t->rgt->lft);
}

int Width_set(int *wdth, int i, Lextok *n) {
  Lextok *fp, *tl;
  int j = i, k;

  for (fp = n; fp; fp = fp->rgt)
    for (tl = fp->lft; tl; tl = tl->rgt) {
      if (tl->sym->type == STRUCT)
        j = Width_set(wdth, j, tl->sym->struct_template);
      else {
        for (k = 0; k < tl->sym->value_type; k++, j++)
          wdth[j] = tl->sym->type;
      }
    }
  return j;
}

void ini_struct(models::Symbol *s) {
  int i;
  Lextok *fp, *tl;

  if (s->type != models::SymbolType::kStruct) /* last step */
  {
    (void)checkvar(s, 0);
    return;
  }
  if (s->Sval == (Lextok **)0) {
    s->Sval = (Lextok **)emalloc(s->value_type * sizeof(Lextok *));
    for (i = 0; i < s->value_type; i++) {
      s->Sval[i] = cpnn(s->struct_template, 1, 1, 1);

      for (fp = s->Sval[i]; fp; fp = fp->rgt)
        for (tl = fp->lft; tl; tl = tl->rgt)
          ini_struct(tl->sym);
    }
  }
}

static Lextok *cpnn(Lextok *s, int L, int R, int S) {
  Lextok *d;
  extern int Nid_nr;

  if (!s)
    return ZN;

  d = (Lextok *)emalloc(sizeof(Lextok));
  d->uiid = s->uiid;
  d->ntyp = s->ntyp;
  d->val = s->val;
  d->ln = s->ln;
  d->fn = s->fn;
  d->sym = s->sym;
  if (L)
    d->lft = cpnn(s->lft, 1, 1, S);
  if (R)
    d->rgt = cpnn(s->rgt, 1, 1, S);

  if (S && s->sym) {
    d->sym = (models::Symbol *)emalloc(sizeof(models::Symbol));
    memcpy(d->sym, s->sym, sizeof(models::Symbol));
    if (d->sym->type == CHAN)
      d->sym->id = ++Nid_nr;
  }
  if (s->sq || s->sl)
    loger::fatal("cannot happen cpnn");

  return d;
}

int full_name(FILE *fd, Lextok *n, models::Symbol *v, int xinit) {
  models::Symbol *tl;
  Lextok *tmp;
  int hiddenarrays = 0;

  fprintf(fd, "%s", v->name.c_str());

  if (!n || !(tl = do_same(n, v, xinit)))
    return 0;
  tmp = n->rgt->lft;

  if (tmp->sym->type == models::SymbolType::kStruct) {
    fprintf(fd, ".");
    hiddenarrays = full_name(fd, tmp, tl, 0);
    goto out;
  }
  fprintf(fd, ".%s", tl->name.c_str());
out:
  if (tmp->sym->value_type > 1 || tmp->sym->is_array == 1) {
    fprintf(fd, "[%d]", eval(tmp->lft));
    hiddenarrays = 1;
  }
  return hiddenarrays;
}

void validref(Lextok *p, Lextok *c) {
  Lextok *fp, *tl;
  std::string lbuf;

  for (fp = p->sym->struct_template; fp; fp = fp->rgt) {
    for (tl = fp->lft; tl; tl = tl->rgt) {
      if (tl->sym->name == c->sym->name) {
        return;
      }
    }
  }

  lbuf = "no field '" + c->sym->name + "' defined in structure '" +
         p->sym->name + "'\n";
  loger::non_fatal(lbuf);
}

void struct_name(Lextok *n, models::Symbol *v, int xinit, std::string &buf) {
  models::Symbol *tl;
  Lextok *tmp;
  std::string lbuf;

  if (!n || !(tl = do_same(n, v, xinit)))
    return;
  tmp = n->rgt->lft;
  if (tmp->sym->type == models::SymbolType::kStruct) {
    buf += ".";
    struct_name(tmp, tl, 0, buf);
    return;
  }
  lbuf = "." + tl->name;
  buf += lbuf;
  if (tmp->sym->value_type > 1 || tmp->sym->is_array == 1) {
    lbuf = "[" + std::to_string(eval(tmp->lft)) + "]";
    buf += lbuf;
  }
}
void walk2_struct(const std::string &s, models::Symbol *z) {
  Lextok *fp, *tl;
  std::string eprefix;
  int ix;

  eprefix = "";
  ini_struct(z);
  if (z->value_type == 1 && z->is_array == 0)
    eprefix = s + z->name + ".";
  for (ix = 0; ix < z->value_type; ix++) {
    if (z->value_type > 1 || z->is_array == 1)
      eprefix = s + z->name + "[" + std::to_string(ix) + "].";
    for (fp = z->Sval[ix]; fp; fp = fp->rgt)
      for (tl = fp->lft; tl; tl = tl->rgt) {
        if (tl->sym->type == models::SymbolType::kStruct)
          walk2_struct(eprefix, tl->sym);
        else if (tl->sym->type == models::SymbolType::kChan)
          Done_case(eprefix, tl->sym);
      }
  }
}

void walk_struct(FILE *ofd, int dowhat, const std::string &s, models::Symbol *z,
                 const std::string &a, const std::string &b,
                 const std::string &c) {
  Lextok *fp, *tl;
  std::string eprefix;
  int ix;

  eprefix = "";

  ini_struct(z);
  if (z->value_type == 1 && z->is_array == 0)
    eprefix = s + z->name + ".";
  for (ix = 0; ix < z->value_type; ix++) {
    if (z->value_type > 1 || z->is_array == 1)
      eprefix = s + z->name + "[" + std::to_string(ix) + "].";
    for (fp = z->Sval[ix]; fp; fp = fp->rgt)
      for (tl = fp->lft; tl; tl = tl->rgt) {
        if (tl->sym->type == models::SymbolType::kStruct)
          walk_struct(ofd, dowhat, eprefix, tl->sym, a, b, c);
        else
          do_var(ofd, dowhat, eprefix, tl->sym, a, b, c);
      }
  }
}

void c_struct(FILE *fd, const std::string &ipref, models::Symbol *z) {
  Lextok *fp, *tl;
  std::string pref, eprefix;
  int ix;

  ini_struct(z);

  for (ix = 0; ix < z->value_type; ix++)
    for (fp = z->Sval[ix]; fp; fp = fp->rgt)
      for (tl = fp->lft; tl; tl = tl->rgt) {
        eprefix = ipref;
        if (z->value_type > 1 ||
            z->is_array == 1) { /* insert index before last '.' */
          eprefix.resize(eprefix.size() - 1);
          pref = "[ " + std::to_string(ix) + " ].";
          eprefix += pref;
        }
        if (tl->sym->type == models::SymbolType::kStruct) {
          eprefix += tl->sym->name;
          eprefix += ".";
          c_struct(fd, eprefix, tl->sym);
        } else
          c_var(fd, eprefix, tl->sym);
      }
}
void dump_struct(models::Symbol *z, const std::string &prefix, RunList *r) {
  Lextok *fp, *tl;
  std::string eprefix;
  int ix, jx;

  ini_struct(z);

  for (ix = 0; ix < z->value_type; ix++) {
    if (z->value_type > 1 || z->is_array == 1)
      eprefix = prefix + "[" + std::to_string(ix) + "]";
    else
      eprefix = prefix;

    for (fp = z->Sval[ix]; fp; fp = fp->rgt)
      for (tl = fp->lft; tl; tl = tl->rgt) {
        if (tl->sym->type == models::SymbolType::kStruct) {
          std::string pref = eprefix + "." + tl->sym->name;
          dump_struct(tl->sym, pref, r);
        } else
          for (jx = 0; jx < tl->sym->value_type; jx++) {
            if (tl->sym->type == models::SymbolType::kChan)
              doq(tl->sym, jx, r);
            else {
              std::string s;
              printf("\t\t");
              if (r)
                printf("%s(%d):", r->n->name.c_str(), r->pid);
              printf("%s.%s", eprefix.c_str(), tl->sym->name.c_str());
              if (tl->sym->value_type > 1 || tl->sym->is_array == 1)
                printf("[%d]", jx);
              printf(" = ");

              if (tl->sym->type == models::SymbolType::kMtype &&
                  tl->sym->mtype_name) {
                s = tl->sym->mtype_name->name;
              }

              sr_mesg(stdout, tl->sym->value[jx],
                      tl->sym->type == models::SymbolType::kMtype, s.c_str());
              printf("\n");
            }
          }
      }
  }
}

static int retrieve(Lextok **targ, int i, int want, Lextok *n, int Ntyp) {
  Lextok *fp, *tl;
  int j = i, k;

  for (fp = n; fp; fp = fp->rgt)
    for (tl = fp->lft; tl; tl = tl->rgt) {
      if (tl->sym->type == models::SymbolType::kStruct) {
        j = retrieve(targ, j, want, tl->sym->struct_template, Ntyp);
        if (j < 0) {
          Lextok *x = cpnn(tl, 1, 0, 0);
          x->rgt = nn(ZN, '.', (*targ), ZN);
          (*targ) = x;
          return -1;
        }
      } else {
        for (k = 0; k < tl->sym->value_type; k++, j++) {
          if (j == want) {
            *targ = cpnn(tl, 1, 0, 0);
            (*targ)->lft = nn(ZN, CONST, ZN, ZN);
            (*targ)->lft->val = k;
            if (Ntyp)
              (*targ)->ntyp = (short)Ntyp;
            return -1;
          }
        }
      }
    }
  return j;
}

static int is_explicit(Lextok *n) {
  if (!n)
    return 0;
  if (!n->sym)
    loger::fatal("unexpected - no symbol");
  if (n->sym->type != models::SymbolType::kStruct)
    return 1;
  if (!n->rgt)
    return 0;
  if (n->rgt->ntyp != '.') {
    lineno = n->ln;
    Fname = n->fn;
    printf("ntyp %d\n", n->rgt->ntyp);
    loger::fatal("unexpected %s, no '.'", n->sym->name);
  }
  return is_explicit(n->rgt->lft);
}

Lextok *expand(Lextok *n, int Ok)
/* turn rgt-lnked list of struct nms, into ',' list of flds */
{
  Lextok *x = ZN, *y;

  if (!Ok)
    return n;

  while (n) {
    y = mk_explicit(n, 1, 0);
    if (x) {
      tail_add(x, y);
    } else {
      x = y;
    }

    n = n->rgt;
  }
  return x;
}

Lextok *mk_explicit(Lextok *n, int Ok, int Ntyp)
/* produce a single ',' list of fields */
{
  Lextok *bld = ZN, *x;
  int i, cnt;
  extern int need_arguments;

  if (n->sym->type != models::SymbolType::kStruct || lexer_.GetInFor() ||
      is_explicit(n))
    return n;

  if (n->rgt && n->rgt->ntyp == '.' && n->rgt->lft && n->rgt->lft->sym &&
      n->rgt->lft->sym->type == models::SymbolType::kStruct) {
    Lextok *y;
    bld = mk_explicit(n->rgt->lft, Ok, Ntyp);
    for (x = bld; x; x = x->rgt) {
      y = cpnn(n, 1, 0, 0);
      y->rgt = nn(ZN, '.', x->lft, ZN);
      x->lft = y;
    }

    return bld;
  }

  if (!Ok || !n->sym->struct_template) {
    if (need_arguments)
      return n;
    printf("spin: saw '");
    comment(stdout, n, 0);
    printf("'\n");
    loger::fatal("incomplete structure ref '%s'", n->sym->name);
  }

  cnt = Cnt_flds(n->sym->struct_template);
  for (i = cnt - 1; i >= 0; i--) {
    bld = nn(ZN, ',', ZN, bld);
    if (retrieve(&(bld->lft), 0, i, n->sym->struct_template, Ntyp) >= 0) {
      printf("cannot retrieve field %d\n", i);
      loger::fatal("bad structure %s", n->sym->name);
    }
    x = cpnn(n, 1, 0, 0);
    x->rgt = nn(ZN, '.', bld->lft, ZN);
    bld->lft = x;
  }
  return bld;
}

Lextok *tail_add(Lextok *a, Lextok *b) {
  Lextok *t;

  for (t = a; t->rgt; t = t->rgt)
    if (t->ntyp != ',')
      loger::fatal("unexpected type - tail_add");
  t->rgt = b;
  return a;
}

void setpname(Lextok *n) {
  UType *tmp;

  for (tmp = Pnames; tmp; tmp = tmp->nxt)
    if (n->sym->name != tmp->nm->name) {
      loger::non_fatal("proctype %s redefined", n->sym->name);
      return;
    }
  tmp = (UType *)emalloc(sizeof(UType));
  tmp->nm = n->sym;
  tmp->nxt = Pnames;
  Pnames = tmp;
}

bool IsProctype(const std::string &value) {
  UType *tmp;

  for (tmp = Pnames; tmp; tmp = tmp->nxt) {
    if (value == tmp->nm->name) {
      return true;
    }
  }
  return false;
}
