#include "helpers.hpp"

#include "../fatal/fatal.hpp"
#include "../models/symbol.hpp"
#include "y.tab.h"
#include <algorithm>
#include <array>
#include <ctype.h>
#include <iostream>

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

int PutType(std::string &foo, int type) {
  switch (type) {
  case models::SymbolType::kUnsigned:
    foo.append("unsigned ");
    break;
  case models::SymbolType::kBit:
    foo.append("bit   ");
    break;
  case models::SymbolType::kByte:
    foo.append("byte  ");
    break;
  case models::SymbolType::kChan:
    foo.append("chan  ");
    break;
  case models::SymbolType::kShort:
    foo.append("short ");
    break;
  case models::SymbolType::kInt:
    foo.append("int   ");
    break;
  case models::SymbolType::kMtype:
    foo.append("mtype ");
    break;
  case models::SymbolType::kStruct:
    foo.append("struct");
    break;
  case models::SymbolType::kProctype:
    foo.append("proctype");
    break;
  case models::SymbolType::kLabel:
    foo.append("label ");
    return 0;
  default:
    foo.append("value ");
    return 0;
  }
  return 1;
}
int PrintType(int type) {
  std::string buf;
  if (helpers::PutType(buf, type)) {
    std::cout << buf;
    return 1;
  }
  return 0;
}

} // namespace helpers