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
  kCodeFrag = 2,
  kProctype = 301
};
/**
 * @struct Symbol
 * Structure representing a symbol.
 */
struct Symbol {
    std::string name; /**< Name of the symbol. */
    short id; /**< Unique number for the name. */
    SymbolType type = kNone; /**< Type of the symbol (bit, short, chan, struct, etc.). */
    unsigned char hidden_flags; /**< Bit flags indicating various properties of the symbol. */
    unsigned char color_number; /**< Color number for use in the IDE. */
    bool is_array; /**< Flag indicating if the symbol is an array. */
    std::string block_scope; /**< Block scope of the symbol. */
    int sc; /**< Scope sequence number (set only for proctypes, currently unused). */
    std::optional<int> nbits; /**< Optional width specifier for the symbol. */
    int value_type; /**< Value type of the symbol (1 if scalar, >1 if array). */
    int last_depth; /**< Last depth value changed. */
    std::vector<int> value; /**< Runtime value(s) of the symbol (initialized to 0). */
    Lextok **Sval; /**< Values for structures. */
    int xu; /**< Exclusive read or write by 1 pid. */
    Lextok *init_value; /**< Initial value or channel definition for the symbol. */
    Lextok *struct_template; /**< Template for the structure if the symbol is a struct. */
    Symbol *xup[2]; /**< Symbol representing exclusive read or write proctype. */
    Access *access; /**< Access information for the symbol (e.g., senders and receivers of a channel). */
    Symbol *mtype_name; /**< Name of the mtype if the symbol's type is MTYPE. */
    Symbol *struct_name; /**< Name of the defining struct if the symbol is a struct. */
    Symbol *owner_name; /**< Symbol representing the owner name (used for subfields in typedefs). */
    Symbol *context; /**< Symbol representing the context (0 if global, or procname). */
    Symbol *next; /**< Pointer to the next symbol in the linked list. */
    
    /**
     * @brief Add access information for the symbol.
     * @param what The symbol being accessed.
     * @param count The parameter number and additional information (e.g., 's' or 'r').
     * @param type The type of access.
     */
    void AddAccess(Symbol *what, int count, int type);
    
    /**
     * @brief Detect side effects of the symbol.
     */
    void DetectSideEffects();
    
    /**
     * @brief Check if the symbol represents a proctype.
     * @return True if the symbol represents a proctype, false otherwise.
     */
    bool IsProctype();
    
    /**
     * @brief Build or find a symbol with the given name.
     * @param name The name of the symbol.
     * @return The constructed or found symbol.
     */
    static Symbol *BuildOrFind(const std::string &name);
    
    /**
     * @brief Set the context symbol.
     * @param context The context symbol.
     */
    static void SetContext(Symbol *context);
    
    /**
     * @brief Get the current context symbol.
     * @return The current context symbol.
     */
    static Symbol *GetContext();

private:
    static Symbol *context_; /**< Static member representing the context symbol. */
};

struct Ordered { /* links all names in Symbol table */
  Symbol *entry;
  Ordered *next;
};

} // namespace models