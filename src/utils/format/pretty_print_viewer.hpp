#pragma once
#include <string>

namespace format {
/**
 * @class PrettyPrintViewer
 * @brief The PrettyPrintViewer class provides a mechanism for pretty-printing
 * code with proper indentation and line breaks.
 */
class PrettyPrintViewer {
public:
  /**
   * @brief Performs the pretty-printing of the code.
   */
  void view();

private:
  /**
   * @brief Starts a new line with proper indentation and handles special cases.
   * @param buf The buffer containing the current line.
   */
  void start_new_line(std::string &buf);
  /**
   * @brief Maps a token to its corresponding string representation.
   * @param n The token.
   * @param buf The buffer to store the mapped string.
   */
  void map_token_to_string(int n, std::string &buf);

  void doindent();
  /**
   * @brief Decreases the current indentation level.
   */
  void decrease_indentation();
  /**
   * @brief Increases the current indentation level.
   */
  void increase_indentation();
  /**
   * @brief Determines whether a new line should be started based on the current
   * and last token.
   * @param current_token The current token.
   * @param last_token The last token.
   * @return True if a new line should be started, False otherwise.
   */
  bool should_start_new_line(int current_token, int last_token);
  /**
   * @brief Appends a space to the buffer if necessary based on the current and
   * last token.
   * @param current_token The current token.
   * @param last_token The last token.
   * @param buffer The buffer to append to.
   */
  void append_space_if_needed(int current_token, int last_token,
                              std::string &buffer);
  int in_decl = 0;
  int in_c_decl = 0;
  int in_c_code = 0;
  int indent = 0;
};
} // namespace format