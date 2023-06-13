#pragma once

namespace helpers {

/**
 * @brief Class representing deferred execution.
 */
class Deferred {
public:
  /**
   * @brief Constructs a Deferred object.
   */
  Deferred();

  /**
   * @brief Checks if the execution is deferred.
   * @return True if the execution is deferred, false otherwise.
   */
  bool IsDiferred();

  /**
   * @brief Sets the deferred execution flag.
   * @param newDeferred The new value for the deferred flag.
   */
  void SetDiferred(bool newDeferred);

  /**
   * @brief Gets the value of the deferred execution flag.
   * @return The value of the deferred execution flag.
   */
  bool GetDeferred();

  /**
   * @brief Clears the deferred execution flag.
   */
  void ZapDeferred();

  /**
   * @brief Puts the execution in a deferred state.
   * @return True if the execution was put in a deferred state, false otherwise.
   */
  bool PutDeffered();

private:
  /**
   * @brief Flag indicating if the execution is deferred.
   */
  bool diferred_;
};


} // namespace helpers