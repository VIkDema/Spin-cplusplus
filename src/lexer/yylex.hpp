#pragma once
#include <string>
/**
 * @brief Performs lexical analysis on the input.
 * @return The token value produced by the lexical analysis.
 *
 * The yylex function is responsible for performing lexical analysis on the
 * input source code. It scans the input and generates tokens that are used by
 * the parser to parse the source code.
 */
int yylex();

/**
 * @brief Generates an LTL list with the given name and formula.
 * @param nm The name of the LTL list.
 * @param fm The formula associated with the LTL list.
 *
 * The ltl_list function generates an LTL (Linear Temporal Logic) list with the
 * specified name and formula. It is used to create LTL lists that can be
 * processed further in the application.
 */
void ltl_list(const std::string &nm, const std::string &fm);
