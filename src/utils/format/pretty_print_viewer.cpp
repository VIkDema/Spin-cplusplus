#include "pretty_print_viewer.hpp"

#include "../../spin.hpp"
#include "y.tab.h"
#include <cstdio>
#include <fmt/core.h>
#include <iostream>
#include <optional>
#include <sstream>

/* The semantic value of the look-ahead symbol.  */
extern YYSTYPE yylval;

namespace format {

void PrettyPrintViewer::doindent() {
  for (int i = 0; i < indent; i++) {
    std::cout << "   ";
  }
}
void PrettyPrintViewer::decrease_indentation() { indent--; }
void PrettyPrintViewer::increase_indentation() { indent++; }

bool PrettyPrintViewer::should_start_new_line(int current_token, int last_token) {
  return (current_token == C_DECL || current_token == C_STATE ||
          current_token == C_TRACK || current_token == SEP ||
          current_token == DO || current_token == IF ||
          (current_token == TYPE && !in_decl)) ||
         (current_token == '{' && last_token != OF && last_token != IN &&
          last_token != ATOMIC && last_token != D_STEP &&
          last_token != C_CODE && last_token != C_DECL && last_token != C_EXPR);
}

void PrettyPrintViewer::start_new_line(std::string &buf) {
  if (buf.size() == 0)
    return;

  if (buf[buf.size() - 1] != ':') /* label? */
  {
    if (buf[0] == ':' && buf[1] == ':') {
      indent--;
      doindent();
      indent++;
    } else {
      doindent();
    }
  }

  std::cout << buf << std::endl;
  buf = "";

  in_decl = 0;
  in_c_code = 0;
  in_c_decl = 0;
}

void PrettyPrintViewer::append_space_if_needed(int current_token, int last_token,
                                         std::string &buffer) {
  if (current_token != ':' && current_token != SEMI && current_token != ',' &&
      current_token != '(' && current_token != '#' && last_token != '#' &&
      current_token != ARROW && last_token != ARROW && current_token != '.' &&
      last_token != '.' && current_token != '!' && last_token != '!' &&
      current_token != SND && last_token != SND && current_token != RCV &&
      last_token != RCV && current_token != O_SND && last_token != O_SND &&
      current_token != R_RCV && last_token != R_RCV &&
      (current_token != ']' || last_token != '[') &&
      (current_token != '>' || last_token != '<') &&
      (current_token != GT || last_token != LT) && current_token != '@' &&
      last_token != '@' && (last_token != '(' || current_token != ')') &&
      (last_token != '/' || current_token != '/') && current_token != DO &&
      current_token != OD && current_token != IF && current_token != FI &&
      current_token != SEP && buffer.size() > 0) {
    buffer += " ";
  }
}

void PrettyPrintViewer::view() {
  std::string buffer = "";
  int current_token, last_token = 0;

  while ((current_token = lex(true)) != EOF) {
    if ((last_token == IF || last_token == DO) && current_token != SEP) {
      decrease_indentation();
    }

    if (should_start_new_line(current_token, last_token)) {
      start_new_line(buffer); /* start on new line */
    }

    if (current_token == PREPROC) {
      int temp_indent = indent;
      start_new_line(buffer);

      assert(strlen(yylval->sym->name) < sizeof(buffer));
      buffer = yylval->sym->name;

      indent = 0;
      start_new_line(buffer);

      indent = temp_indent;
      continue;
    }
    append_space_if_needed(current_token, last_token, buffer);

    if (current_token == '}' || current_token == OD || current_token == FI) {
      start_new_line(buffer);
      decrease_indentation();
    }

    map_token_to_string(current_token, buffer);

    if (current_token == '}' || current_token == BREAK ||
        current_token == SEMI || current_token == ELSE ||
        (current_token == ':' && last_token == NAME) || current_token == '{' ||
        current_token == DO || current_token == IF) {
      start_new_line(buffer);
    }
    if (current_token == '{' || current_token == DO || current_token == IF) {
      increase_indentation();
    }

    last_token = current_token;
  }
  start_new_line(buffer);
}

void PrettyPrintViewer::map_token_to_string(int n, std::string &buf) {
  std::stringstream mtxt;

  switch (n) {
  default:
    if (n > 0 && n < 256) {
      mtxt << (char)n;
    } else {
      mtxt << fmt::format("<{}?>", n);
    }
    break;
  case '(':
    mtxt << "(";
    in_decl++;
    break;
  case ')':
    mtxt << ")";
    in_decl--;
    break;
  case '{':
    mtxt << "{";
    break;
  case '}':
    mtxt << "}";
    break;
  case '\t':
    mtxt << "\\t";
    break;
  case '\f':
    mtxt << "\\f";
    break;
  case '\n':
    mtxt << "\\n";
    break;
  case '\r':
    mtxt << "\\r";
    break;
  case 'c':
    mtxt << "condition";
    break;
  case 's':
    mtxt << "send";
    break;
  case 'r':
    mtxt << "recv";
    break;
  case 'R':
    mtxt << "recv poll";
    break;
  case '@':
    mtxt << "@";
    break;
  case '?':
    mtxt << "(x->y:z)";
    break;
  case NEXT:
    mtxt << "X";
    break;
  case ALWAYS:
    mtxt << "[]";
    break;
  case EVENTUALLY:
    mtxt << "<>";
    break;
  case IMPLIES:
    mtxt << "->";
    break;
  case EQUIV:
    mtxt << "<->";
    break;
  case UNTIL:
    mtxt << "U";
    break;
  case WEAK_UNTIL:
    mtxt << "W";
    break;
  case IN:
    mtxt << "in";
    break;
  case ACTIVE:
    mtxt << "active";
    break;
  case AND:
    mtxt << "&&";
    break;
  case ARROW:
    mtxt << "->";
    break;
  case ASGN:
    mtxt << "=";
    break;
  case ASSERT:
    mtxt << "assert";
    break;
  case ATOMIC:
    mtxt << "atomic";
    break;
  case BREAK:
    mtxt << "break";
    break;
  case C_CODE:
    mtxt << "c_code";
    in_c_code++;
    break;
  case C_DECL:
    mtxt << "c_decl";
    in_c_decl++;
    break;
  case C_EXPR:
    mtxt << "c_expr";
    break;
  case C_STATE:
    mtxt << "c_state";
    break;
  case C_TRACK:
    mtxt << "c_track";
    break;
  case CLAIM:
    mtxt << "never";
    break;
  case CONST:
    mtxt << yylval->val;
    break;
  case DECR:
    mtxt << "--";
    break;
  case D_STEP:
    mtxt << "d_step";
    break;
  case D_PROCTYPE:
    mtxt << "d_proctype";
    break;
  case DO:
    mtxt << "do";
    break;
  case DOT:
    mtxt << ".";
    break;
  case ELSE:
    mtxt << "else";
    break;
  case EMPTY:
    mtxt << "empty";
    break;
  case ENABLED:
    mtxt << "enabled";
    break;
  case EQ:
    mtxt << "==";
    break;
  case EVAL:
    mtxt << "eval";
    break;
  case FI:
    mtxt << "fi";
    break;
  case FOR:
    mtxt << "for";
    break;
  case FULL:
    mtxt << "full";
    break;
  case GE:
    mtxt << ">=";
    break;
  case GET_P:
    mtxt << "get_priority";
    break;
  case GOTO:
    mtxt << "goto";
    break;
  case GT:
    mtxt << ">";
    break;
  case HIDDEN:
    mtxt << "hidden";
    break;
  case IF:
    mtxt << "if";
    break;
  case INCR:
    mtxt << "++";
    break;
  case INLINE:
    mtxt << "inline";
    break;
  case INIT:
    mtxt << "init";
    break;
  case ISLOCAL:
    mtxt << "local";
    break;
  case LABEL:
    mtxt << "<label-name>";
    break;
  case LE:
    mtxt << "<=";
    break;
  case LEN:
    mtxt << "len";
    break;
  case LSHIFT:
    mtxt << "<<";
    break;
  case LT:
    mtxt << "<";
    break;
  case LTL:
    mtxt << "ltl";
    break;
  case NAME:
    mtxt << yylval->sym->name;
    break;
  case XU:
    switch (yylval->val) {
    case XR:
      mtxt << "xr";
      break;
    case XS:
      mtxt << "xs";
      break;
    default:
      mtxt << "<?>";
      break;
    }
    break;

  case TYPE:
    switch (yylval->val) {
    case BIT:
      mtxt << "bit";
      break;
    case BYTE:
      mtxt << "byte";
      break;
    case CHAN:
      mtxt << "chan";
      in_decl++;
      break;
    case INT:
      mtxt << "int";
      break;
    case MTYPE:
      mtxt << "mtype";
      break;
    case SHORT:
      mtxt << "short";
      break;
    case UNSIGNED:
      mtxt << "unsigned";
      break;
    default:
      mtxt << "<unknown type>";
      break;
    }
    break;

  case NE:
    mtxt << "!=";
    break;
  case NEG:
    mtxt << "!";
    break;
  case NEMPTY:
    mtxt << "nempty";
    break;
  case NFULL:
    mtxt << "nfull";
    break;
  case NON_ATOMIC:
    mtxt << "<sub-sequence>";
    break;
  case NONPROGRESS:
    mtxt << "np_";
    break;
  case OD:
    mtxt << "od";
    break;
  case OF:
    mtxt << "of";
    break;
  case OR:
    mtxt << "||";
    break;
  case O_SND:
    mtxt << "!!";
    break;
  case PC_VAL:
    mtxt << "pc_value";
    break;
  case PRINT:
    mtxt << "printf";
    break;
  case PRINTM:
    mtxt << "printm";
    break;
  case PRIORITY:
    mtxt << "priority";
    break;
  case PROCTYPE:
    mtxt << "proctype";
    break;
  case PROVIDED:
    mtxt << "provided";
    break;
  case RETURN:
    mtxt << "return";
    break;
  case RCV:
    mtxt << "?";
    break;
  case R_RCV:
    mtxt << "??";
    break;
  case RSHIFT:
    mtxt << ">>";
    break;
  case RUN:
    mtxt << "run";
    break;
  case SEP:
    mtxt << "::";
    break;
  case SEMI:
    mtxt << ";";
    break;
  case SET_P:
    mtxt << "set_priority";
    break;
  case SHOW:
    mtxt << "show";
    break;
  case SND:
    mtxt << "!";
    break;
  case INAME:
  case UNAME:
  case PNAME:
  case STRING:
    mtxt << yylval->sym->name;
    break;
  case TRACE:
    mtxt << "trace";
    break;
  case TIMEOUT:
    mtxt << "(timeout)";
    break;
  case TYPEDEF:
    mtxt << "typedef";
    break;
  case UMIN:
    mtxt << "-";
    break;
  case UNLESS:
    mtxt << "unless";
    break;
  }
  buf += mtxt.str();
}

} // namespace format