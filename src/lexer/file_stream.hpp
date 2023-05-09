#pragma once

#include "../fatal/fatal.hpp"
#include "../models/itype.hpp"
#include "../spin.hpp"
#include <string>

namespace file {

class FileStream {
public:
  FileStream();
  int GetChar();
  std::string GetWord(int, int (*)(int));

  void Ungetch(int curr);

  void push_back(const std::string &value) {
    if (pushed_back_ + value.size() > 4094) {
      loger::fatal("select statement too large");
    }
    pushed_back_stream_ += value;
    pushed_back_ += value.size();
  }

private:
  int push_back_;
  int pushed_back_;
  std::string pushed_back_stream_;
};
} // namespace file