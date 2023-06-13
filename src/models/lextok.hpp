#pragma once

#include "models_fwd.hpp"

namespace models {

/**
 * @struct Lextok
 * Structure representing a lexical token in the parse tree.
 */
struct Lextok {
    unsigned short node_type; /**< Node type. */
    bool is_mtype_token; /**< Indicates if it is a CONST derived from MTYP. */
    int value; /**< Value attribute. */
    int line_number; /**< Line number. */
    int index_step; /**< Part of the d_step sequence. */
    int opt_inline_id; /**< Inline ID, if non-zero. */
    Symbol *file_name; /**< File name. */
    Symbol *symbol; /**< Symbol reference. */
    Sequence *sequence; /**< Sequence. */
    SeqList *seq_list; /**< Sequence list. */
    Lextok *left; /**< Left child in the parse tree. */
    Lextok *right; /**< Right child in the parse tree. */

    void ProcessSymbolForRead();
    int ResolveSymbolType();
    Lextok *AddTail(Lextok *tail);

    static Lextok *nn(models::Lextok *symbol, int type, models::Lextok *left,
                      models::Lextok *right);
    static Lextok *CreateRemoteLabelAssignment(models::Symbol *proctype_name,
                                               models::Lextok *pid,
                                               models::Symbol *label_name);
    static Lextok *CreateRemoteVariableAssignment(models::Symbol *a,
                                                  models::Lextok *b,
                                                  models::Symbol *c,
                                                  models::Lextok *ndx);
};

} // namespace models