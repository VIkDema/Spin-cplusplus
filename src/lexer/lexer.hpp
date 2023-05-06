#pragma once
#include "deferred.hpp"
#include "file_stream.hpp"
#include "scope.hpp"
#include <optional>
#include <string>
#include <vector>

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

  void inc_parameter_count() { parameter_count_++; }
  void des_parameter_count() { parameter_count_--; }

  void SetHasCode(int has_code) { has_code_ = has_code; }
  void SetHasPriority(int has_priority) { has_priority_ = has_priority; }
  void SetInFor(int in_for) { in_for_ = in_for; }
  void IncHasPriority() { has_priority_++; }

  short GetHasCode() { return has_code_; }
  short GetHasPriority() { return has_priority_; }
  short GetHasLast() { return has_last_; }
  int GetInFor() { return in_for_; }

  bool IsLtlMode() { return ltl_mode_; }
  void SetLtlMode(bool ltl_mode) {
    if (ltl_mode) {
      has_ltl_ = true;
    }
    ltl_mode_ = ltl_mode;
  }
  bool HasLtl() { return has_ltl_; }
  int GetInSeq() { return in_seq_; }
  void SetInSeq(int in_seq) { in_seq_ = in_seq; }
  int GetImpliedSemis() { return implied_semis_; }
  void SetImpliedSemis(int implied_semis) { implied_semis_ = implied_semis; }

private:
  int pre_proc();
  void do_directive(int first_char);
  int CheckName(const std::string &value);
  bool ScatTo(int stop, int (*Predicate)(int), std::string &buf,
              int max_size = 0);
  bool ScatTo(int stop);
  int Follow(int token, int ifyes, int ifno);

  file::FileStream stream_;
  ScopeProcessor& scope_;
  ::helpers::Deferred deferred_;

  std::vector<std::string> inline_arguments_;
  std::size_t curr_inline_argument_;
  std::size_t argument_nesting_;
  int last_token_;
  bool pp_mode_;

  std::string temp_hold_;
  int temp_has_;

  int parameter_count_;
  int has_last_;
  short has_code_;     /* spec contains c_code, c_expr, c_state */
  short has_priority_; /* spec refers to _priority */
  int in_for_;
  unsigned char in_comment_;
  bool ltl_mode_; /* set when parsing an ltl formula */
  bool has_ltl_;
  int implied_semis_;
  int in_seq_;
};
} // namespace lexer