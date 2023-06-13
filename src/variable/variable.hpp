#pragma once
#include "../models/models_fwd.hpp"
#include <cstdio>
#include <string>

namespace variable {

/**
 * @brief Dumps information about global variables.
 */
void DumpGlobals();

/**
 * @brief Dumps information about local variables.
 * @param runList Pointer to an object of type models::RunList.
 * @param value The value to check.
 */
void DumpLocal(models::RunList *runList, int value);

/**
 * @brief Dumps claims information.
 * @param file Pointer to the file.
 * @param value The value to check.
 * @param str The string containing additional information.
 */
void DumpClaims(FILE *file, int value, const std::string &str);

/**
 * @brief Gets the value of a Lextok object.
 * @param lextok Pointer to a models::Lextok object.
 * @return The value of the Lextok object.
 */
int GetValue(models::Lextok *lextok);

/**
 * @brief Sets the value of a Lextok object.
 * @param lextok Pointer to a models::Lextok object.
 * @param value The value to set.
 * @return The new value of the Lextok object.
 */
int SetVal(models::Lextok *lextok, int value);

/**
 * @brief Checks the variable.
 * @param symbol Pointer to a models::Symbol object.
 * @param value The value to check.
 * @return The result of the variable check.
 */
int CheckVar(models::Symbol *symbol, int value);

/**
 * @brief Casts the value to a specified type.
 * @param value The value to cast.
 * @param type The type to cast to.
 * @param size The size of the type.
 * @return The casted value.
 */
int CastValue(int value, int type, int size);

} // namespace variable
