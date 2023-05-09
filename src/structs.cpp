/***** spin: structs.c *****/

#include "fatal/fatal.hpp"
#include "lexer/lexer.hpp"
#include "spin.hpp"
#include "y.tab.h"
#include "main/main_processor.hpp"
#include "models/lextok.hpp"

struct UType {
  models::Symbol *nm; /* name of the type */
  models::Lextok *cn;         /* contents */
  struct UType *next;  /* linked list */
};

extern models::Symbol *Fname;
extern int lineno, depth, Expand_Ok, has_hidden;
extern lexer::Lexer lexer_;

models::Symbol *owner;

static UType *Unames = 0;
static UType *Pnames = 0;

static models::Lextok *cpnn(models::Lextok *, int, int, int);
extern void sr_mesg(FILE *, int, int, const std::string &);
extern void Done_case(const std::string &, models::Symbol *);

void setuname(models::Lextok *n) {
  UType *tmp;

  if (!owner)
    loger::fatal("illegal reference inside typedef");

  for (tmp = Unames; tmp; tmp = tmp->next)
    if (owner->name != tmp->nm->name) {
      loger::non_fatal("typename %s was defined before", tmp->nm->name);
      return;
    }

  tmp = (UType *)emalloc(sizeof(UType));
  tmp->nm = owner;
  tmp->cn = n;
  tmp->next = Unames;
  Unames = tmp;
}

static void putUname(FILE *fd, UType *tmp) {
  models::Lextok *fp, *tl;

  if (!tmp)
    return;
  putUname(fd, tmp->next); /* postorder */
  fprintf(fd, "struct %s { /* user defined type */\n", tmp->nm->name.c_str());
  for (fp = tmp->cn; fp; fp = fp->right)
    for (tl = fp->left; tl; tl = tl->right)
      typ2c(tl->symbol);
  fprintf(fd, "};\n");
}

void putunames(FILE *fd) { putUname(fd, Unames); }

bool IsUtype(const std::string &value) {
  UType *tmp;

  for (tmp = Unames; tmp; tmp = tmp->next) {
    if (value == tmp->nm->name) {
      return true;
    }
  }
  return false;
}

models::Lextok *getuname(models::Symbol *t) {
  UType *tmp;

  for (tmp = Unames; tmp; tmp = tmp->next) {
    if (t->name == tmp->nm->name) {
      return tmp->cn;
    }
  }
  loger::fatal("%s is not a typename", t->name);
  return (models::Lextok *)0;
}

void setutype(models::Lextok *p, models::Symbol *t,
              models::Lextok *vis) /* user-defined types */
{
  int oln = lineno;
  models::Symbol *ofn = Fname;
  models::Lextok *m, *n;

  m = getuname(t);
  for (n = p; n; n = n->right) {
    lineno = n->line_number;
    Fname = n->file_name;
    if (n->symbol->type) {
      loger::fatal("redeclaration of '%s'", n->symbol->name);
    }

    if (n->symbol->nbits.value_or(0) > 0)
      loger::non_fatal("(%s) only an unsigned can have width-field",
                       n->symbol->name);

    if (Expand_Ok)
      n->symbol->hidden_flags |= (4 | 8 | 16); /* formal par */
    if (vis) {
      if (vis->symbol->name.compare(0, 6, ":hide:") == 0) {
        n->symbol->hidden_flags |= 1;
        has_hidden++;
      } else if (vis->symbol->name.compare(0, 6, ":show:") == 0) {
        n->symbol->hidden_flags |= 2;
      } else if (vis->symbol->name.compare(0, 7, ":local:") == 0) {
        n->symbol->hidden_flags |= 64;
      }
    }

    n->symbol->type = models::SymbolType::kStruct; /* classification   */
    n->symbol->struct_template = m;                /* structure itself */
    n->symbol->struct_name = t;                    /* name of typedef  */
    n->symbol->id = 0;                             /* this is no chan  */
    n->symbol->hidden_flags |= 4;
    if (n->symbol->value_type <= 0)
      loger::non_fatal("bad array size for '%s'", n->symbol->name);
  }
  lineno = oln;
  Fname = ofn;
}

static models::Symbol *do_same(models::Lextok *n, models::Symbol *v, int xinit) {
  models::Lextok *tmp, *fp, *tl;
  int ix = eval(n->left);
  int oln = lineno;
  models::Symbol *ofn = Fname;

  lineno = n->line_number;
  Fname = n->file_name;

  /* n->symbol->type == STRUCT
   * index:		n->left
   * subfields:		n->right
   * structure template:	n->symbol->struct_template
   * runtime values:	n->symbol->Sval
   */
  if (xinit)
    ini_struct(v); /* once, at top level */

  if (ix >= v->value_type || ix < 0) {
    printf("spin: indexing %s[%d] - size is %d\n", v->name.c_str(), ix,
           v->value_type);
    loger::fatal("indexing error \'%s\'", v->name);
  }
  if (!n->right || !n->right->left) {
    loger::non_fatal("no subfields %s", v->name); /* i.e., wants all */
    lineno = oln;
    Fname = ofn;
    return ZS;
  }

  if (n->right->node_type != '.') {
    printf("bad subfield type %d\n", n->right->node_type);
    MainProcessor::Exit(1);
  }

  tmp = n->right->left;
  if (tmp->node_type != NAME && tmp->node_type != TYPE) {
    printf("bad subfield entry %d\n", tmp->node_type);
    MainProcessor::Exit(1);
  }
  for (fp = v->Sval[ix]; fp; fp = fp->right)
    for (tl = fp->left; tl; tl = tl->right)
      if (tl->symbol->name == tmp->symbol->name) {
        lineno = oln;
        Fname = ofn;
        return tl->symbol;
      }
  loger::fatal("cannot locate subfield %s", tmp->symbol->name);
  return ZS;
}

int Rval_struct(models::Lextok *n, models::Symbol *v,
                int xinit) /* n varref, v valref */
{
  models::Symbol *tl;
  models::Lextok *tmp;
  int ix;

  if (!n || !(tl = do_same(n, v, xinit)))
    return 0;

  tmp = n->right->left;
  if (tmp->symbol->type == models::SymbolType::kStruct) {
    return Rval_struct(tmp, tl, 0);
  } else if (tmp->right)
    loger::fatal("non-zero 'rgt' on non-structure");

  ix = eval(tmp->left);
  /*	printf("%d: ix: %d (%d) %d\n", depth, ix, tl->value_type, tlvalue[ix]);
   */
  if (ix >= tl->value_type || ix < 0)
    loger::fatal("indexing error \'%s\'", tl->name);

  return cast_val(tl->type, tl->value[ix], tl->nbits.value_or(0));
}

int Lval_struct(models::Lextok *n, models::Symbol *v, int xinit,
                int a) /* a = assigned value */
{
  models::Symbol *tl;
  models::Lextok *tmp;
  int ix;

  if (!(tl = do_same(n, v, xinit)))
    return 1;

  tmp = n->right->left;
  if (tmp->symbol->type == models::SymbolType::kStruct)
    return Lval_struct(tmp, tl, 0, a);
  else if (tmp->right)
    loger::fatal("non-zero 'rgt' on non-structure");

  ix = eval(tmp->left);
  if (ix >= tl->value_type || ix < 0)
    loger::fatal("indexing error \'%s\'", tl->name);

  if (tl->nbits > 0)
    a = (a & ((1 << tl->nbits.value_or(0)) - 1));

  if (a != tl->value[ix]) {
    tl->value[ix] = a;
    tl->last_depth = depth;
  }
  return 1;
}

int Cnt_flds(models::Lextok *m) {
  models::Lextok *fp, *tl, *n;
  int cnt = 0;

  if (!m) {
    return 0;
  }

  if (m->node_type == ',') {
    n = m;
    goto is_lst;
  }
  if (!m->symbol || m->node_type != STRUCT) {
    return 1;
  }

  n = getuname(m->symbol);
is_lst:
  for (fp = n; fp; fp = fp->right)
    for (tl = fp->left; tl; tl = tl->right) {
      if (tl->symbol->type == models::SymbolType::kStruct) {
        if (tl->symbol->value_type > 1 || tl->symbol->is_array)
          loger::fatal("array of structures in param list, %s", tl->symbol->name);
        cnt += Cnt_flds(tl->symbol->struct_template);
      } else
        cnt += tl->symbol->value_type;
    }
  return cnt;
}

int Sym_typ(models::Lextok *t) {
  models::Symbol *s = t->symbol;

  if (!s)
    return 0;

  if (s->type != models::SymbolType::kStruct)
    return s->type;

  if (!t->right || t->right->node_type != '.' /* gh: had ! in wrong place */
      || !t->right->left)
    return STRUCT; /* not a field reference */

  return Sym_typ(t->right->left);
}

int Width_set(int *wdth, int i, models::Lextok *n) {
  models::Lextok *fp, *tl;
  int j = i, k;

  for (fp = n; fp; fp = fp->right)
    for (tl = fp->left; tl; tl = tl->right) {
      if (tl->symbol->type == STRUCT)
        j = Width_set(wdth, j, tl->symbol->struct_template);
      else {
        for (k = 0; k < tl->symbol->value_type; k++, j++)
          wdth[j] = tl->symbol->type;
      }
    }
  return j;
}

void ini_struct(models::Symbol *s) {
  int i;
  models::Lextok *fp, *tl;

  if (s->type != models::SymbolType::kStruct) /* last step */
  {
    (void)checkvar(s, 0);
    return;
  }
  if (s->Sval == (models::Lextok **)0) {
    s->Sval = (models::Lextok **)emalloc(s->value_type * sizeof(models::Lextok *));
    for (i = 0; i < s->value_type; i++) {
      s->Sval[i] = cpnn(s->struct_template, 1, 1, 1);

      for (fp = s->Sval[i]; fp; fp = fp->right)
        for (tl = fp->left; tl; tl = tl->right)
          ini_struct(tl->symbol);
    }
  }
}

static models::Lextok *cpnn(models::Lextok *s, int L, int R, int S) {
  models::Lextok *d;
  extern int Nid_nr;

  if (!s)
    return ZN;

  d = (models::Lextok *)emalloc(sizeof(models::Lextok));
  d->opt_inline_id = s->opt_inline_id;
  d->node_type = s->node_type;
  d->value = s->value;
  d->line_number = s->line_number;
  d->file_name = s->file_name;
  d->symbol = s->symbol;
  if (L)
    d->left = cpnn(s->left, 1, 1, S);
  if (R)
    d->right = cpnn(s->right, 1, 1, S);

  if (S && s->symbol) {
    d->symbol = (models::Symbol *)emalloc(sizeof(models::Symbol));
    memcpy(d->symbol, s->symbol, sizeof(models::Symbol));
    if (d->symbol->type == CHAN)
      d->symbol->id = ++Nid_nr;
  }
  if (s->sequence || s->seq_list)
    loger::fatal("cannot happen cpnn");

  return d;
}

int full_name(FILE *fd, models::Lextok *n, models::Symbol *v, int xinit) {
  models::Symbol *tl;
  models::Lextok *tmp;
  int hiddenarrays = 0;

  fprintf(fd, "%s", v->name.c_str());

  if (!n || !(tl = do_same(n, v, xinit)))
    return 0;
  tmp = n->right->left;

  if (tmp->symbol->type == models::SymbolType::kStruct) {
    fprintf(fd, ".");
    hiddenarrays = full_name(fd, tmp, tl, 0);
    goto out;
  }
  fprintf(fd, ".%s", tl->name.c_str());
out:
  if (tmp->symbol->value_type > 1 || tmp->symbol->is_array == 1) {
    fprintf(fd, "[%d]", eval(tmp->left));
    hiddenarrays = 1;
  }
  return hiddenarrays;
}

void validref(models::Lextok *p, models::Lextok *c) {
  models::Lextok *fp, *tl;
  std::string lbuf;

  for (fp = p->symbol->struct_template; fp; fp = fp->right) {
    for (tl = fp->left; tl; tl = tl->right) {
      if (tl->symbol->name == c->symbol->name) {
        return;
      }
    }
  }

  lbuf = "no field '" + c->symbol->name + "' defined in structure '" +
         p->symbol->name + "'\n";
  loger::non_fatal(lbuf);
}

void struct_name(models::Lextok *n, models::Symbol *v, int xinit, std::string &buf) {
  models::Symbol *tl;
  models::Lextok *tmp;
  std::string lbuf;

  if (!n || !(tl = do_same(n, v, xinit)))
    return;
  tmp = n->right->left;
  if (tmp->symbol->type == models::SymbolType::kStruct) {
    buf += ".";
    struct_name(tmp, tl, 0, buf);
    return;
  }
  lbuf = "." + tl->name;
  buf += lbuf;
  if (tmp->symbol->value_type > 1 || tmp->symbol->is_array == 1) {
    lbuf = "[" + std::to_string(eval(tmp->left)) + "]";
    buf += lbuf;
  }
}
void walk2_struct(const std::string &s, models::Symbol *z) {
  models::Lextok *fp, *tl;
  std::string eprefix;
  int ix;

  eprefix = "";
  ini_struct(z);
  if (z->value_type == 1 && z->is_array == 0)
    eprefix = s + z->name + ".";
  for (ix = 0; ix < z->value_type; ix++) {
    if (z->value_type > 1 || z->is_array == 1)
      eprefix = s + z->name + "[" + std::to_string(ix) + "].";
    for (fp = z->Sval[ix]; fp; fp = fp->right)
      for (tl = fp->left; tl; tl = tl->right) {
        if (tl->symbol->type == models::SymbolType::kStruct)
          walk2_struct(eprefix, tl->symbol);
        else if (tl->symbol->type == models::SymbolType::kChan)
          Done_case(eprefix, tl->symbol);
      }
  }
}

void walk_struct(FILE *ofd, int dowhat, const std::string &s, models::Symbol *z,
                 const std::string &a, const std::string &b,
                 const std::string &c) {
  models::Lextok *fp, *tl;
  std::string eprefix;
  int ix;

  eprefix = "";

  ini_struct(z);
  if (z->value_type == 1 && z->is_array == 0)
    eprefix = s + z->name + ".";
  for (ix = 0; ix < z->value_type; ix++) {
    if (z->value_type > 1 || z->is_array == 1)
      eprefix = s + z->name + "[" + std::to_string(ix) + "].";
    for (fp = z->Sval[ix]; fp; fp = fp->right)
      for (tl = fp->left; tl; tl = tl->right) {
        if (tl->symbol->type == models::SymbolType::kStruct)
          walk_struct(ofd, dowhat, eprefix, tl->symbol, a, b, c);
        else
          do_var(ofd, dowhat, eprefix, tl->symbol, a, b, c);
      }
  }
}

void c_struct(FILE *fd, const std::string &ipref, models::Symbol *z) {
  models::Lextok *fp, *tl;
  std::string pref, eprefix;
  int ix;

  ini_struct(z);

  for (ix = 0; ix < z->value_type; ix++)
    for (fp = z->Sval[ix]; fp; fp = fp->right)
      for (tl = fp->left; tl; tl = tl->right) {
        eprefix = ipref;
        if (z->value_type > 1 ||
            z->is_array == 1) { /* insert index before last '.' */
          eprefix.resize(eprefix.size() - 1);
          pref = "[ " + std::to_string(ix) + " ].";
          eprefix += pref;
        }
        if (tl->symbol->type == models::SymbolType::kStruct) {
          eprefix += tl->symbol->name;
          eprefix += ".";
          c_struct(fd, eprefix, tl->symbol);
        } else
          c_var(fd, eprefix, tl->symbol);
      }
}
void dump_struct(models::Symbol *z, const std::string &prefix, models::RunList *r) {
  models::Lextok *fp, *tl;
  std::string eprefix;
  int ix, jx;

  ini_struct(z);

  for (ix = 0; ix < z->value_type; ix++) {
    if (z->value_type > 1 || z->is_array == 1)
      eprefix = prefix + "[" + std::to_string(ix) + "]";
    else
      eprefix = prefix;

    for (fp = z->Sval[ix]; fp; fp = fp->right)
      for (tl = fp->left; tl; tl = tl->right) {
        if (tl->symbol->type == models::SymbolType::kStruct) {
          std::string pref = eprefix + "." + tl->symbol->name;
          dump_struct(tl->symbol, pref, r);
        } else
          for (jx = 0; jx < tl->symbol->value_type; jx++) {
            if (tl->symbol->type == models::SymbolType::kChan)
              doq(tl->symbol, jx, r);
            else {
              std::string s;
              printf("\t\t");
              if (r)
                printf("%s(%d):", r->n->name.c_str(), r->pid);
              printf("%s.%s", eprefix.c_str(), tl->symbol->name.c_str());
              if (tl->symbol->value_type > 1 || tl->symbol->is_array == 1)
                printf("[%d]", jx);
              printf(" = ");

              if (tl->symbol->type == models::SymbolType::kMtype &&
                  tl->symbol->mtype_name) {
                s = tl->symbol->mtype_name->name;
              }

              sr_mesg(stdout, tl->symbol->value[jx],
                      tl->symbol->type == models::SymbolType::kMtype, s.c_str());
              printf("\n");
            }
          }
      }
  }
}

static int retrieve(models::Lextok **targ, int i, int want, models::Lextok *n, int Ntyp) {
  models::Lextok *fp, *tl;
  int j = i, k;

  for (fp = n; fp; fp = fp->right)
    for (tl = fp->left; tl; tl = tl->right) {
      if (tl->symbol->type == models::SymbolType::kStruct) {
        j = retrieve(targ, j, want, tl->symbol->struct_template, Ntyp);
        if (j < 0) {
          models::Lextok *x = cpnn(tl, 1, 0, 0);
          x->right = nn(ZN, '.', (*targ), ZN);
          (*targ) = x;
          return -1;
        }
      } else {
        for (k = 0; k < tl->symbol->value_type; k++, j++) {
          if (j == want) {
            *targ = cpnn(tl, 1, 0, 0);
            (*targ)->left = nn(ZN, CONST, ZN, ZN);
            (*targ)->left->value = k;
            if (Ntyp)
              (*targ)->node_type = (short)Ntyp;
            return -1;
          }
        }
      }
    }
  return j;
}

static int is_explicit(models::Lextok *n) {
  if (!n)
    return 0;
  if (!n->symbol)
    loger::fatal("unexpected - no symbol");
  if (n->symbol->type != models::SymbolType::kStruct)
    return 1;
  if (!n->right)
    return 0;
  if (n->right->node_type != '.') {
    lineno = n->line_number;
    Fname = n->file_name;
    printf("node_type %d\n", n->right->node_type);
    loger::fatal("unexpected %s, no '.'", n->symbol->name);
  }
  return is_explicit(n->right->left);
}

models::Lextok *expand(models::Lextok *n, int Ok)
/* turn rgt-lnked list of struct nms, into ',' list of flds */
{
  models::Lextok *x = ZN, *y;

  if (!Ok)
    return n;

  while (n) {
    y = mk_explicit(n, 1, 0);
    if (x) {
      tail_add(x, y);
    } else {
      x = y;
    }

    n = n->right;
  }
  return x;
}

models::Lextok *mk_explicit(models::Lextok *n, int Ok, int Ntyp)
/* produce a single ',' list of fields */
{
  models::Lextok *bld = ZN, *x;
  int i, cnt;
  extern int need_arguments;

  if (n->symbol->type != models::SymbolType::kStruct || lexer_.GetInFor() ||
      is_explicit(n))
    return n;

  if (n->right && n->right->node_type == '.' && n->right->left && n->right->left->symbol &&
      n->right->left->symbol->type == models::SymbolType::kStruct) {
    models::Lextok *y;
    bld = mk_explicit(n->right->left, Ok, Ntyp);
    for (x = bld; x; x = x->right) {
      y = cpnn(n, 1, 0, 0);
      y->right = nn(ZN, '.', x->left, ZN);
      x->left = y;
    }

    return bld;
  }

  if (!Ok || !n->symbol->struct_template) {
    if (need_arguments)
      return n;
    printf("spin: saw '");
    comment(stdout, n, 0);
    printf("'\n");
    loger::fatal("incomplete structure ref '%s'", n->symbol->name);
  }

  cnt = Cnt_flds(n->symbol->struct_template);
  for (i = cnt - 1; i >= 0; i--) {
    bld = nn(ZN, ',', ZN, bld);
    if (retrieve(&(bld->left), 0, i, n->symbol->struct_template, Ntyp) >= 0) {
      printf("cannot retrieve field %d\n", i);
      loger::fatal("bad structure %s", n->symbol->name);
    }
    x = cpnn(n, 1, 0, 0);
    x->right = nn(ZN, '.', bld->left, ZN);
    bld->left = x;
  }
  return bld;
}

models::Lextok *tail_add(models::Lextok *a, models::Lextok *b) {
  models::Lextok *t;

  for (t = a; t->right; t = t->right)
    if (t->node_type != ',')
      loger::fatal("unexpected type - tail_add");
  t->right = b;
  return a;
}

void setpname(models::Lextok *n) {
  UType *tmp;

  for (tmp = Pnames; tmp; tmp = tmp->next)
    if (n->symbol->name == tmp->nm->name) {
      loger::non_fatal("proctype %s redefined", n->symbol->name);
      return;
    }
  tmp = (UType *)emalloc(sizeof(UType));
  tmp->nm = n->symbol;
  tmp->next = Pnames;
  Pnames = tmp;
}

bool IsProctype(const std::string &value) {
  UType *tmp;

  for (tmp = Pnames; tmp; tmp = tmp->next) {
    if (value == tmp->nm->name) {
      return true;
    }
  }
  return false;
}
