#pragma once
#include "../models/models_fwd.hpp"
#include <string>

namespace symbol {
/**
 * @brief Checks the channel access.
 *
 * This function checks the channel access and performs necessary actions based on the result.
 * It does not return any value.
 */
void CheckChanAccess();

/**
 * @brief Dumps the symbols.
 *
 * This function dumps the symbols and prints them.
 * It does not return any value.
 */
void DumpSymbols();

/**
 * @brief Prints the symbol variable.
 * 
 * @param symbol A pointer to the symbol variable to be printed.
 * 
 * This function prints the given symbol variable.
 * It does not return any value.
 */
void PrintSymbolVariable(models::Symbol *);

/**
 * @brief Checks if the given string is an Mtype.
 * 
 * @param str The input string to be checked.
 * @return true if the string is an Mtype, false otherwise.
 * 
 * This function checks if the given string corresponds to an Mtype.
 */
int IsMtype(const std::string &);

/**
 * @brief Gets the Mtype name.
 * 
 * @param str The input string representing an Mtype.
 * @return The name of the Mtype.
 * 
 * This function returns the name of the Mtype for the given input string.
 */
std::string GetMtypeName(const std::string &);

/**
 * @brief Adds an Mtype.
 * 
 * @param lextok1 The first Lextok parameter.
 * @param lextok2 The second Lextok parameter.
 * 
 * This function adds an Mtype using the provided Lextok parameters.
 * It does not return any value.
 */
void AddMtype(models::Lextok *, models::Lextok *);

/**
 * @brief Gets the list of Mtypes for the given string.
 * 
 * @param s The input string.
 * @return A double pointer to the list of Mtypes.
 * 
 * This function retrieves the list of Mtypes associated with the given string.
 */
models::Lextok **GetListOfMtype(const std::string &s);


/**
 * @brief Sets the Xus value.
 * 
 * @param lextok The Lextok parameter.
 * @param value The value to be set.
 * 
 * This function sets the Xus value in the provided Lextok to the given value.
 * It does not return any value.
 */
void SetXus(models::Lextok *, int);

/**
 * @brief Sets the Pname value.
 * 
 * @param lextok1 The first Lextok parameter.
 * @param lextok2 The second Lextok parameter.
 * @param value The value to be set.
 * @param lextok3 The third Lextok parameter.
 * 
 * This function sets the Pname value using the provided Lextok parameters and value.
 * It does not return any value.
 */
void SetPname(models::Lextok *, models::Lextok *, int, models::Lextok *);

/**
 * @brief Tracks the use of a channel.
 * 
 * @param lextok1 The first Lextok parameter.
 * @param lextok2 The second Lextok parameter.
 * @param value The value to be set.
 * 
 * This function tracks the use of a channel using the provided Lextok parameters and value.
 * It does not return any value.
 */
void TrackUseChan(models::Lextok *, models::Lextok *, int);

/**
 * @brief Function to track a variable
 * @param n Pointer to a lexical token
 * @param m Pointer to a lexical token
 */
void TrackVar(models::Lextok* n, models::Lextok* m);
/**
 * @brief Function to eliminate ambiguity
 */
void EliminateAmbiguity();

/**
 * @brief Checks the run for a symbol.
 *
 * @param symbol A pointer to the symbol.
 * @param value The value to be checked.
 *
 * This function checks the run for the given symbol and performs necessary actions based on the value.
 * It does not return any value.
 */
void CheckRun(models::Symbol *, int);

/**
 * @brief Tracks the run using a Lextok.
 *
 * @param n The Lextok parameter.
 *
 * This function tracks the run using the provided Lextok parameter and performs necessary actions.
 * It does not return any value.
 */
void TrackRun(models::Lextok* n);

} // namespace symbol