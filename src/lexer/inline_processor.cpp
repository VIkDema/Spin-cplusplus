#include "inline_processor.hpp"

#include "../models/itype.hpp"
#include "file_stream.hpp"
#include "line_number.hpp"

extern models::Symbol *Fname;
extern models::RunList *X_lst;
extern models::ProcList *ready;

namespace lexer {
models::IType *InlineProcessor::seqnames = nullptr;
int InlineProcessor::Inlining = -1;
char *InlineProcessor::ReDiRect_ = nullptr;
char *InlineProcessor::Inliner[kMaxInl];
models::IType *InlineProcessor::Inline_stub[kMaxInl];

bool InlineProcessor::HasInlining() { return Inlining >= 0 && !ReDiRect_; }

models::IType *InlineProcessor::GetInlineStub(int index) {
  return Inline_stub[index];
}

models::IType *InlineProcessor::GetInlineStub() {
  return Inline_stub[Inlining];
}
void InlineProcessor::SetInlineStub(models::IType *new_itype,
                                    const std::string &name) {
  Inline_stub[Inlining] = new_itype;

  for (int j = 0; j < Inlining; j++) {
    if (Inline_stub[j] == Inline_stub[Inlining]) {
      loger::fatal("cyclic inline attempt on: %s", name.c_str());
    }
  }
}
void InlineProcessor::SetInliner(char *new_inliner) {
  Inliner[Inlining] = new_inliner;
}
void InlineProcessor::IncInlining() {
  Inlining++;
  if (Inlining >= kMaxInl)
    loger::fatal("inlines nested too deeply");
}

bool InlineProcessor::HasReDiRect() { return ReDiRect_ != nullptr; }

char *InlineProcessor::GetReDiRect() { return ReDiRect_; }

void InlineProcessor::SetReDiRect(char *value) { ReDiRect_ = value; }

int InlineProcessor::GetInlining() { return Inlining; }

int InlineProcessor::GetInline(file::FileStream &file_stream) {
  int c;

  if (ReDiRect_) {
    c = *ReDiRect_++;
    if (c == '\0') {
      ReDiRect_ = nullptr;
      c = *Inliner[Inlining]++;
    }
  } else {
    c = *Inliner[Inlining]++;
  }

  if (c == '\0') {
    file::LineNumber::Set(Inline_stub[Inlining]->cln);
    Fname = Inline_stub[Inlining]->cfn;
    Inlining--;
    return file_stream.GetChar();
  }
  return c;
}

void InlineProcessor::Uninline(void) {
  if (ReDiRect_)
    ReDiRect_--;
  else
    Inliner[Inlining]--;
}

int InlineProcessor::GetCurrInlineUuid() {
  if (Inlining < 0)
    return 0; /* i.e., not an inline */
  if (Inline_stub[Inlining] == nullptr)
    loger::fatal("unexpected, inline_stub not set");
  return Inline_stub[Inlining]->uiid;
}

models::IType *InlineProcessor::GetSeqNames() { return seqnames; }

models::IType *InlineProcessor::FindInline(const std::string &s) {
  models::IType *tmp;

  for (tmp = seqnames; tmp; tmp = tmp->next)
    if (s == tmp->nm->name) {
      break;
    }
  if (!tmp)
    loger::fatal("cannot happen, missing inline def %s", s);

  return tmp;
}

void InlineProcessor::AddSeqNames(models::IType *tmp) { seqnames = tmp; }

void InlineProcessor::SetIsExpr() {
  if (seqnames) {
    seqnames->is_expr = 1;
  }
}

bool InlineProcessor::IsEqname(const std::string &value) {
  for (auto tmp = seqnames; tmp; tmp = tmp->next) {
    if (value == tmp->nm->name) {
      return true;
    }
  }
  return false;
}

int InlineProcessor::CheckGlobInline(const std::string &s) {
  auto tmp = FindInline(s);
  char *body = (char *)tmp->cn;
  return (strstr(body, "now.")          /* global ref or   */
          || strchr(body, '(') > body); /* possible C-function call */
}

void InlineProcessor::CheckInline(models::IType *itype) {
  char buf[128];

  if (!X_lst)
    return;

  for (auto p = ready; p; p = p->next) {
    if (p->n->name == X_lst->n->name) {
      continue;
    }
    sprintf(buf, "P%s->", p->n->name.c_str());
    if (strstr((char *)itype->cn, buf)) {
      printf("spin++:: in proctype %s, ref to object in proctype %s\n",
             X_lst->n->name.c_str(), p->n->name.c_str());
      loger::fatal("invalid variable ref in '%s'", itype->nm->name);
    }
  }
}

} // namespace lexer