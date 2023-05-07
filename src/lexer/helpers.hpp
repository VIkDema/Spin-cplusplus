#pragma once
namespace lexer::helpers {

int isdigit_(int curr);
int isalpha_(int curr);
int isalnum_(int curr);

int IsNotQuote(int curr);

bool IsFollowsToken(int curr_token);

int IsNotDollar(int curr);

bool IsWhitespace(int curr);
} // namespace lexer::helpers