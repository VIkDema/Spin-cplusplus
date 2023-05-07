
#include "fatal.hpp"
#include "../spin.hpp"
#include "../utils/verbose/verbose.hpp"
#include "y.tab.h"
#include <filesystem>
#include <fmt/core.h>
#include <iostream>
#include <sstream>

extern models::Symbol *Fname;
extern models::Symbol *oFname;
//TODO: fix it
extern int nr_errs;
extern int lineno;
extern int yychar;
extern char yytext[];

namespace loger {
static constexpr std::string_view kOperator = "operator: ";
static constexpr std::string_view kKeyword = "keyword: ";
static constexpr std::string_view kFunction = "function-name: ";

void non_fatal(const std::string_view &s1,
               const std::optional<std::string> &s2) {
  std::string fname =
      Fname ? Fname->name : (oFname ? oFname->name : "nofilename");
  std::string separator =
      yychar == SEMI ? " statement separator"
                     : fmt::format(" saw '{}'", explainToString(yychar));
  std::string near =
      strlen(yytext) > 1 ? fmt::format(" near '{}'", yytext) : "";

  std::cout << fmt::format("spin: {0}:{1}, Error: {2}{3} {4}", fname, lineno,
                           s1, s2.value_or(""), separator, near);
  std::cout << std::endl;
  nr_errs++;
}

void fatal(const std::string_view &s1, const std::optional<std::string> &s2) {
  auto &verbose_flags = utils::verbose::Flags::getInstance();
  non_fatal(s1, s2);
  for (const auto &file :
       {"pan.b", "pan.c", "pan.h", "pan.m", "pan.t", "pan.p", "pan.pre"}) {
    std::filesystem::remove(file);
  }
  if (!verbose_flags.NeedToPrintVerbose()) {
    std::filesystem::remove("_spin_nvr.tmp");
  }
  alldone(1);
}

std::string explainToString(int n) {
  std::stringstream ss;
  switch (n) {
  default:
    if (n > 0 && n < 256)
      ss << "'" << static_cast<char>(n) << "' = ";
    ss << n;
    break;
  case '\b':
    ss << "\\b";
    break;
  case '\t':
    ss << "\\t";
    break;
  case '\f':
    ss << "\\f";
    break;
  case '\n':
    ss << "\\n";
    break;
  case '\r':
    ss << "\\r";
    break;
  case 'c':
    ss << "condition";
    break;
  case 's':
    ss << "send";
    break;
  case 'r':
    ss << "recv";
    break;
  case 'R':
    ss << "recv poll " << kOperator;
    break;
  case '@':
    ss << "@";
    break;
  case '?':
    ss << "(x->y:z)";
    break;
  case NEXT:
    ss << "X";
    break;
  case ALWAYS:
    ss << "[]";
    break;
  case EVENTUALLY:
    ss << "<>";
    break;
  case IMPLIES:
    ss << "->";
    break;
  case EQUIV:
    ss << "<->";
    break;
  case UNTIL:
    ss << "U";
    break;
  case WEAK_UNTIL:
    ss << "W";
    break;
  case IN:
    ss << kKeyword << "in";
    break;
  case ACTIVE:
    ss << kKeyword << "active";
    break;
  case AND:
    ss << kOperator << "&&";
    break;
  case ASGN:
    ss << kOperator << "=";
    break;
  case ASSERT:
    ss << kFunction << "assert";
    break;
  case ATOMIC:
    ss << kKeyword << "atomic";
    break;
  case BREAK:
    ss << kKeyword << "break";
    break;
  case C_CODE:
    ss << kKeyword << "c_code";
    break;
  case C_DECL:
    ss << kKeyword << "c_decl";
    break;
  case C_EXPR:
    ss << kKeyword << "c_expr";
    break;
  case C_STATE:
    ss << kKeyword << "c_state";
    break;
  case C_TRACK:
    ss << kKeyword << "c_track";
    break;
  case CLAIM:
    ss << kKeyword << "never";
    break;
  case CONST:
    ss << "a constant";
    break;
  case DECR:
    ss << kOperator << "--";
    break;
  case D_STEP:
    ss << kKeyword << "d_step";
    break;
  case D_PROCTYPE:
    ss << kKeyword << "d_proctype";
    break;
  case DO:
    ss << kKeyword << "do";
    break;
  case DOT:
    ss << ".";
    break;
  case ELSE:
    ss << kKeyword << "else";
    break;
  case EMPTY:
    ss << kFunction << "empty";
    break;
  case ENABLED:
    ss << kFunction << "enabled";
    break;
  case EQ:
    ss << kOperator << "==";
    break;
  case EVAL:
    ss << kFunction << "eval";
    break;
  case FI:
    ss << kKeyword << "fi";
    break;
  case FULL:
    ss << kFunction << "full";
    break;
  case GE:
    ss << kOperator << ">=";
    break;
  case GET_P:
    ss << kFunction << "get_priority";
    break;
  case GOTO:
    ss << kKeyword << "goto";
    break;
  case GT:
    ss << kOperator << ">";
    break;
  case HIDDEN:
    ss << kKeyword << "hidden_flags";
    break;
  case IF:
    ss << kKeyword << "if";
    break;
  case INCR:
    ss << kOperator << "++";
    break;
  case INAME:
    ss << "inline name";
    break;
  case INLINE:
    ss << kKeyword << "inline";
    break;
  case INIT:
    ss << kKeyword << "init";
    break;
  case ISLOCAL:
    ss << kKeyword << "local";
    break;
  case LABEL:
    ss << "a label-name";
    break;
  case LE:
    ss << kOperator << "<=";
    break;
  case LEN:
    ss << kFunction << "len";
    break;
  case LSHIFT:
    ss << kOperator << "<<";
    break;
  case LT:
    ss << kOperator << "<";
    break;
  case MTYPE:
    ss << kKeyword << "mtype";
    break;
  case NAME:
    ss << "an identifier";
    break;
  case NE:
    ss << kOperator << "!=";
    break;
  case NEG:
    ss << kOperator << "! (not)";
    break;
  case NEMPTY:
    ss << kFunction << "nempty";
    break;
  case NFULL:
    ss << kFunction << "nfull";
    break;
  case NON_ATOMIC:
    ss << "sub-sequence";
    break;
  case NONPROGRESS:
    ss << kFunction << "np_";
    break;
  case OD:
    ss << kKeyword << "od";
    break;
  case OF:
    ss << kKeyword << "of";
    break;
  case OR:
    ss << kOperator << "||";
    break;
  case O_SND:
    ss << kOperator << "!!";
    break;
  case PC_VAL:
    ss << kFunction << "pc_value";
    break;
  case PNAME:
    ss << "process name";
    break;
  case PRINT:
    ss << kFunction << "printf";
    break;
  case PRINTM:
    ss << kFunction << "printm";
    break;
  case PRIORITY:
    ss << kKeyword << "priority";
    break;
  case PROCTYPE:
    ss << kKeyword << "proctype";
    break;
  case PROVIDED:
    ss << kKeyword << "provided";
    break;
  case RCV:
    ss << kOperator << "?";
    break;
  case R_RCV:
    ss << kOperator << "??";
    break;
  case RSHIFT:
    ss << kOperator << ">>";
    break;
  case RUN:
    ss << kOperator << "run";
    break;
  case SEP:
    ss << "token: ::";
    break;
  case SEMI:
    ss << ";";
    break;
  case ARROW:
    ss << "->";
    break;
  case SET_P:
    ss << kFunction << "set_priority";
    break;
  case SHOW:
    ss << kKeyword << "show";
    break;
  case SND:
    ss << kOperator << "!";
    break;
  case STRING:
    ss << kKeyword << "a string";
    break;
  case TRACE:
    ss << kKeyword << "trace";
    break;
  case TIMEOUT:
    ss << kKeyword << "timeout";
    break;
  case TYPE:
    ss << "data typename";
    break;
  case TYPEDEF:
    ss << kKeyword << "typedef";
    break;
  case XU:
    ss << kKeyword << "x[rs]";
    break;
  case UMIN:
    ss << kOperator << "- (unary minus)";
    break;
  case UNAME:
    ss << "a typename";
    break;
  case UNLESS:
    ss << kKeyword << "unless";
    break;
  }
  return ss.str();
}
} // namespace loger