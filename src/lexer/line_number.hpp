#pragma once

namespace file {
class LineNumber {
public:
  /**
   * @brief Increments the line number by 1.
   */
  static void Inc();

  /**
   * @brief Sets the line number to a new value.
   * @param new_line_number The new line number.
   */
  static void Set(int new_line_number);

  /**
   * @brief Retrieves the current line number.
   * @return The current line number.
   */
  static int Get();

private:
  static int line_number_; /**< The current line number. */
};
} // namespace file