#pragma once
#include "../models/models_fwd.hpp"
#include "file_stream.hpp"

namespace lexer {
constexpr int kMaxInl = 16;

/**
 * @brief Class responsible for inline processing.
 */
class InlineProcessor {
public:
  /**
   * @brief Checks if inlining is enabled.
   * @return True if inlining is enabled, false otherwise.
   */
  static bool HasInlining();

  /**
   * @brief Gets the inline stub at the specified index.
   * @param index The index of the inline stub.
   * @return The inline stub at the specified index.
   */
  static models::IType *GetInlineStub(int index);

  /**
   * @brief Gets the current inline stub.
   * @return The current inline stub.
   */
  static models::IType *GetInlineStub();

  /**
   * @brief Checks if redirection is enabled.
   * @return True if redirection is enabled, false otherwise.
   */
  static bool HasReDiRect();

  /**
   * @brief Gets the redirection value.
   * @return The redirection value.
   */
  static char *GetReDiRect();

  /**
   * @brief Gets the current inlining count.
   * @return The current inlining count.
   */
  static int GetInlining();

  /**
   * @brief Sets the redirection value.
   * @param value The new redirection value.
   */
  static void SetReDiRect(char *value);

  /**
   * @brief Gets the inlining count and processes the input file stream.
   * @param file_stream The input file stream.
   * @return The inlining count.
   */
  static int GetInline(file::FileStream &file_stream);

  /**
   * @brief Disables inlining.
   */
  static void Uninline();

  /**
   * @brief Gets the sequence names.
   * @return The sequence names.
   */
  static models::IType *GetSeqNames();

  /**
   * @brief Adds sequence names.
   * @param tmp The sequence names to add.
   */
  static void AddSeqNames(models::IType *tmp);

  /**
   * @brief Finds an inline stub with the specified name.
   * @param s The name of the inline stub to find.
   * @return The found inline stub, or nullptr if not found.
   */
  static models::IType *FindInline(const std::string &s);

  /**
   * @brief Sets the inline expression flag.
   */
  static void SetIsExpr();

  /**
   * @brief Gets the current inline UUID.
   * @return The current inline UUID.
   */
  static int GetCurrInlineUuid();

  /**
   * @brief Checks if the given value is an EQNAME.
   * @param value The value to check.
   * @return True if the value is an EQNAME, false otherwise.
   */
  static bool IsEqname(const std::string &value);

  /**
   * @brief Sets the inline stub at the specified index.
   * @param new_itype The new inline stub.
   * @param name The name of the inline stub.
   */
  static void SetInlineStub(models::IType *new_itype, const std::string &name);

  /**
   * @brief Sets the inliner.
   * @param new_inliner The new inliner.
   */
  static void SetInliner(char *new_inliner);

  /**
   * @brief Increments the inlining count.
   */
  static void IncInlining();

  /**
   * @brief Checks for global inlining of the specified name.
   * @param s The name to check.
   * @return The inlining count if found, -1 otherwise.
   */
  static int CheckGlobInline(const std::string &s);

  /**
   * @brief Checks if inlining is allowed for the given inline stub.
   * @param itype The inline stub to check.
   */
  static void CheckInline(models::IType *itype);

private:
  /**
   * @brief The current inlining count.
   */
  static int Inlining;

  /**
   * @brief The redirection value.
   */
  static char *ReDiRect_;

  /**
   * @brief The inliner.
   */
  static char *Inliner[kMaxInl];

  /**
   * @brief The array of inline stubs.
   */
  static models::IType *Inline_stub[kMaxInl];

  /**
   * @brief The sequence names.
   */
  static models::IType *seqnames;
};
} // namespace lexer