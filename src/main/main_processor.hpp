#pragma once

#include "launch_settings.hpp"
/**
 * @brief Class representing the main processor of the program.
 */
class MainProcessor {
public:
  /**
   * @brief The main entry point of the program.
   * @param argc The number of command-line arguments.
   * @param argv An array of command-line argument strings.
   * @return The exit status of the program.
   */
  int main(int argc, char *argv[]);

  /**
   * @brief Executes a system command and returns the exit status.
   * @param code The exit status code to be returned.
   * @param message The message to be printed before exiting.
   * @return The exit status code.
   */
  static int e_system(int code, const std::string &message);

  /**
   * @brief Exits the program with the specified error status.
   * @param error_status The error status to exit with.
   */
  static void Exit(int error_status);

private:
  /**
   * @brief Changes a parameter value within a string.
   * @param t The string containing the parameter value.
   * @param what The name of the parameter to be changed.
   * @param range The range of valid values for the parameter.
   * @param bottom The bottom value for the parameter range.
   * @return The updated parameter value.
   */
  static int ChangeParam(std::string &t, const std::string &what, int range,
                         int bottom);

  /**
   * @brief Initializes the random seed for the program.
   */
  void InitSeed();

  /**
   * @brief Initializes the streams for input and output.
   */
  void InitStreams();

  /**
   * @brief Initializes the scope for symbol tables.
   */
  void InitScope();

  /**
   * @brief Initializes the preprocessor settings.
   */
  void InitPreProcSettings();

  /**
   * @brief Handles the launch settings passed as command-line arguments.
   * @param argc The number of command-line arguments.
   * @param argv An array of command-line argument strings.
   * @return True if the launch settings were successfully handled, false
   * otherwise.
   */
  bool HandleLaunchSettings(int argc, char *argv[]);

  /**
   * @brief Initializes the symbol tables.
   */
  void InitSymbols();

  /**
   * @brief Changes the random seed value within a string.
   * @param t The string containing the random seed value.
   */
  static void ChangeRandomSeed(std::string &t);

  /**
   * @brief Omits a substring from a string.
   * @param in The input string.
   * @param s The substring to be omitted.
   * @return The updated string after omitting the substring.
   */
  static int OmitStr(std::string &in, const std::string &s);

  /**
   * @brief Trims whitespace from the beginning and end of a string.
   * @param t The string to be trimmed.
   */
  static void StringTrim(std::string &t);

  /**
   * @brief Performs final adjustments or manipulations before program
   * execution.
   */
  static void FinalFiddle();

  /**
   * @brief The output string for the program.
   */
  static std::string out_;
};
