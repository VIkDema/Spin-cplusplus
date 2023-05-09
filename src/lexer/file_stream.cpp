#include "file_stream.hpp"

#define MAXINL 16 /* max recursion depth inline fcts */
#include "../models/itype.hpp"
#include <string>
#include <vector>

static int Inlining = -1;

extern FILE *yyin;
static char *ReDiRect;
static char *Inliner[MAXINL];
static models::IType *Inline_stub[MAXINL];
extern models::Symbol *Fname;

namespace file {
FileStream::FileStream() : push_back_(0), pushed_back_(0), line_number_(0) {}
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

  if (Inlining < 0) {
    do {
      curr = getc(yyin);
    } while (curr == 0);
  } else {
    curr = GetInline();
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

int FileStream::GetInline() {
  int c;

  if (ReDiRect) {
    c = *ReDiRect++;
    if (c == '\0') {
      ReDiRect = (char *)0;
      c = *Inliner[Inlining]++;
    }
  } else {
    c = *Inliner[Inlining]++;
  }

  if (c == '\0') {
    line_number_ = Inline_stub[Inlining]->cln;
    Fname = Inline_stub[Inlining]->cfn;
    Inlining--;
    return GetChar();
  }
  return c;
}

void FileStream::Ungetch(int curr) {
  if (pushed_back_ > 0 && push_back_ > 0) {
    push_back_--;
    return;
  }
  if (Inlining) {
    ungetc(curr, yyin);
  } else {
    Uninline();
  }
}

void FileStream::Uninline(void) {
  if (ReDiRect)
    ReDiRect--;
  else
    Inliner[Inlining]--;
}

bool FileStream::HasInlining() { return Inlining >= 0 && !ReDiRect; }

models::IType *FileStream::GetInlineStub(int index) {
  return Inline_stub[index];
}

bool FileStream::HasReDiRect() { return ReDiRect != nullptr; }

char *FileStream::GetReDiRect() { return ReDiRect; }

void FileStream::SetReDiRect(char *value) { ReDiRect = value; }

int FileStream::GetInlining() { return Inlining; }

} // namespace file
