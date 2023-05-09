#include "file_stream.hpp"

#include "../models/itype.hpp"
#include "inline_processor.hpp"
#include <string>
#include <vector>

extern FILE *yyin;

namespace file {
FileStream::FileStream() : push_back_(0), pushed_back_(0){}
int FileStream::GetChar() {
  int curr;
  if (pushed_back_ > 0 && push_back_ < pushed_back_) {
    curr = pushed_back_stream_[push_back_++];
    if (push_back_ == pushed_back_) {
      pushed_back_stream_[0] = '\0';
      push_back_ = pushed_back_ = 0;
    }
    return curr;
  }

  if (lexer::InlineProcessor::GetInlining() < 0) {
    do {
      curr = getc(yyin);
    } while (curr == 0);
  } else {
    curr = lexer::InlineProcessor::GetInline(*this);
  }

  return curr;
}

// TODO: fix on function
std::string FileStream::GetWord(int first, int (*Predicate)(int)) {
  int curr;
  std::string result;
  result.push_back(first);

  while (Predicate(curr = GetChar())) {
    if (curr == EOF) {
      break;
    }
    result.push_back(curr);
    if (curr == '\\') {
      curr = GetChar();
      result.push_back(curr);
    }
  }
  Ungetch(curr);
  return result;
}

void FileStream::Ungetch(int curr) {
  if (pushed_back_ > 0 && push_back_ > 0) {
    push_back_--;
    return;
  }
  if (lexer::InlineProcessor::GetInlining()) {
    ungetc(curr, yyin);
  } else {
    lexer::InlineProcessor::Uninline();
  }
}

} // namespace file
