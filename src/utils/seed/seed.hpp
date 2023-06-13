#pragma once

/**
 * @file
 * @brief Contains the declaration of the Seed class.
 */

namespace utils::seed {

/**
 * @brief The Seed class represents a seed for random number generation.
 *
 * The Seed class provides functionality for generating and managing a seed,
 * as well as determining whether the seed should be printed.
 */
class Seed {
private:
  /**
   * @brief Default private constructor.
   */
  Seed();

  /**
   * @brief Private copy constructor.
   * @param other The Seed object to be copied.
   */
  Seed(const Seed &other);

  /**
   * @brief Private assignment operator.
   * @param other The Seed object to be assigned.
   * @return A reference to the assigned Seed object.
   */
  Seed &operator=(Seed &other);

public:
  /**
   * @brief Get the current seed value.
   * @return The current seed value.
   */
  int GetSeed();

  /**
   * @brief Set a new seed value.
   * @param seed The new seed value to be set.
   */
  void SetSeed(int seed);

  /**
   * @brief Generate a new random seed value.
   */
  void GenerateSeed();

  /**
   * @brief Check if the seed value needs to be printed.
   * @return True if the seed value needs to be printed, false otherwise.
   */
  bool NeedToPrintSeed();

  /**
   * @brief Set whether the seed value needs to be printed.
   * @param need_to_print_seed True if the seed value needs to be printed, false otherwise.
   */
  void SetNeedToPrintSeed(bool need_to_print_seed);

  /**
   * @brief Generate a random long value.
   * @return The generated random long value.
   */
  static long Rand();

  /**
   * @brief Get the singleton instance of the Seed class.
   * @return The reference to the singleton instance of the Seed class.
   */
  static Seed &getInstance(){
    static Seed instance;
    return instance;
  }

private:
  bool need_to_print_seed_; ///< Flag indicating whether the seed value needs to be printed.
  int seed_; ///< The current seed value.
};

} // namespace utils::seed
