#pragma once
#include <string>

namespace helpers {

int isdigit_(int curr);
int isalpha_(int curr);
int isalnum_(int curr);

int IsNotQuote(int curr);

bool IsFollowsToken(int curr_token);

int IsNotDollar(int curr);

bool IsWhitespace(int curr);

std::string SkipWhite(const std::string &p);

std::string SkipNonwhite(const std::string &p);

int PutType(std::string &foo, int type);

int PrintType(int type);

} // namespace helpers