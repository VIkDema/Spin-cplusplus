#pragma once
#include <string>

/**
 * @brief Namespace containing helper functions for character and string manipulation.
 *
 * The `helpers` namespace provides a collection of functions that assist in character and string manipulation operations.
 */
namespace helpers {

/**
 * @brief Checks if the given character is a digit.
 * @param curr The character to check.
 * @return `true` if the character is a digit, `false` otherwise.
 */
int isdigit_(int curr);

/**
 * @brief Checks if the given character is an alphabetic character.
 * @param curr The character to check.
 * @return `true` if the character is an alphabetic character, `false` otherwise.
 */
int isalpha_(int curr);

/**
 * @brief Checks if the given character is an alphanumeric character.
 * @param curr The character to check.
 * @return `true` if the character is an alphanumeric character, `false` otherwise.
 */
int isalnum_(int curr);

/**
 * @brief Checks if the given character is not a quote character.
 * @param curr The character to check.
 * @return `true` if the character is not a quote character, `false` otherwise.
 */
int IsNotQuote(int curr);

/**
 * @brief Checks if the given character is not a dollar character.
 * @param curr The character to check.
 * @return `true` if the character is not a dollar character, `false` otherwise.
 */
int IsNotDollar(int curr);

/**
 * @brief Checks if the given character is a whitespace character.
 * @param curr The character to check.
 * @return `true` if the character is a whitespace character, `false` otherwise.
 */
bool IsWhitespace(int curr);

bool IsFollowsToken(int curr_token);

/**
 * @brief Skips over white space characters in the given string.
 * @param p The input string.
 * @return A new string with leading white space characters skipped.
 */
std::string SkipWhite(const std::string &p);

/**
 * @brief Skips over non-white space characters in the given string.
 * @param p The input string.
 * @return A new string with leading non-white space characters skipped.
 */
std::string SkipNonwhite(const std::string &p);

/**
 * @brief Writes the type of a token to the given string.
 * @param foo The string to write the type to.
 * @param type The token type.
 * @return The number of characters written to the string.
 */
int PutType(std::string &foo, int type);

/**
 * @brief Prints the type of a token to the standard output.
 * @param type The token type.
 * @return The number of characters printed.
 */
int PrintType(int type);

} // namespace helpers
