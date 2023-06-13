#pragma once

#include "models_fwd.hpp"

namespace models {
/**
 * @struct IType
 * Structure representing an inline type.
 */
struct IType {
    Symbol *nm; /**< Name of the type. */
    Lextok *cn; /**< Contents. */
    Lextok *params; /**< Formal parameters, if any. */
    Lextok *rval; /**< Variable to assign the return value, if any. */
    char **anms; /**< Literal text for actual parameters. */
    char *prec; /**< Precondition for c_code or c_expr. */
    int uiid; /**< Unique inline ID. */
    int is_expr; /**< Indicates if c_expr is in an LTL formula. */
    int dln, cln; /**< Definition and call line numbers. */
    Symbol *dfn, *cfn; /**< Definition and call filenames. */
    IType *next; /**< Linked list. */
};
} // namespace models
