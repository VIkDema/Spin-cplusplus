#pragma once

namespace utils::verbose {
/**
 * @class Flags
 * @brief Controls verbosity flags for printing debug information.
 *
 * The Flags class allows controlling various verbosity flags to print debug
 * information. Use the provided setter methods to enable specific flags and the
 * Active() method to check if any flags are active.
 */
class Flags {
private:
  /**
   * @brief Default constructor.
   */
  Flags();

  /**
   * @brief Copy constructor.
   * @param[in] other The Flags object to be copied.
   */
  Flags(const Flags &other);

  /**
   * @brief Assignment operator.
   * @param[in] other The Flags object to be assigned.
   * @return The reference to the assigned Flags object.
   */
  Flags &operator=(Flags &other);

public:
  /**
   * @brief Check if the flag to print global variables is active.
   * @return True if the flag is active, false otherwise.
   */
  bool NeedToPrintGlobalVariables();

  /**
   * @brief Check if the flag to print local variables is active.
   * @return True if the flag is active, false otherwise.
   */
  bool NeedToPrintLocalVariables();

  /**
   * @brief Check if the flag to print all process actions is active.
   * @return True if the flag is active, false otherwise.
   */
  bool NeedToPrintAllProcessActions();

  /**
   * @brief Check if the flag to print receive actions is active.
   * @return True if the flag is active, false otherwise.
   */
  bool NeedToPrintReceives();

  /**
   * @brief Check if the flag to print send actions is active.
   * @return True if the flag is active, false otherwise.
   */
  bool NeedToPrintSends();

  /**
   * @brief Check if the flag to print verbose information is active.
   * @return True if the flag is active, false otherwise.
   */
  bool NeedToPrintVerbose();

  /**
   * @brief Check if the flag to print very verbose information is active.
   * @return True if the flag is active, false otherwise.
   */
  bool NeedToPrintVeryVerbose();

  /**
   * @brief Check if any verbosity flags are active.
   * @return True if any flag is active, false otherwise.
   */
  bool Active();

  /**
   * @brief Clean all verbosity flags.
   */
  void Clean();

  /**
   * @brief Activate all verbosity flags.
   */
  void Activate();

  /**
   * @brief Set the flag to print global variables.
   */
  void SetNeedToPrintGlobalVariables();

  /**
   * @brief Set the flag to print local variables.
   */
  void SetNeedToPrintLocalVariables();

  /**
   * @brief Set the flag to print all process actions.
   */
  void SetNeedToPrintAllProcessActions();

  /**
   * @brief Set the flag to print receive actions.
   */
  void SetNeedToPrintReceives();

  /**
   * @brief Set the flag to print send actions.
   */
  void SetNeedToPrintSends();

  /**
   * @brief Set the flag to print verbose information.
   */
  void SetNeedToPrintVerbose();

  /**
   * @brief Set the flag to print very verbose information.
   */
  void SetNeedToPrintVeryVerbose();
  static Flags &getInstance() {
    static Flags instance;
    return instance;
  }

private:
    bool clean_;
    bool need_to_print_global_variables_;
    bool need_to_print_local_variables_;
    bool need_to_print_all_process_actions_;
    bool need_to_print_receives_;
    bool need_to_print_sends_;
    bool need_to_print_verbose_;
    bool need_to_print_very_verbose_;
};

} // namespace utils::verbose