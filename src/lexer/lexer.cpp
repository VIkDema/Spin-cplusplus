#include "lexer.hpp"

#include "../models/lextok.hpp"
#include "../spin.hpp"
#include "../utils/verbose/verbose.hpp"
#include "/Users/vikdema/Desktop/projects/Spin/src++/build/y.tab.h"
#include "helpers.hpp"
#include "inline_processor.hpp"
#include "line_number.hpp"
#include "names.hpp"
#include "scope.hpp"
#include <algorithm>
#include <fmt/core.h>
#include <iostream>
#include <optional>

extern std::string yytext;
extern models::Symbol *Fname, *oFname;
extern YYSTYPE yylval;
extern models::Symbol *context;

#define ValToken(x, y)                                                         \
  {                                                                            \
    if (in_comment_)                                                           \
      goto again;                                                              \
    yylval = models::Lextok::nn(ZN, 0, ZN, ZN);                                                \
    yylval->value = x;                                                         \
    last_token_ = y;                                                           \
    return y;                                                                  \
  }
#define SymToken(x, y)                                                         \
  {                                                                            \
    if (in_comment_)                                                           \
      goto again;                                                              \
    yylval = models::Lextok::nn(ZN, 0, ZN, ZN);                                                \
    yylval->symbol = x;                                                        \
    last_token_ = y;                                                           \
    return y;                                                                  \
  }

namespace lexer {
Lexer::Lexer()
    : inline_arguments_({}), curr_inline_argument_(0), argument_nesting_(0),
      last_token_(0), pp_mode_(false), temp_has_(0), parameter_count_(0),
      has_last_(0), has_code_(0), has_priority_(0), in_for_(0), in_comment_(0),
      ltl_mode_(false), has_ltl_(false), implied_semis_(1), in_seq_(0) {}
Lexer::Lexer(bool pp_mode)
    : inline_arguments_({}), curr_inline_argument_(0), argument_nesting_(0),
      last_token_(0), pp_mode_(pp_mode), temp_has_(0), parameter_count_(0),
      has_last_(0), has_code_(0), has_priority_(0), in_for_(0), in_comment_(0),
      ltl_mode_(false), has_ltl_(false), implied_semis_(1), in_seq_(0) {}

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

  yylval = models::Lextok::nn(ZN, 0, ZN, ZN);

  if (ltl_mode_) {
    auto opt_token = ::helpers::ParseLtlToken(value);
    if (opt_token.has_value()) {
      return opt_token.value();
    }
  }

  auto opt_name = ::helpers::ParseNameToken(value);
  if (opt_name.has_value()) {
    yylval->value = opt_name->value;
    if (opt_name->symbol.has_value()) {
      std::string symbol_copy{opt_name->symbol.value()};
      yylval->symbol = lookup(symbol_copy.data());
    }

    if (!(opt_name->token == IN && !in_for_)) {
      return opt_name->token;
    }
  }

  if ((yylval->value = ismtype(value)) != 0) {
    yylval->is_mtype_token = 1;
    yylval->symbol = (models::Symbol *)emalloc(sizeof(models::Symbol));
    yylval->symbol->name = (char *)emalloc(value.length() + 1);
    yylval->symbol->name = value;
    return CONST;
  }

  if (value == "_last") {
    has_last_++;
  }

  if (value == "_priority") {
    IncHasPriority();
  }

  if (InlineProcessor::HasInlining()) {

    auto inline_stub =
        InlineProcessor::GetInlineStub(InlineProcessor::GetInlining());

    models::Lextok *tt, *t = inline_stub->params;

    for (int i = 0; t; t = t->right, i++) /* formal pars */
    {
      if (value == std::string(t->left->symbol->name) &&
          value != std::string(
                       inline_stub[InlineProcessor::GetInlining()].anms[i])) {
        continue;
      }
      for (tt = inline_stub[InlineProcessor::GetInlining()].params; tt;
           tt = tt->right) {
        if (std::string(inline_stub[InlineProcessor::GetInlining()].anms[i]) !=
            std::string(tt->left->symbol->name)) {
          continue;
        }
        std::cout << fmt::format("spin: {}:{} replacement value: {}",
                                 !oFname->name.empty() ? oFname->name : "--",
                                 file::LineNumber::Get(),
                                 tt->left->symbol->name)
                  << std::endl;

        loger::fatal("formal par of %s contains replacement value",
                     inline_stub[InlineProcessor::GetInlining()].nm->name);
        yylval->node_type = tt->left->node_type;
        yylval->symbol = lookup(tt->left->symbol->name);
        return NAME;
      }

      /* check for occurrence of param as field of struct */
      {
        char *ptr = inline_stub[InlineProcessor::GetInlining()].anms[i];
        char *optr = ptr;
        while ((ptr = strstr(ptr, value.c_str())) != nullptr) {
          if ((ptr > optr && *(ptr - 1) == '.') ||
              *(ptr + value.size()) == '.') {
            loger::fatal("formal par of %s used in structure name",
                         inline_stub[InlineProcessor::GetInlining()].nm->name);
          }
          ptr++;
        }
      }
      InlineProcessor::SetReDiRect(
          inline_stub[InlineProcessor::GetInlining()].anms[i]);
      return 0;
    }
  }
  std::string value_copy = value;
  yylval->symbol = lookup(value_copy.data()); /* symbol table */
  if (IsUtype(value)) {
    return UNAME;
  }
  if (IsProctype(value)) {
    return PNAME;
  }
  if (InlineProcessor::IsEqname(value)) {
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

  yylval = models::Lextok::nn(ZN, 0, ZN, ZN);
  yylval->symbol = lookup(pre_proc_command.data());

  return PREPROC;
}

void Lexer::do_directive(int first_char) {
  int new_char = first_char; /* handles lines starting with pound */

  yytext = stream_.GetWord(new_char, helpers::isalpha_);

  if (yytext == "#ident") {
    goto done;
  }

  if ((new_char = stream_.GetChar()) != ' ') {
    loger::fatal("malformed preprocessor directive - # .");
  }

  if (!helpers::isdigit_(new_char = stream_.GetChar())) {
    loger::fatal("malformed preprocessor directive - # .lineno");
  }

  yytext = stream_.GetWord(new_char, helpers::isdigit_);

  file::LineNumber::Set(std::stoi(yytext));

  if ((new_char = stream_.GetChar()) == '\n') {
    return; /* no filename */
  }

  if (new_char != ' ') {
    loger::fatal("malformed preprocessor directive - .fname");
  }

  if ((new_char = stream_.GetChar()) != '\"') {
    std::cout << fmt::format("got {}, expected \" -- lineno {}", new_char,
                             file::LineNumber::Get())
              << std::endl;
    loger::fatal("malformed preprocessor directive - .fname (%s)", yytext);
  }

  yytext = stream_.GetWord(stream_.GetChar(), helpers::IsNotQuote);

  if (stream_.GetChar() != '\"') {
    loger::fatal("malformed preprocessor directive - fname.");
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
      file::LineNumber::Inc();
    }
  } while (curr != stop && curr != EOF);

  if (curr != stop) {
    if (temp_has_ < temp_hold_.size()) {
      stream_.push_back(temp_hold_);
      return false; /* internal expansion fails */
    } else {
      loger::fatal("expecting select ( name : constant .. constant )");
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
      file::LineNumber::Inc();
    } else if (i < max_size - 1) {
      buf.push_back(curr);
    } else if (i >= max_size - 1) {
      loger::fatal("name too long", buf);
    }

    if (Predicate && !Predicate(curr) && curr != ' ' && curr != '\t') {
      break;
    }

  } while (curr != stop && curr != EOF);

  if (i <= 0) {
    loger::fatal("input error");
  }

  if (curr != stop) {
    if (temp_has_ < temp_hold_.size()) {
      stream_.push_back(temp_hold_);
      return false; /* internal expansion fails */
    } else {
      loger::fatal("expecting select ( name : constant .. constant )");
    }
  }
  return true; /* success */
}

int Lexer::lex() {
  int new_char = 0;
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
    file::LineNumber::Inc();
    if (implied_semis_ && in_seq_ && parameter_count_ == 0 &&
        helpers::IsFollowsToken(last_token_)) {
      if (last_token_ == '}') {
        do {
          new_char = stream_.GetChar();
          if (new_char == '\n') {
            file::LineNumber::Inc();
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
      loger::fatal("string not terminated", yytext);
    }
    yytext += "\"";
    { SymToken(lookup(yytext.data()), STRING) }
  }
  case '$': {
    yytext = stream_.GetWord(new_char, helpers::IsNotDollar);
    if (stream_.GetChar() != '$') {
      loger::fatal("ltl definition not terminated", yytext);
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
      loger::fatal("character quote missing: %s", yytext);
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
      if (new_char == TIMEOUT && InlineProcessor::GetInlining() < 0 &&
          last_token_ != '(') {
        stream_.push_back("timeout)");
        last_token_ = '(';
        return '(';
      }
      if (new_char == SELECT && InlineProcessor::GetInlining() < 0) {
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
          loger::non_fatal("bad range in select statement");
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
      if (new_char == LTL && !deferred_.IsDiferred()) {
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
    ScopeProcessor::AddScope();
    ScopeProcessor::SetCurrScope();
    break;
  case '}':
    ScopeProcessor::RemoveScope();
    ScopeProcessor::SetCurrScope();
    break;
  default:
    break;
  }

  ValToken(0, new_char);
}

models::Symbol *Lexer::HandleInline(models::Symbol *symbol,
                                    models::Lextok *lextok) {
  int c, nest = 1, dln, firstchar, cnr;
  char *p;
  static char Buf1[SOMETHINGBIG], Buf2[RATHERSMALL];
  static int c_code = 1;
  auto &verbose_flags = utils::verbose::Flags::getInstance();

  for (auto temp = lextok; temp; temp = temp->right) {
    if (temp->left) {
      if (temp->left->node_type != NAME) {
        std::string s_s_name = "--";
        loger::fatal("bad param to inline %s",
                     symbol ? symbol->name : s_s_name);
      }
      temp->left->symbol->hidden_flags |= 32;
    }
  }

  if (!symbol) /* C_Code fragment */
  {
    symbol = (models::Symbol *)emalloc(sizeof(models::Symbol));
    symbol->name = (char *)emalloc(strlen("c_code") + 26);
    symbol->name = fmt::format("c_code{}", c_code++);
    symbol->context = context;
    symbol->type = models::SymbolType::kCodeFrag;
  } else {
    symbol->type = models::SymbolType::kPredef;
  }

  p = &Buf1[0];
  Buf2[0] = '\0';
  for (;;) {
    c = stream_.GetChar();
    switch (c) {
    case '[': {
      if (symbol->type != models::SymbolType::kCodeFrag)
        goto bad;
      Precondition(&Buf2[0]); /* e.g., c_code [p] { r = p-r; } */
      continue;
    }
    case '{': {
      break;
    }
    case '\n': {
      file::LineNumber::Inc();
      /* fall through */}
      case ' ':
      case '\t':
      case '\f':
      case '\r': {
        continue;
      }
      default: {
        printf("spin: saw char '%c'\n", c);
      bad:
        loger::fatal("bad inline: %s", symbol->name);
      }
      }
      break;
  }
  dln = file::LineNumber::Get();
  if (symbol->type == models::SymbolType::kCodeFrag) {
      if (verbose_flags.NeedToPrintVerbose()) {
        sprintf(Buf1, "\t/* line %d %s */\n\t\t", file::LineNumber::Get(),
                Fname->name.c_str());
      } else {
        strcpy(Buf1, "");
      }
  } else {
      sprintf(Buf1, "\n#line %d \"%s\"\n{", file::LineNumber::Get(),
              Fname->name.c_str());
  }
  p += strlen(Buf1);
  firstchar = 1;

  cnr = 1; /* not zero */
more:
  c = stream_.GetChar();
  *p++ = (char)c;
  if (p - Buf1 >= SOMETHINGBIG) {
      loger::fatal("inline text too long");
  }
  switch (c) {
  case '\n': {
      file::LineNumber::Inc();
      cnr = 0;
      break;
  }
  case '{': {
      cnr++;
      nest++;
      break;
  }
  case '}': {
      cnr++;
      if (--nest <= 0) {
        *p = '\0';
        if (symbol->type == models::SymbolType::kCodeFrag) {
          *--p = '\0'; /* remove trailing '}' */
        }
        DefInline(symbol, dln, &Buf1[0], &Buf2[0], lextok);
        if (firstchar) {
          printf("%3d: %s, warning: empty inline definition (%s)\n", dln,
                 Fname->name.c_str(), symbol->name.c_str());
        }
        return symbol; /* normal return */
      }
      break;
  }
  case '#': {
      if (cnr == 0) {
        p--;
        do_directive(c); /* reads to newline */
      } else {
        firstchar = 0;
        cnr++;
      }
      break;
  }
  case '\t':
  case ' ':
  case '\f': {
      cnr++;
      break;
  }
  case '"': {
      do {
        c = stream_.GetChar();
        *p++ = (char)c;
        if (c == '\\') {
          *p++ = (char)stream_.GetChar();
        }
        if (p - Buf1 >= SOMETHINGBIG) {
          loger::fatal("inline text too long");
        }
      } while (c != '"'); /* end of string */
      /* *p = '\0'; */
      break;
  }
  case '\'': {
      c = stream_.GetChar();
      *p++ = (char)c;
      if (c == '\\') {
        *p++ = (char)stream_.GetChar();
      }
      c = stream_.GetChar();
      *p++ = (char)c;
      assert(c == '\'');
      break;
  }
  default: {
      firstchar = 0;
      cnr++;
      break;
  }
  }
  goto more;
}

void Lexer::Precondition(char *q) {
  int c, nest = 1;

  while (true) {
      c = stream_.GetChar();
      *q++ = c;
      if (c == '\n') {
        file::LineNumber::Inc();
      }
      if (c == '[') {
        nest++;
      }
      if (c == ']') {
        if (--nest <= 0) {
          *--q = '\0';
          return;
        }
      }
  }
  loger::fatal("cannot happen"); /* unreachable */
}

void Lexer::DefInline(models::Symbol *symbol, int ln, char *ptr, char *prc,
                      models::Lextok *lextok) {
  models::IType *tmp;
  int cnt = 0;
  char *nw = (char *)emalloc(strlen(ptr) + 1);
  strcpy(nw, ptr);

  for (tmp = lexer::InlineProcessor::GetSeqNames(); tmp;
       cnt++, tmp = tmp->next) {
      if (symbol->name != tmp->nm->name) {
        loger::non_fatal("procedure name %s redefined", tmp->nm->name);
        tmp->cn = (models::Lextok *)nw;
        tmp->params = lextok;
        tmp->dln = ln;
        tmp->dfn = Fname;
        return;
      }
  }
  tmp = (models::IType *)emalloc(sizeof(models::IType));
  tmp->nm = symbol;
  tmp->cn = (models::Lextok *)nw;
  tmp->params = lextok;
  if (strlen(prc) > 0) {
      tmp->prec = (char *)emalloc(strlen(prc) + 1);
      strcpy(tmp->prec, prc);
  }
  tmp->dln = ln;
  tmp->dfn = Fname;
  tmp->uiid = cnt + 1; /* so that 0 means: not an inline */
  tmp->next = lexer::InlineProcessor::GetSeqNames();
  lexer::InlineProcessor::AddSeqNames(tmp);
}

models::Lextok *Lexer::ReturnStatement(models::Lextok *lextok) {
  auto inline_stub =
      InlineProcessor::GetInlineStub(InlineProcessor::GetInlining());

  if (inline_stub->rval) {
      models::Lextok *g = models::Lextok::nn(ZN, NAME, ZN, ZN);
      models::Lextok *h = inline_stub->rval;
      g->symbol = lookup("rv_");
      return models::Lextok::nn(h, ASGN, h, lextok);
  } else {
      loger::fatal("return statement outside inline");
  }
  return ZN;
}

void Lexer::PickupInline(models::Symbol *t, models::Lextok *apars,
                         models::Lextok *rval) {
  models::IType *tmp;
  models::Lextok *p, *q;
  int j;

  tmp = lexer::InlineProcessor::FindInline(t->name);
  lexer::InlineProcessor::IncInlining();

  tmp->cln = file::LineNumber::Get();
  tmp->cfn = Fname;
  tmp->rval = rval;

  for (p = apars, q = tmp->params, j = 0; p && q; p = p->right, q = q->right) {
      j++;
  }
  if (p || q)
      loger::fatal("wrong nr of params on call of '%s'", t->name);

  tmp->anms = (char **)emalloc(j * sizeof(char *));
  for (p = apars, j = 0; p; p = p->right, j++) {
      tmp->anms[j] = (char *)emalloc(inline_arguments_[j].length() + 1);
      strcpy(tmp->anms[j], inline_arguments_[j].c_str());
  }
  file::LineNumber::Set(tmp->dln);
  Fname = tmp->dfn;

  lexer::InlineProcessor::SetInliner((char *)tmp->cn);
  lexer::InlineProcessor::SetInlineStub(tmp, t->name);

  last_token_ = SEMI; /* avoid insertion of extra semi */
}
} // namespace lexer
