#pragma once

#include "models_fwd.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace models {

/* val defines width in bits */
enum SymbolType {
  kUnsigned = 5,
  kBit = 1,
  kBite = 8,
  kShort = 16,
  kInt = 32,
  kChan = 64,
  kStruct = 128,
  kPredef = 3, /* predefined name: _p, _last */
  kLabel = 278,
  kName = 315
};

enum class SymbolFlag : unsigned char {
  kHide = 0x1,
  kShow = 0x2,
  kBitEquiv = 0x4,
  kByteEquiv = 0x8,
  kFormalPar = 0x10,
  kInlinePar = 0x20,
  kTreatLocal = 0x40,
  kReadAtLeastOnce = 0x80
};

struct Symbol {
  std::string name;
  short id; /* unique number for the name OLD id */

  SymbolType type;         /* bit,short,.., chan,struct  */
  SymbolFlag hidden_flags; /* bit-flags */

  unsigned char color_number; /* for use ide */
  bool is_array;              /* set if decl specifies array bound */

  std::string block_scope;

  int sc; /* scope seq no -- set only for proctypes unused */
  std::optional<std::byte> nbits; /* optional width specifier */
  int value_type;                 /* 1 if scalar, >1 if array   OLD: nel*/
  int last_depth;                 /* last depth value changed   */

  std::vector<int> value; /* runtime value(s), initl 0  */
  Lextok **Sval;          /* values for structures */

  int xu; /* exclusive r or w by 1 pid  */

  Lextok *init_value;      /* initial value, or chan-def */
  Lextok *struct_template; /* template for structure if struct */

  Symbol *xup[2];       /* xr or xs proctype  */
  Access *access;       /* e.g., senders and receives of chan */
  Symbol *mtype_name;   /* if type == MTYPE else nil */
  Symbol *struct_name;  /* name of the defining struct */
  Symbol *owner_name;   /* set for names of subfields in typedefs */
  Symbol *context;      /* 0 if global, or procname */
  models::Symbol *next; /* linked list */
};
} // namespace models