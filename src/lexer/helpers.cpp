#include "/Users/vikdema/Desktop/projects/Spin/src++/build/y.tab.h"
#include <algorithm>
#include <array>
#include <ctype.h>

namespace lexer::helpers {

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

} // namespace lexer::helpers