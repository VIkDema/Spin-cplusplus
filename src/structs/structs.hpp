#pragma once
#include "../models/models_fwd.hpp"
#include <string>

namespace structs {
/**
 * @brief Sets the parent name of a given Lextok.
 *
 * @param lextok A pointer to the Lextok whose parent name needs to be set.
 */
void SetPname(models::Lextok *);

/**
 * @brief Creates an explicit Lextok with a given token, line number, and column number.
 *
 * @param token The token value for the explicit Lextok.
 * @param line The line number for the explicit Lextok.
 * @param col The column number for the explicit Lextok.
 * @return A pointer to the created explicit Lextok.
 */
models::Lextok *mk_explicit(models::Lextok *, int, int);

/**
 * @brief Expands a given Lextok using the specified depth.
 *
 * @param lextok A pointer to the Lextok that needs to be expanded.
 * @param depth The depth of expansion.
 * @return A pointer to the expanded Lextok.
 */
models::Lextok *ExpandLextok(models::Lextok *, int);


/**
 * @brief Dumps the structure of a Symbol into a file using a specified name and run list.
 *
 * @param symbol A pointer to the Symbol whose structure needs to be dumped.
 * @param name The name for the dump.
 * @param runList A pointer to the RunList for the dump.
 */
void DumpStruct(models::Symbol *, const std::string &, models::RunList *);

/**
 * @brief Dumps the structure of a Symbol into a file stream using a specified name.
 *
 * @param file The file stream where the structure should be dumped.
 * @param name The name for the dump.
 * @param symbol A pointer to the Symbol whose structure needs to be dumped.
 */

void CStruct(FILE *, const std::string &, models::Symbol *);

/**
 * @brief Walks the structure of a Symbol and writes it to a file stream.
 *
 * @param file The file stream where the structure should be written.
 * @param indent The indentation level for the structure.
 * @param name The name of the Symbol.
 * @param symbol A pointer to the Symbol whose structure needs to be walked.
 * @param prefix The prefix for field names.
 * @param enumName The enum name for enumeration values.
 * @param bitfieldName The bitfield name for bitfield values.
 */
void WalkStruct(FILE *, int, const std::string &, models::Symbol *,
                const std::string &, const std::string &, const std::string &);

/**
 * @brief Walks the structure of a Symbol and writes it to a file using a specified name.
 *
 * @param name The name of the Symbol.
 * @param symbol A pointer to the Symbol whose structure needs to be walked.
 */
void WalkStruct(const std::string &, models::Symbol *);

/**
 * @brief Gets the structure name from a given Lextok and Symbol.
 *
 * @param lextok A pointer to the Lextok.
 * @param symbol A pointer to the Symbol.
 * @param index The index for the structure name.
 * @param result[out] The resulting structure name.
 */
void GetStructName(models::Lextok *, models::Symbol *, int, std::string &);

/**
 * @brief Checks the validity of a reference within a Lextok.
 *
 * @param lextok A pointer to the Lextok.
 * @param reference A pointer to the reference that needs to be checked.
 */
void CheckValidRef(models::Lextok *, models::Lextok *);

/**
 * @brief Gets the full name of a Lextok and Symbol and writes it to a file stream.
 *
 * @param file The file stream where the full name should be written.
 * @param lextok A pointer to the Lextok.
 * @param symbol A pointer to the Symbol.
 * @param index The index for the full name.
 * @return The number of characters written.
 */
int GetFullName(FILE *, models::Lextok *, models::Symbol *, int);

/**
 * @brief Initializes the structure of a Symbol.
 *
 * @param symbol A pointer to the Symbol that needs to be initialized.
 */
void InitStruct(models::Symbol *);

/**
 * @brief Gets the width of a Lextok at a given index.
 *
 * @param width A pointer to the array of widths.
 * @param index The index for the width.
 * @param lextok A pointer to the Lextok.
 * @return The width of the Lextok at the specified index.
 */
int GetWidth(int *, int, models::Lextok *);

/**
 * @brief Counts the number of fields in a Lextok.
 *
 * @param lextok A pointer to the Lextok.
 * @return The total number of fields.
 */
int CountFields(models::Lextok *);

/**
 * @brief Assigns a value to a struct member within a Lextok and Symbol.
 *
 * @param lextok A pointer to the Lextok.
 * @param symbol A pointer to the Symbol.
 * @param index The index of the struct member.
 * @param value The value to assign.
 * @return The result of the assignment.
 */
int Lval_struct(models::Lextok *, models::Symbol *, int, int);

/**
 * @brief Retrieves the value of a struct member within a Lextok and Symbol.
 *
 * @param lextok A pointer to the Lextok.
 * @param symbol A pointer to the Symbol.
 * @param index The index of the struct member.
 * @return The value of the struct member.
 */
int Rval_struct(models::Lextok *, models::Symbol *, int);

/**
 * @brief Sets the user-defined type within a Lextok and Symbol.
 *
 * @param lextok A pointer to the Lextok.
 * @param symbol A pointer to the Symbol.
 * @param utype A pointer to the user-defined type.
 */
void SetUtype(models::Lextok *, models::Symbol *, models::Lextok *);

/**
 * @brief Gets the user-defined type from a Symbol.
 *
 * @param symbol A pointer to the Symbol.
 * @return A pointer to the user-defined type.
 */
models::Lextok *GetUname(models::Symbol *);

/**
 * @brief Prints the user-defined type names to a file.
 *
 * @param file The file stream where the type names should be printed.
 */
void PrintUnames(FILE *);

/**
 * @brief Sets the user-defined name within a Lextok.
 *
 * @param lextok A pointer to the Lextok.
 */
void SetUname(models::Lextok *);

/**
 * @brief Checks if a given string is a user-defined type.
 *
 * @param str The string to check.
 * @return True if the string is a user-defined type, false otherwise.
 */
bool IsUtype(const std::string &);

} // namespace structs