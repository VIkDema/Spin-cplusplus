#include "helpers.hpp"

#include "../fatal/fatal.hpp"
#include "/Users/vikdema/Desktop/projects/Spin/src++/build/y.tab.h"
#include <algorithm>
#include <array>
#include <ctype.h>

namespace helpers {

constexpr std::array<int, 15> kSpecials = {'}',  ')',   ']',    OD,     FI,
                                           ELSE, BREAK, C_CODE, C_EXPR, C_DECL,
                                           NAME, CONST, INCR,   DECR,   0};

int isdigit_(int curr) { return isdigit(curr); }
int isalpha_(int curr) { return isalpha(curr); }
int isalnum_(int curr) { return (isalnum(curr) || curr == '_'); }

int IsNotQuote(int curr) { return (curr != '\"' && curr != '\n'); }

bool IsFollowsToken(int curr_token) {
  auto it = std::find(kSpecials.begin(), kSpecials.end(), curr_token);
  return it != kSpecials.end();
}

int IsNotDollar(int curr) { return (curr != '$' && curr != '\n'); }

bool IsWhitespace(int curr) {
  return curr == ' ' || curr == '\t' || curr == '\f' || curr == '\n' ||
         curr == '\r';
}

std::string SkipWhite(const std::string &p) {
  std::string::size_type i = 0;
  while (i < p.length() && (p[i] == ' ' || p[i] == '\t')) {
    i++;
  }
  if (i == p.length()) {
    loger::fatal("bad format - 1");
  }
  return p.substr(i);
}

std::string SkipNonwhite(const std::string &p) {
  std::string::size_type i = 0;
  while (i < p.length() && (p[i] != ' ' && p[i] != '\t')) {
    i++;
  }
  if (i == p.length()) {
    loger::fatal("bad format - 2");
  }
  return p.substr(i);
}
} // namespace helpers