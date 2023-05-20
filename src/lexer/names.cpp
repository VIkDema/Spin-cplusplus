#include "names.hpp"

#include "y.tab.h"
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>

namespace helpers {

#define UNSIGNED 5 /* val defines width in bits */
#define BIT 1      /* also equal to width in bits */
#define BYTE 8     /* ditto */
#define SHORT 16   /* ditto */
#define INT 32     /* ditto */
#define CHAN 64    /* not */
#define STRUCT 128 /* user defined structure name */
#define XR 1       /* non-shared receive-only */
#define XS 2       /* non-shared send-only    */
#define XX 4       /* overrides XR or XS tag  */

const std::unordered_map<std::string_view, int> kLtlSyms{
    {"U", UNTIL},          {"V", RELEASE},         {"W", WEAK_UNTIL},
    {"X", NEXT},           {"always", ALWAYS},     {"eventually", EVENTUALLY},
    {"until", UNTIL},      {"stronguntil", UNTIL}, {"weakuntil", WEAK_UNTIL},
    {"release", RELEASE},  {"next", NEXT},         {"implies", IMPLIES},
    {"equivalent", EQUIV},
};

const std::unordered_map<std::string_view, Name> kNames{
    {"active", {ACTIVE}},
    {"assert", {ASSERT}},
    {"atomic", {ATOMIC}},
    {"bit", {TYPE, BIT}},
    {"bool", {TYPE, BIT}},
    {"break", {BREAK}},
    {"byte", {TYPE, BYTE}},
    {"c_code", {C_CODE}},
    {"c_decl", {C_DECL}},
    {"c_expr", {C_EXPR}},
    {"c_state", {C_STATE}},
    {"c_track", {C_TRACK}},
    {"D_proctype", {D_PROCTYPE}},
    {"do", {DO}},
    {"chan", {TYPE, CHAN}},
    {"else", {ELSE}},
    {"empty", {EMPTY}},
    {"enabled", {ENABLED}},
    {"eval", {EVAL}},
    {"false", {CONST}},
    {"fi", {FI}},
    {"for", {FOR}},
    {"full", {FULL}},
    {"get_priority", {GET_P}},
    {"goto", {GOTO}},
    {"hidden_flags", {HIDDEN, 0, ":hide:"}},
    {"if", {IF}},
    {"in", {IN}},
    {"init", {INIT, 0, ":init:"}},
    {"inline", {INLINE}},
    {"int", {TYPE, INT}},
    {"len", {LEN}},
    {"local", {ISLOCAL, 0, ":local:"}},
    {"ltl", {LTL, 0, ":ltl:"}},
    {"mtype", {TYPE, MTYPE}},
    {"nempty", {NEMPTY}},
    {"never", {CLAIM, 0, ":never:"}},
    {"nfull", {NFULL}},
    {"notrace", {TRACE, 0, ":notrace:"}},
    {"np_", {NONPROGRESS}},
    {"od", {OD}},
    {"of", {OF}},
    {"pc_value", {PC_VAL}},
    {"pid", {TYPE, BYTE}},
    {"printf", {PRINT}},
    {"printm", {PRINTM}},
    {"priority", {PRIORITY}},
    {"proctype", {PROCTYPE}},
    {"provided", {PROVIDED}},
    {"return", {RETURN}},
    {"run", {RUN}},
    {"d_step", {D_STEP}},
    {"select", {SELECT}},
    {"set_priority", {SET_P}},
    {"short", {TYPE, SHORT}},
    {"skip", {CONST, 1}},
    {"timeout", {TIMEOUT}},
    {"trace", {TRACE, 0, ":trace:"}},
    {"true", {CONST, 1}},
    {"show", {SHOW, 0, ":show:"}},
    {"typedef", {TYPEDEF}},
    {"unless", {UNLESS}},
    {"unsigned", {TYPE, UNSIGNED}},
    {"xr", {XU, XR}},
    {"xs", {XU, XS}},
};

std::optional<int> ParseLtlToken(const std::string &value) {
  auto it = kLtlSyms.find(value);
  if (it == kLtlSyms.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<Name> ParseNameToken(const std::string &value) {
  auto it = kNames.find(value);
  if (it == kNames.end()) {
    return std::nullopt;
  }
  return it->second;
}

} // namespace helpers