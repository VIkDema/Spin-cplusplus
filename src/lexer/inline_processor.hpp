#pragma once
#include "../models/models_fwd.hpp"
#include "file_stream.hpp"

namespace lexer {
constexpr int kMaxInl = 16;

class InlineProcessor {
public:
  static bool HasInlining();
  static models::IType *GetInlineStub(int index);
  static models::IType *GetInlineStub();

  static bool HasReDiRect();
  static char *GetReDiRect();

  static int GetInlining();
  static void SetReDiRect(char *value);

  static int GetInline(file::FileStream &file_stream);
  static void Uninline();

  static models::IType *GetSeqNames();
  static void AddSeqNames(models::IType *tmp);
  static models::IType *FindInline(const std::string &s);
  static void SetIsExpr();
  static int GetCurrInlineUuid();

  static bool IsEqname(const std::string &value);

  static void SetInlineStub(models::IType *new_itype, const std::string &name);
  static void SetInliner(char *new_inliner);
  static void IncInlining();

  static int CheckGlobInline(const std::string &s);
  static void CheckInline(models::IType* itype);

private:
  static int Inlining;
  static char *ReDiRect_;
  static char *Inliner[kMaxInl];
  static models::IType *Inline_stub[kMaxInl];
  static models::IType *seqnames;
};
} // namespace lexer