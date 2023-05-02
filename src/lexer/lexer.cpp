#include "lexer.hpp"

#include "../spin.hpp"
#include "/Users/vikdema/Desktop/projects/Spin/src++/build/y.tab.h"
#include "helpers.hpp"
#include "names.hpp"
#include <algorithm>
#include <fmt/core.h>
#include <iostream>
#include <optional>

extern std::string yytext;
extern Symbol *Fname, *oFname;
extern YYSTYPE yylval;

#define ValToken(x, y)                                                         \
  {                                                                            \
    if (in_comment_)                                                           \
      goto again;                                                              \
    yylval = nn(ZN, 0, ZN, ZN);                                                \
    yylval->val = x;                                                           \
    last_token_ = y;                                                           \
    return y;                                                                  \
  }
#define SymToken(x, y)                                                         \
  {                                                                            \
    if (in_comment_)                                                           \
      goto again;                                                              \
    yylval = nn(ZN, 0, ZN, ZN);                                                \
    yylval->sym = x;                                                           \
    last_token_ = y;                                                           \
    return y;                                                                  \
  }

namespace lexer {
Lexer::Lexer()
    : inline_arguments_({}), curr_inline_argument_(0), argument_nesting_(0),
      last_token_(0), pp_mode_(false), temp_has_(0), parameter_count_(0),
      has_last_(0), has_code_(0), has_priority_(0), in_for_(0), in_comment_(0),
      ltl_mode_(false), has_ltl_(false) {}
Lexer::Lexer(bool pp_mode)
    : inline_arguments_({}), curr_inline_argument_(0), argument_nesting_(0),
      last_token_(0), pp_mode_(pp_mode), temp_has_(0), parameter_count_(0),
      has_last_(0), has_code_(0), has_priority_(0), in_for_(0), in_comment_(0),
      ltl_mode_(false), has_ltl_(false) {}

void Lexer::SetLastToken(int last_token) { last_token_ = last_token; }
int Lexer::GetLastToken() { return last_token_; }

void Lexer::add_inline_argument() {
  inline_arguments_.push_back(std::string{});
}
void Lexer::update_inline_argument(const std::string &additional) {
  inline_arguments_[curr_inline_argument_] += additional;
}
void Lexer::inc_index_argument() { curr_inline_argument_++; }

void Lexer::inc_inline_nesting() { argument_nesting_++; }
void Lexer::des_inline_nesting() { argument_nesting_--; }
std::size_t Lexer::get_inline_nesting() { return argument_nesting_; }

int Lexer::CheckName(const std::string &value) {

  yylval = nn(ZN, 0, ZN, ZN);

  if (ltl_mode_) {
    auto opt_token = ::helpers::ParseLtlToken(value);
    if (opt_token.has_value()) {
      return opt_token.value();
    }
  }

  auto opt_name = ::helpers::ParseNameToken(value);
  if (opt_name.has_value()) {
    yylval->val = opt_name->value;
    if (opt_name->symbol.has_value()) {
      std::string symbol_copy{opt_name->symbol.value()};
      yylval->sym = lookup(symbol_copy.data());
    }

    if (!(opt_name->token == IN && !in_for_)) {
      return opt_name->token;
    }
  }

  if ((yylval->val = ismtype(value)) != 0) {
    yylval->ismtyp = 1;
    yylval->sym = (Symbol *)emalloc(sizeof(Symbol));
    yylval->sym->name = (char *)emalloc(value.length() + 1);
    strcpy(yylval->sym->name, value.c_str());
    return CONST;
  }

  if (value == "_last") {
    has_last_++;
  }

  if (value == "_priority") {
    IncHasPriority();
  }

  if (stream_.HasInlining()) {

    auto inline_stub = stream_.GetInlineStub(stream_.GetInlining());

    Lextok *tt, *t = inline_stub->params;

    for (int i = 0; t; t = t->rgt, i++) /* formal pars */
    {
      if (value == std::string(t->lft->sym->name) &&
          value != std::string(inline_stub[stream_.GetInlining()].anms[i])) {
        continue;
      }
      for (tt = inline_stub[stream_.GetInlining()].params; tt; tt = tt->rgt) {
        if (std::string(inline_stub[stream_.GetInlining()].anms[i]) !=
            std::string(tt->lft->sym->name)) {
          continue;
        }
        std::cout << fmt::format("spin: {}:{} replacement value: {}",
                                 oFname->name ? oFname->name : "--",
                                 stream_.GetLineNumber(), tt->lft->sym->name)
                  << std::endl;

        log::fatal("formal par of %s contains replacement value",
                   inline_stub[stream_.GetInlining()].nm->name);
        yylval->ntyp = tt->lft->ntyp;
        yylval->sym = lookup(tt->lft->sym->name);
        return NAME;
      }

      /* check for occurrence of param as field of struct */
      {
        char *ptr = inline_stub[stream_.GetInlining()].anms[i];
        char *optr = ptr;
        while ((ptr = strstr(ptr, value.c_str())) != nullptr) {
          if ((ptr > optr && *(ptr - 1) == '.') ||
              *(ptr + value.size()) == '.') {
            log::fatal("formal par of %s used in structure name",
                       inline_stub[stream_.GetInlining()].nm->name);
          }
          ptr++;
        }
      }
      stream_.SetReDiRect(inline_stub[stream_.GetInlining()].anms[i]);
      return 0;
    }
  }
  std::string value_copy = value;
  yylval->sym = lookup(value_copy.data()); /* symbol table */
  if (IsUtype(value)) {
    return UNAME;
  }
  if (IsProctype(value)) {
    return PNAME;
  }
  if (IsEqname(value)) {
    return INAME;
  }

  return NAME;
}

int Lexer::Follow(int token, int ifyes, int ifno) {
  int curr;

  if ((curr = stream_.GetChar()) == token) {
    return ifyes;
  }
  stream_.Ungetch(curr);

  return ifno;
}

int Lexer::pre_proc() {
  std::string pre_proc_command;
  int curr = 0;

  pre_proc_command += '#';
  while ((curr = stream_.GetChar()) != '\n' && curr != EOF) {
    pre_proc_command += (char)curr;
  }

  yylval = nn(ZN, 0, ZN, ZN);
  yylval->sym = lookup(pre_proc_command.data());

  return PREPROC;
}

void Lexer::do_directive(int first_char) {
  int new_char = first_char; /* handles lines starting with pound */

  yytext = stream_.GetWord(new_char, helpers::isalpha_);

  if (yytext == "#ident") {
    goto done;
  }

  if ((new_char = stream_.GetChar()) != ' ') {
    log::fatal("malformed preprocessor directive - # .");
  }

  if (!helpers::isdigit_(new_char = stream_.GetChar())) {
    log::fatal("malformed preprocessor directive - # .lineno");
  }

  yytext = stream_.GetWord(new_char, helpers::isdigit_);

  stream_.SetLineNumber(std::stoi(yytext));

  if ((new_char = stream_.GetChar()) == '\n') {
    return; /* no filename */
  }

  if (new_char != ' ') {
    log::fatal("malformed preprocessor directive - .fname");
  }

  if ((new_char = stream_.GetChar()) != '\"') {
    std::cout << fmt::format("got {}, expected \" -- lineno {}", new_char,
                             stream_.GetLineNumber())
              << std::endl;
    log::fatal("malformed preprocessor directive - .fname (%s)", yytext);
  }

  yytext = stream_.GetWord(stream_.GetChar(), helpers::IsNotQuote);

  if (stream_.GetChar() != '\"') {
    log::fatal("malformed preprocessor directive - fname.");
  }

  Fname = lookup(yytext.data());
done:
  while (stream_.GetChar() != '\n') {
  };
}

bool Lexer::ScatTo(int stop) {
  int curr = 0;

  do {
    curr = stream_.GetChar();

    if (temp_has_ < temp_hold_.size()) {
      temp_hold_[temp_has_++] = curr;
    }

    if (curr == '\n') {
      stream_.IncLineNumber();
    }
  } while (curr != stop && curr != EOF);

  if (curr != stop) {
    if (temp_has_ < temp_hold_.size()) {
      stream_.push_back(temp_hold_);
      return false; /* internal expansion fails */
    } else {
      log::fatal("expecting select ( name : constant .. constant )");
    }
  }
  return true; /* success */
}

bool Lexer::ScatTo(int stop, int (*Predicate)(int), std::string &buf,
                   int max_size) {
  int curr, i = 0;

  do {
    curr = stream_.GetChar();

    if (temp_has_ < temp_hold_.size()) {
      temp_hold_[temp_has_++] = curr;
    }

    if (curr == '\n') {
      stream_.IncLineNumber();
    } else if (i < max_size - 1) {
      buf.push_back(curr);
    } else if (i >= max_size - 1) {
      log::fatal("name too long", buf);
    }

    if (Predicate && !Predicate(curr) && curr != ' ' && curr != '\t') {
      break;
    }

  } while (curr != stop && curr != EOF);

  if (i <= 0) {
    log::fatal("input error");
  }

  if (curr != stop) {
    if (temp_has_ < temp_hold_.size()) {
      stream_.push_back(temp_hold_);
      return false; /* internal expansion fails */
    } else {
      log::fatal("expecting select ( name : constant .. constant )");
    }
  }
  return true; /* success */
}

int Lexer::lex() {
  int new_char;
again:
  new_char = stream_.GetChar();

  yytext = (char)new_char;

  switch (new_char) {
  case EOF: {
    if (!deferred_.IsDiferred()) {
      deferred_.SetDiferred(true);
      if (deferred_.GetDeferred()) {
        goto again;
      }
    } else {
      deferred_.ZapDeferred();
    }
    return new_char;
  }
  case '\n': // new_line
  {
    stream_.IncLineNumber();
    if (parameter_count_ == 0 && helpers::IsFollowsToken(last_token_)) {
      if (last_token_ == '}') {
        do {
          new_char = stream_.GetChar();
          if (new_char == '\n') {
            stream_.IncLineNumber();
          }
        } while (helpers::IsWhitespace(new_char));
        stream_.Ungetch(new_char);
        if (new_char == 'u') {
          goto again;
        }
      }
      ValToken(1, SEMI);
    }
  }
  case '\r':
  case ' ':
  case '\t':
  case '\f': {
    goto again;
  }
  case '#': /* preprocessor directive */
  {
    if (in_comment_)
      goto again;
    if (pp_mode_) {
      last_token_ = PREPROC;
      return pre_proc();
    }
    do_directive(new_char);
    goto again;
  }
  case '\"': {
    yytext = stream_.GetWord(new_char, helpers::IsNotQuote);
    if (stream_.GetChar() != '\"') {
      log::fatal("string not terminated", yytext);
    }
    yytext += "\"";
    { SymToken(lookup(yytext.data()), STRING) }
  }
  case '$': {
    yytext = stream_.GetWord(new_char, helpers::IsNotDollar);
    if (stream_.GetChar() != '$') {
      log::fatal("ltl definition not terminated", yytext);
    }
    yytext += "\"";
    { SymToken(lookup(yytext.data()), STRING) }
  }
  case '\'': {
    new_char = stream_.GetChar();
    if (new_char == '\\') {
      new_char = stream_.GetChar();
      if (new_char == 'n') {
        new_char = '\n';
      } else if (new_char == 'r') {
        new_char = '\r';
      } else if (new_char == 't') {
        new_char = '\t';
      } else if (new_char == 'f') {
        new_char = '\f';
      }
    }
    if (stream_.GetChar() != '\'' && !in_comment_)
      log::fatal("character quote missing: %s", yytext);
    { ValToken(new_char, CONST) }
  }
  default:
    break;
  }

  if (helpers::isdigit_(new_char)) {
    long int nr;
    yytext = stream_.GetWord(new_char, helpers::isdigit_);
    try {
      std::size_t pos;
      nr = std::stol(yytext, &pos, 10);
      if (pos != yytext.length()) {
        throw std::invalid_argument("spin: value out of range");
      }
    } catch (const std::invalid_argument &e) {
      std::cerr << e.what() << ": " << yytext << std::endl;
    }
    ValToken((int)nr, CONST)
  }

  if (helpers::isalpha_(new_char) || new_char == '_') {
    yytext = stream_.GetWord(new_char, helpers::isalnum_);
    if (!in_comment_) {
      new_char = CheckName(yytext);
      if (new_char == TIMEOUT && stream_.GetInlining() < 0 &&
          last_token_ != '(') {
        stream_.push_back("timeout)");
        last_token_ = '(';
        return '(';
      }
      if (new_char == SELECT && stream_.GetInlining() < 0) {
        std::string name, from, upto;
        int i;
        temp_hold_.clear();
        temp_has_ = false;
        if (!ScatTo('(') && !ScatTo(':', helpers::isalpha_, name, 64) &&
            !ScatTo('.', helpers::isdigit_, from, 32) && !ScatTo('.') &&
            !ScatTo(')', helpers::isdigit_, upto, 32)) {
          goto not_expanded;
        }
        int a = std::stoi(from);
        int b = std::stoi(upto);
        if (a > b) {
          log::non_fatal("bad range in select statement");
          goto again;
        }
        if (b - a <= 32) {
          stream_.push_back("if ");
          for (i = a; i <= b; i++) {
            stream_.push_back(":: ");
            std::string buf = fmt::format("{} = {}", name, i);
            stream_.push_back(buf);
          }
          stream_.push_back("fi ");
        } else {
          std::string buf = fmt::format("{} = {}; do ", name, a);
          stream_.push_back(buf);
          buf = fmt::format(":: ({} < {}) -> {}++ ", name, b, name);
          stream_.push_back(buf);
          stream_.push_back(":: break od; ");
        }
        goto again;
      }
    not_expanded:
      if (new_char == LTL && !deferred_.GetDeferred()) {
        if (deferred_.PutDeffered()) {
          goto again;
        }
      }
      if (new_char) {
        last_token_ = new_char;
        return new_char;
      }
    }
    goto again;
  }

  if (ltl_mode_) {
    switch (new_char) {
    case '-':
      new_char = Follow('>', IMPLIES, '-');
      break;
    case '[':
      new_char = Follow(']', ALWAYS, '[');
      break;
    case '/':
      new_char = Follow('\\', AND, '/');
      break;
    case '\\':
      new_char = Follow('/', OR, '\\');
      break;
    case '<':
      new_char = Follow('>', EVENTUALLY, '<');
      if (new_char != '<') {
        break;
      }
      new_char = stream_.GetChar();
      if (new_char == '-') {
        new_char = Follow('>', EQUIV, '-');
        if (new_char == '-') {
          stream_.Ungetch(new_char);
          new_char = '<';
        }
      } else {
        stream_.Ungetch(new_char);
        new_char = '<';
      }
    default:
      break;
    }
  }

  switch (new_char) {
  case '/':
    new_char = Follow('*', 0, '/');
    if (!new_char) {
      in_comment_ = 1;
      goto again;
    }
    break;
  case '*':
    new_char = Follow('/', 0, '*');
    if (!new_char) {
      in_comment_ = 0;
      goto again;
    }
    break;
  case ':':
    new_char = Follow(':', SEP, ':');
    break;
  case '-':
    new_char = Follow('>', ARROW, Follow('-', DECR, '-'));
    break;
  case '+':
    new_char = Follow('+', INCR, '+');
    break;
  case '<':
    new_char = Follow('<', LSHIFT, Follow('=', LE, LT));
    break;
  case '>':
    new_char = Follow('>', RSHIFT, Follow('=', GE, GT));
    break;
  case '=':
    new_char = Follow('=', EQ, ASGN);
    break;
  case '!':
    new_char = Follow('=', NE, Follow('!', O_SND, SND));
    break;
  case '?':
    new_char = Follow('?', R_RCV, RCV);
    break;
  case '&':
    new_char = Follow('&', AND, '&');
    break;
  case '|':
    new_char = Follow('|', OR, '|');
    break;
  case ';':
    new_char = SEMI;
    break;
  case '.':
    new_char = Follow('.', DOTDOT, '.');
    break;
  case '{':
    scope_.AddScope();
    scope_.SetCurrScope();
    break;
  case '}':
    scope_.RemoveScope();
    scope_.SetCurrScope();
    break;
  default:
    break;
  }

  ValToken(0, new_char);
}

} // namespace lexer
