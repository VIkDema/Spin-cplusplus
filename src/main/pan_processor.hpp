#pragma once
#include <fmt/core.h>
#include <string>

/**
 * @brief Class representing the Pan processor.
 */
class PanProcessor {
public:
  /**
   * @brief Adds runtime information to the Pan processor.
   * @param add The additional runtime information to be added.
   */
  void AddRuntime(const std::string &add);

  /**
   * @brief Sets the value of itsr_n for the Pan processor.
   * @param s The value of itsr_n to be set.
   */
  void SetItsr_n(const std::string &s);

  /**
   * @brief Adds compile-time information to the Pan processor.
   * @param add The additional compile-time information to be added.
   */
  void AddComptime(const std::string &add);

  /**
   * @brief Returns the Pan runtime information.
   * @return The Pan runtime information.
   */
  std::string& GetPanRuntime();

  /**
   * @brief Returns the Pan compile-time information.
   * @return The Pan compile-time information.
   */
  std::string& GetPanComptime();

  /**
   * @brief Returns the value of itsr.
   * @return The value of itsr.
   */
  int GetItsr() { return itsr_; }

  /**
   * @brief Increments the value of itsr by 1.
   */
  void IncItsr() { itsr_++; }

  /**
   * @brief Decrements the value of itsr by 1.
   */
  void DesItsr() { itsr_--; }

  /**
   * @brief Reverses the sign of itsr.
   */
  void ReverseItsr() { itsr_ = -itsr_; }

  /**
   * @brief Returns the value of sw_or_bt.
   * @return The value of sw_or_bt.
   */
  int GetSwOrBt() { return sw_or_bt_; }

  /**
   * @brief Returns the value of itsr_n.
   * @return The value of itsr_n.
   */
  int GetItsrN() { return itsr_n_; }

private:
  /**
   * @brief The Pan runtime information.
   */
  std::string pan_runtime_;

  /**
   * @brief The Pan compile-time information.
   */
  std::string pan_comptime_;

  /**
   * @brief The value of itsr.
   */
  int itsr_;

  /**
   * @brief The value of sw_or_bt.
   */
  int sw_or_bt_;

  /**
   * @brief The value of itsr_n.
   */
  int itsr_n_;
};
