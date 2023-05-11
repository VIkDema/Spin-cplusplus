#include "symbol.hpp"

#include "../lexer/inline_processor.hpp"
#include "../lexer/line_number.hpp"
#include "../main/launch_settings.hpp"
#include "../utils/utils.hpp"
#include "access.hpp"
#include "utype.hpp"

extern models::Symbol *Fname;
extern models::UType *Pnames;
extern models::Symbol *owner;
extern char *emalloc(size_t);
extern LaunchSettings launch_settings;
extern int depth, NamesNotAdded;

static models::Ordered *last_name = nullptr;
static models::Symbol *symtab[Nhash + 1];
models::Ordered *all_names;

namespace models {

namespace {
static int samename(models::Symbol *a, models::Symbol *b) {
  if (!a && !b)
    return 1;
  if (!a || !b)
    return 0;
  return a->name == b->name;
}
} // namespace

void Symbol::AddAccess(models::Symbol *what, int count, int type) {
  models::Access *a;

  for (a = access; a; a = a->next)
    if (a->who == models::Symbol::GetContext() && a->what == what &&
        a->count == count && a->type == type)
      return;

  a = (models::Access *)emalloc(sizeof(models::Access));
  a->who = models::Symbol::GetContext();
  a->what = what;
  a->count = count;
  a->type = type;
  a->next = access;
  access = a;
}

bool SideScan(const std::string &t, const std::string &pat) {
  size_t pos = t.find(pat);
  return (pos != std::string::npos && pos > 0 && t[pos - 1] != '"' &&
          t[pos - 1] != '\'');
}

void Symbol::DetectSideEffects() {
  char *t;
  char *z;

  /* could still defeat this check via hidden_flags
   * side effects in function calls,
   * but this will catch at least some cases
   */

  auto tmp = lexer::InlineProcessor::FindInline(name);
  t = (char *)tmp->cn;
  while (t && *t == ' ') {
    t++;
  }

  z = strchr(t, '(');
  if (z && z > t && isalnum((int)*(z - 1)) &&
      strncmp(t, "spin_mutex_free(", strlen("spin_mutex_free(")) != 0) {
    goto bad; /* fct call */
  }

  if (SideScan(t, ";") || SideScan(t, "++") || SideScan(t, "--")) {
  bad:
    file::LineNumber::Set(tmp->dln);
    Fname = tmp->dfn;
    loger::non_fatal("c_expr %s has side-effects", name);
    return;
  }
  while ((t = strchr(t, '=')) != NULL) {
    if (*(t - 1) == '!' || *(t - 1) == '>' || *(t - 1) == '<' ||
        *(t - 1) == '"' || *(t - 1) == '\'') {
      t += 2;
      continue;
    }
    t++;
    if (*t != '=')
      goto bad;
    t++;
  }
}
Symbol *Symbol::context_ = nullptr;

void Symbol::SetContext(Symbol *context) { context_ = context; }

Symbol *Symbol::GetContext() { return context_; }

Symbol *Symbol::BuildOrFind(const std::string &name) {
  models::Ordered *no;
  unsigned int h = utils::hash(name);

  if (launch_settings.need_old_scope_rules) { /* same scope - global refering to
                            global or local to local */
    for (auto sp = symtab[h]; sp; sp = sp->next) {
      if (sp->name == name && samename(sp->context, context_) &&
          samename(sp->owner_name, owner)) {
        return sp; /* found */
      }
    }
  } else { /* added 6.0.0: more traditional, scope rule */
    for (auto sp = symtab[h]; sp; sp = sp->next) {
      if (sp->name == name && samename(sp->context, context_) &&
          (sp->block_scope == lexer::ScopeProcessor::GetCurrScope() ||
           (sp->block_scope.compare(0, sp->block_scope.length(),
                                    lexer::ScopeProcessor::GetCurrScope()) ==
                0 &&
            samename(sp->owner_name, owner)))) {
        if (!samename(sp->owner_name, owner)) {
          printf("spin: different container %s\n", sp->name.c_str());
          printf("    old: %s\n",
                 sp->owner_name ? sp->owner_name->name.c_str() : "--");
          printf("    new: %s\n", owner ? owner->name.c_str() : "--");
        }
        return sp; /* found */
      }
    }
  }

  if (context_) /* in proctype, refers to global */ {
    for (auto sp = symtab[h]; sp; sp = sp->next) {
      if (sp->name != name || sp->context != nullptr ||
          !samename(sp->owner_name, owner)) {
        continue;
      }
      return sp; /* global */
    }
  }

  auto sp = (models::Symbol *)emalloc(sizeof(models::Symbol));
  sp->name = name;
  sp->value_type = 1;
  sp->last_depth = depth;
  sp->context = context_;
  sp->owner_name = owner; /* if fld in struct */
  sp->block_scope = lexer::ScopeProcessor::GetCurrScope();

  if (NamesNotAdded != 0) {
    return sp;
  }

  sp->next = symtab[h];
  symtab[h] = sp;
  no = (models::Ordered *)emalloc(sizeof(models::Ordered));
  no->entry = sp;

  if (!last_name)
    last_name = all_names = no;
  else {
    last_name->next = no;
    last_name = no;
  }

  return sp;
}

bool Symbol::IsProctype() {

  for (auto tmp = Pnames; tmp; tmp = tmp->next) {
    if (name == tmp->nm->name) {
      return true;
    }
  }
  return false;
}

} // namespace models