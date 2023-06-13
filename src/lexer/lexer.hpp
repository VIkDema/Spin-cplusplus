#pragma once
#include "deferred.hpp"
#include "file_stream.hpp"
#include "scope.hpp"
#include <optional>
#include <string>
#include <vector>

namespace lexer {
/**
 * @brief Class responsible for lexical analysis.
 */
class Lexer {
public:
  /**
   * @brief Default constructor.
   */
  Lexer();
  /**
   * @brief Constructor with preprocessor mode option.
   * @param pp_mode Whether the lexer is in preprocessor mode.
   */
  Lexer(bool pp_mode);
  /**
   * @brief Sets the last token value.
   * @param last_token The value of the last token.
   */
  void SetLastToken(int last_token);
  /**
   * @brief Gets the last token value.
   * @return The value of the last token.
   */
  int GetLastToken();
  /**
   * @brief Performs lexical analysis.
   * @return The next token.
   */
  int lex();
  /**
   * @brief Adds an inline argument.
   */
  void add_inline_argument();
  /**
   * @brief Updates the inline argument with additional content.
   * @param additional The additional content to update the inline argument.
   */
  void update_inline_argument(const std::string &additional);
  /**
   * @brief Increments the index of the inline argument.
   */
  void inc_index_argument();
  /**
   * @brief Increments the inline nesting level.
   */
  void inc_inline_nesting();
  /**
   * @brief Decrements the inline nesting level.
   */
  void des_inline_nesting();
  /**
   * @brief Gets the current inline nesting level.
   * @return The current inline nesting level.
   */
  std::size_t get_inline_nesting();
  /**
   * @brief Increments the parameter count.
   */
  void inc_parameter_count() { parameter_count_++; }
  /**
   * @brief Decrements the parameter count.
   */
  void des_parameter_count() { parameter_count_--; }
  /**
   * @brief Sets the has_code flag.
   * @param has_code The value of the has_code flag.
   */
  void SetHasCode(int has_code) { has_code_ = has_code; }
  /**
   * @brief Sets the has_priority flag.
   * @param has_priority The value of the has_priority flag.
   */
  void SetHasPriority(int has_priority) { has_priority_ = has_priority; }
  /**
   * @brief Sets the in_for flag.
   * @param in_for The value of the in_for flag.
   */
  void SetInFor(int in_for) { in_for_ = in_for; }
  /**
   * @brief Increments the has_priority value.
   */
  void IncHasPriority() { has_priority_++; }
  /**
   * @brief Gets the has_code flag.
   * @return The value of the has_code flag.
   */
  short GetHasCode() { return has_code_; }
  /**
   * @brief Gets the has_priority flag.
   * @return The value of the has_priority flag.
   */
  short GetHasPriority() { return has_priority_; }
  /**
   * @brief Gets the has_last flag.
   * @return The value of the has_last flag.
   */
  short GetHasLast() { return has_last_; }

  /**
   * @brief Gets the in_for flag.
   * @return The value of the in_for flag.
   */
  int GetInFor() { return in_for_; }
  /**
   * @brief Checks if the lexer is in LTL mode.
   * @return True if the lexer is in LTL mode, false otherwise.
   */
  bool IsLtlMode() { return ltl_mode_; }
  /**
   * @brief Sets the LTL mode flag.
   * @param ltl_mode The value of the LTL mode flag.
   */
  void SetLtlMode(bool ltl_mode) {
    if (ltl_mode) {
      has_ltl_ = true;
    }
    ltl_mode_ = ltl_mode;
  }
  /**
   * @brief Checks if the lexer has encountered an LTL formula.
   * @return True if an LTL formula has been encountered, false otherwise.
   */
  bool HasLtl() { return has_ltl_; }
  /**
   * @brief Gets the in_seq value.
   * @return The value of the in_seq flag.
   */
  int GetInSeq() { return in_seq_; }
  /**
   * @brief Sets the in_seq value.
   * @param in_seq The value of the in_seq flag.
   */
  void SetInSeq(int in_seq) { in_seq_ = in_seq; }
  /**
   * @brief Gets the implied_semis value.
   * @return The value of the implied_semis flag.
   */
  int GetImpliedSemis() { return implied_semis_; }
  /**
   * @brief Sets the implied_semis value.
   * @param implied_semis The value of the implied_semis flag.
   */
  void SetImpliedSemis(int implied_semis) { implied_semis_ = implied_semis; }
  /**
   * @brief Handles inline symbols.
   * @param s The symbol to handle.
   * @param nms The namespace of the symbol.
   * @return The modified symbol.
   */
  models::Symbol *HandleInline(models::Symbol *s, models::Lextok *nms);
  /**
   * @brief Returns a statement.
   * @param Lextok The statement to return.
   * @return The modified statement.
   */
  models::Lextok *ReturnStatement(models::Lextok *Lextok);
  /**
   * @brief Picks up inline arguments.
   * @param t The symbol.
   * @param apars The argument parameters.
   * @param rval The return value.
   */
  void PickupInline(models::Symbol *t, models::Lextok *apars,
                    models::Lextok *rval);

private:
  /**
   * @brief Performs preprocessing.
   * @return The next token.
   */
  int pre_proc();
  /**
   * @brief Performs preprocessing for directives.
   * @param first_char The first character of the directive.
   */
  void do_directive(int first_char);
  /**
   * @brief Checks if a name is valid.
   * @param value The name to check.
   * @return The token type of the name.
   */
  int CheckName(const std::string &value);
  /**
   * @brief Scans until a specified character is found.
   * @param stop The stop character.
   * @param Predicate The predicate function to check each character.
   * @param buf The buffer to store the scanned characters.
   * @param max_size The maximum size of the buffer (optional).
   * @return True if the stop character is found, false otherwise.
   */
  bool ScatTo(int stop, int (*Predicate)(int), std::string &buf,
              int max_size = 0);
  /**
   * @brief Scans until a specified character is found.
   * @param stop The stop character.
   * @return True if the stop character is found, false otherwise.
   */
  bool ScatTo(int stop);
  /**
   * @brief Skips tokens based on a condition.
   * @param token The current token.
   * @param ifyes The token to return if the condition is true.
   * @param ifno The token to return if the condition is false.
   * @return The next token based on the condition.
   */
  int Follow(int token, int ifyes, int ifno);
  /**
   * @brief Handles preconditions.
   * @param q The precondition string.
   */
  void Precondition(char *q);
  /**
   * @brief Defines an inline symbol.
   * @param s The symbol to define.
   * @param ln The line number.
   * @param ptr The pointer to the symbol.
   * @param prc The procedure.
   * @param nms The namespace.
   */
  void DefInline(models::Symbol *s, int ln, char *ptr, char *prc,
                 models::Lextok *nms);

  file::FileStream stream_; /**< The file stream used for lexical analysis. */
  ::helpers::Deferred deferred_; /**< The deferred flag. */

  std::vector<std::string>
      inline_arguments_;             /**< The list of inline arguments. */
  std::size_t curr_inline_argument_; /**< The current inline argument index. */
  std::size_t argument_nesting_; /**< The nesting level of inline arguments. */
  int last_token_;               /**< The value of the last token. */
  bool pp_mode_;                 /**< The preprocessor mode flag. */

  std::string temp_hold_; /**< Temporary hold string. */
  int temp_has_;          /**< Temporary has value. */

  int parameter_count_;      /**< The parameter count. */
  int has_last_;             /**< The has_last flag. */
  short has_code_;           /**< The has_code flag. */
  short has_priority_;       /**< The has_priority flag. */
  int in_for_;               /**< The in_for flag. */
  unsigned char in_comment_; /**< The in_comment flag. */
  bool ltl_mode_;            /**< The LTL mode flag. */
  bool has_ltl_;             /**< The has_ltl flag. */
  int implied_semis_;        /**< The implied_semis value. */
  int in_seq_;               /**< The in_seq flag. */
};
} // namespace lexer