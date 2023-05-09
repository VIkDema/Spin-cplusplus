#pragma once

#include "models_fwd.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace models {

/* val defines width in bits */
enum SymbolType {
  kNone = 0,
  kUnsigned = 5,
  kBit = 1,
  kByte = 8,
  kShort = 16,
  kInt = 32,
  kChan = 64,
  kStruct = 128,
  kPredef = 3, /* predefined name: _p, _last */
  kLabel = 278,
  kName = 315,
  kMtype = 275,
  kCodeDecl = 4,
  kCodeFrag = 2
};

struct Symbol {
  std::string name;
  short id; /* unique number for the name OLD id */

  SymbolType type = kNone;            /* bit,short,.., chan,struct  */
  unsigned char hidden_flags; /* bit-flags:
                                   1=hide, 2=show,
                                   4=bit-equiv,   8=byte-equiv,
                                  16=formal par, 32=inline par,
                                  64=treat as if local; 128=read at least once
                                 */

  unsigned char color_number; /* for use ide */
  bool is_array;              /* set if decl specifies array bound */

  std::string block_scope;

  int sc; /* scope seq no -- set only for proctypes unused */
  std::optional<int> nbits; /* optional width specifier */
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

struct Ordered { /* links all names in Symbol table */
  models::Symbol *entry;
  struct Ordered *next;
};

} // namespace models