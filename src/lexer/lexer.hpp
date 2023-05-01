#pragma once
#include "deferred.hpp"
#include "file_stream.hpp"
#include <optional>
#include <string>
#include <vector>
#include "scope.hpp"

namespace lexer {
class Lexer {
public:
  Lexer();
  Lexer(bool pp_mode);

  void SetLastToken(int last_token);
  int GetLastToken();

  int lex();

  void add_inline_argument();
  void update_inline_argument(const std::string &additional);
  void inc_index_argument();

  void inc_inline_nesting();
  void des_inline_nesting();
  std::size_t get_inline_nesting();

private:
  int pre_proc();
  void do_directive(int first_char);
  int CheckName(const std::string &value);
  bool ScatTo(int stop, int (*Predicate)(int), std::string &buf,
              int max_size = 0);
  bool ScatTo(int stop);
  int Follow(int token, int ifyes, int ifno);

  file::FileStream stream_;
  ScopeProcessor scope_;
  ::helpers::Deferred deferred_;

  std::vector<std::string> inline_arguments_;
  std::size_t curr_inline_argument_;
  std::size_t argument_nesting_;
  int last_token_;
  [[maybe_unused]] bool pp_mode_;

  std::string temp_hold_;
  int temp_has_;
};
} // namespace lexer