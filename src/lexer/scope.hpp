#pragma once

#include <array>
#include <string>
#include <vector>

namespace lexer {
/**
 * @class ScopeProcessor
 * @brief Manages the scope processing functionality.
 *
 * The ScopeProcessor class provides methods to initialize, add, and remove
 * scopes, as well as retrieve information about the current scope.
 */
class ScopeProcessor {
public:
  /**
   * @brief Initializes the scope name.
   */
  static void InitScopeName();

  /**
   * @brief Sets the current scope.
   */
  static void SetCurrScope();

  /**
   * @brief Adds a new scope.
   */
  static void AddScope();

  /**
   * @brief Removes the current scope.
   */
  static void RemoveScope();

  /**
   * @brief Gets the name of the current scope.
   * @return The name of the current scope.
   */
  static std::string GetCurrScope();

  /**
   * @brief Gets the level of the current scope.
   * @return The level of the current scope.
   */
  static int GetCurrScopeLevel();

  /**
   * @brief Gets the current segment.
   * @return The current segment.
   */
  static int GetCurrSegment();

  /**
   * @brief Sets the level of the current scope.
   * @param scope_level The level of the current scope.
   */
  static void SetCurrScopeLevel(int scope_level);

private:
  static int scope_level_; /**< The level of the current scope. */
  static std::array<int, 256> scope_seq_; /**< The sequence of scopes. */
  static std::string curr_scope_name_;    /**< The name of the current scope. */
};

} // namespace lexer