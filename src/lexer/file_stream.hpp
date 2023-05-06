#pragma once
#include "../fatal/fatal.hpp"
#include "../spin.hpp"
#include "models/itype.hpp"
#include <string>

namespace file {

class FileStream {
public:
  FileStream();
  int GetChar();
  std::string GetWord(int, int (*)(int));

  void Ungetch(int curr);

  void IncLineNumber() { line_number_++; }

  void SetLineNumber(int new_line_number) { line_number_ = new_line_number; }

  int GetLineNumber() { return line_number_; }

  void push_back(const std::string &value) {
    if (pushed_back_ + value.size() > 4094) {
      loger::fatal("select statement too large");
    }
    pushed_back_stream_ += value;
    pushed_back_ += value.size();
  }

  // TODO: вынести отсюда в отдельный класс
  bool HasInlining();
  models::IType *GetInlineStub(int index);
  bool HasReDiRect();
  char *GetReDiRect();
  int GetInlining();
  void SetReDiRect(char *value);

private:
  int GetInline();
  void Uninline();

  int push_back_;
  int pushed_back_;
  int line_number_;
  std::string pushed_back_stream_;
};
} // namespace file