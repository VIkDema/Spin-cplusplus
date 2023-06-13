#pragma once
#include <string>

/**
 * @brief Class representing preprocessor settings.
 */
class PreProcSettings {
public:
  /**
   * @brief Initializes the preprocessor settings.
   */
  void Init();

  /**
   * @brief Sets the preprocessor command.
   * @param command The command to be set.
   */
  void SetCommand(const std::string &command);

  /**
   * @brief Returns the preprocessor command.
   * @return The preprocessor command.
   */
  std::string GetCommand();

  /**
   * @brief Checks if the preprocessor settings are set to the default values.
   * @return True if the settings are at default, false otherwise.
   */
  bool IsDefault();

  /**
   * @brief Preprocesses the given input files using the specified options.
   * @param a The first input file.
   * @param b The second input file.
   * @param a_tmp The temporary variable.
   */
  void Preprocess(const std::string &a, const std::string &b, int a_tmp);

private:
  /**
   * @brief Flag indicating if the preprocessor settings were changed.
   */
  bool was_changed_ = false;

  /**
   * @brief The preprocessor command.
   */
  std::string command_;
};


static PreProcSettings pre_proc_processor;