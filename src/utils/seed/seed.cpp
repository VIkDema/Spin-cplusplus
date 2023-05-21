#include "seed.hpp"

#include <iostream>
#include <time.h>

namespace utils::seed {

Seed::Seed() {}

Seed::Seed(const Seed &) {}

Seed &Seed::operator=(Seed &other) {
  if (this == &other) {
    return *this;
  }
  return *this;
}

int Seed::GetSeed() { return seed_; }

void Seed::SetSeed(int seed) { seed_ = seed; }

void Seed::GenerateSeed() { seed_ = (int)time((time_t *)0); }

bool Seed::NeedToPrintSeed() { return need_to_print_seed_; }

void Seed::SetNeedToPrintSeed(bool need_to_print_seed) {
  need_to_print_seed_ = need_to_print_seed;
}

long Seed::Rand() {
  auto &seed = utils::seed::Seed::getInstance();
  auto Seed = seed.GetSeed();

  Seed = 16807 * (Seed % 127773) - 2836 * (Seed / 127773);

  if (Seed <= 0) {
    Seed += 2147483647;
  }

  seed.SetSeed(Seed);
  return Seed;
}

} // namespace utils::seed