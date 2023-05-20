#include "yylex.hpp"

#include "../main/launch_settings.hpp"
#include "../spin.hpp"
#include "y.tab.h"
#include "lexer.hpp"
#include <array>
#include <fmt/core.h>

extern std::string yytext;
extern int need_arguments;
extern YYSTYPE yylval;
extern models::Symbol *owner;
lexer::Lexer lexer_;
extern LaunchSettings launch_settings;

namespace {
constexpr std::array<int, 3> kConditionElse = {SEMI, ARROW, FI};
constexpr std::array<int, 11> kConditionGuillement = {
    PROCTYPE, INIT, CLAIM, SEP, FI, OD, '}', UNLESS, SEMI, ARROW, EOF};

std::string TokenToString(int temp_token) {
  switch (temp_token) {
  case ARROW:
    return "->";
  case SEP:
    return "::";
  case SEMI:
    return ";";
  case DECR:
    return "--";
  case INCR:
    return "++";
  case LSHIFT:
    return "<<";
  case RSHIFT:
    return ">>";
  case LE:
    return "<=";
  case LT:
    return "<";
  case GE:
    return ">=";
  case GT:
    return ">";
  case EQ:
    return "==";
  case ASGN:
    return "=";
  case NE:
    return "!=";
  case R_RCV:
    return "??";
  case RCV:
    return "?";
  case O_SND:
    return "!!";
  case SND:
    return "!";
  case AND:
    return "&&";
  case OR:
    return "||";
  }
}

} // namespace
int yylex() {
  static int last_token = 0;
  static int hold_token = 0;

  int temp_token;

  if (hold_token) {
    temp_token = hold_token;
    hold_token = 0;
    lexer_.SetLastToken(temp_token);
  } else {
    temp_token = lexer_.lex();
    if ((last_token == ELSE &&
         std::find(kConditionElse.begin(), kConditionElse.end(), temp_token) ==
             kConditionElse.end()) ||
        (last_token == '}' &&
         std::find(kConditionGuillement.begin(), kConditionGuillement.end(),
                   temp_token) == kConditionGuillement.end())) {
      hold_token = temp_token;
      last_token = 0;
      lexer_.SetLastToken(SEMI);
      return SEMI;
    }

    if (temp_token == SEMI || temp_token == ARROW) {
      if (models::Symbol::GetContext()) {
        owner = ZS;
      }
      hold_token = lexer_.lex();
      if (hold_token == '}' || hold_token == ARROW || hold_token == SEMI) {
        temp_token = hold_token;
        hold_token = 0;
      }
    }
  }

  last_token = temp_token;

  if (need_arguments) {

    if (yytext == ",") {

      lexer_.add_inline_argument();
      lexer_.inc_index_argument();

    } else if (yytext == "(") {

      lexer_.inc_index_argument();

      if (lexer_.get_inline_nesting() == 0) {
        lexer_.add_inline_argument();
      } else {
        lexer_.update_inline_argument(yytext);
      }
    } else if (yytext == ")") {

      if (lexer_.get_inline_nesting() > 0) {
        lexer_.update_inline_argument(yytext);
      }

      lexer_.des_inline_nesting();

    } else if (temp_token == CONST && yytext[0] == '\'') {

      yytext = fmt::format("\'{}\'", (char)yylval->value);
      lexer_.update_inline_argument(yytext);

    } else if (temp_token == CONST) {

      yytext = fmt::format("{}", yylval->value);
      lexer_.update_inline_argument(yytext);

    } else {

      yytext = TokenToString(temp_token);
      lexer_.update_inline_argument(yytext);
    }
  }
  return temp_token;
}

void ltl_list(const std::string &nm, const std::string &fm) {
  extern char *ltl_claims;
  extern FILE *tl_out;
  extern FILE *fd_ltl;
  extern int nr_errs;
  if (launch_settings.need_save_trail || launch_settings.need_to_analyze ||
      launch_settings.need_produce_symbol_table_information) {

    if (!ltl_claims) {
      ltl_claims = "_spin_nvr.tmp";
      if ((fd_ltl = fopen(ltl_claims, MFLAGS)) == NULL) {
        loger::fatal("cannot open tmp file %s", ltl_claims);
      }
      tl_out = fd_ltl;
    }
    launch_settings.add_ltl = (char **)emalloc(5 * sizeof(char *));
    launch_settings.add_ltl[1] = "-c";
    launch_settings.add_ltl[2] = strdup(nm.c_str());
    launch_settings.add_ltl[3] = "-f";
    std::string fm_modified = "!(" + fm + ")";
    launch_settings.add_ltl[4] = strdup(fm_modified.c_str());
    nr_errs += tl_main(4, launch_settings.add_ltl);
    fflush(tl_out);
  }
}