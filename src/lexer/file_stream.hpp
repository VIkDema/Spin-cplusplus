#pragma once

#include "../fatal/fatal.hpp"
#include "../models/itype.hpp"
#include "../spin.hpp"
#include <string>

namespace file {
/**
 * @brief Class representing a file stream.
 */
class FileStream {
public:
  /**
   * @brief Constructs a FileStream object.
   */
  FileStream();

  /**
   * @brief Gets the next character from the stream.
   * @return The next character from the stream.
   */
  int GetChar();

  /**
   * @brief Gets the next word from the stream.
   * @param size The maximum size of the word.
   * @param delimit A function pointer to a delimiter check function.
   * @return The next word from the stream.
   */
  std::string GetWord(int size, int (*delimit)(int));

  /**
   * @brief Pushes a character back into the stream.
   * @param curr The character to be pushed back.
   */
  void Ungetch(int curr);

  /**
   * @brief Pushes a string back into the stream.
   * @param value The string to be pushed back.
   */
  void push_back(const std::string &value) {
    if (pushed_back_ + value.size() > 4094) {
      loger::fatal("select statement too large");
    }
    pushed_back_stream_ += value;
    pushed_back_ += value.size();
  }

private:
  /**
   * @brief The number of characters pushed back into the stream.
   */
  int push_back_;

  /**
   * @brief The total number of characters pushed back into the stream.
   */
  int pushed_back_;

  /**
   * @brief The stream of pushed back characters.
   */
  std::string pushed_back_stream_;
};

} // namespace file