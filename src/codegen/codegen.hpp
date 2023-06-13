#pragma once

#include "../models/models_fwd.hpp"
#include <cstdio>
#include <string>

namespace codegen {
/**
 * @brief Generates the code table.
 *
 * The `GenCodeTable` function generates the code table based on the specified
 * file pointer.
 *
 * @param fd The file pointer for the output file.
 */
void GenCodeTable(FILE *fd);

/**
 * @brief Inserts an inline section into the code.
 *
 * The `PlunkInline` function inserts an inline section into the code based on
 * the specified parameters.
 *
 * @param fd The file pointer for the output file.
 * @param s The inline section to insert.
 * @param how The insertion method.
 * @param gencode Indicates whether to generate code or not.
 */
void PlunkInline(FILE *fd, const std::string &s, int how, int gencode);

/**
 * @brief Inserts an expression into the code.
 *
 * The `PlunkExpr` function inserts an expression into the code based on the
 * specified string.
 *
 * @param fd The file pointer for the output file.
 * @param s The expression to insert.
 */
void PlunkExpr(FILE *fd, const std::string &s);

/**
 * @brief Handles the C state.
 *
 * The `HandleCState` function handles the C state based on the specified
 * symbols.
 *
 * @param s The first symbol.
 * @param t The second symbol.
 * @param ival The third symbol.
 */
void HandleCState(models::Symbol *s, models::Symbol *t, models::Symbol *ival);

/**
 * @brief Handles the C track.
 *
 * The `HandleCTrack` function handles the C track based on the specified
 * symbols.
 *
 * @param s The first symbol.
 * @param t The second symbol.
 * @param stackonly The third symbol.
 */
void HandleCTrack(models::Symbol *s, models::Symbol *t,
                  models::Symbol *stackonly);

/**
 * @brief Handles the C declarations.
 *
 * The `HandleCDescls` function handles the C declarations.
 */
void HandleCDescls(FILE *fd);

/**
 * @brief Handles the C FCTS.
 *
 * The `HandleCFCTS` function handles the C FCTS.
 */
void HandleCFCTS(FILE *fd);

/**
 * @brief Adds a local initialization.
 *
 * The `AddLocInit` function adds a local initialization based on the specified
 * parameters.
 *
 * @param fd The file pointer for the output file.
 * @param tpnr The type number.
 * @param pnm The parameter name.
 */
void AddLocInit(FILE *fd, int tpnr, const std::string &pnm);

/**
 * @brief Previews the C code.
 *
 * The `CPreview` function previews the C code.
 */
void CPreview();

/**
 * @brief Adds an SV.
 *
 * The `CAddSv` function adds an SV and returns its index.
 *
 * @param fd The file pointer for the output file.
 * @return The index of the added SV.
 */
int CAddSv(FILE *fd);

/**
 * @brief Sets the C stack size.
 *
 * The `CStackSize` function sets the C stack size based on the specified file
 * pointer.
 *
 * @param fd The file pointer for the output file.
 */
void CStackSize(FILE *fd);

/**
 * @brief Adds a stack.
 *
 * The `CAddStack` function adds a stack based on the specified file pointer.
 *
 * @param fd The file pointer for the output file.
 */
void CAddStack(FILE *fd);

/**
 * @brief Adds a location.
 *
 * The `CAddLoc` function adds a location based on the specified file pointer
 * and string.
 *
 * @param fd The file pointer for the output file.
 * @param s The location string to add.
 */
void CAddLoc(FILE *fd, const std::string &s);

/**
 * @brief Adds a definition.
 *
 * The `CAddDef` function adds a definition based on the specified file pointer.
 *
 * @param fd The file pointer for the output file.
 */
void CAddDef(FILE *fd);

/**
 * @brief Preprocesses the ruse.
 *
 * The `PreRuse` function preprocesses the ruse based on the specified file
 * pointer and Lextok.
 *
 * @param fd The file pointer for the output file.
 * @param n The Lextok to preprocess.
 */
void PreRuse(FILE *fd, models::Lextok *n);

/**
 * @brief Inserts an inline code into the file.
 *
 * The `PutInline` function inserts an inline code into the file based on the
 * specified string and returns a pointer to the inserted code.
 *
 * @param fd The file pointer for the output file.
 * @param s The inline code to insert.
 * @return A pointer to the inserted code.
 */
char *PutInline(FILE *fd, const std::string &s);
} // namespace codegen