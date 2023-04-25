#include "seed.hpp"

#include <time.h>

namespace utils::seed {

Seed::Seed() {}

Seed::Seed(const Seed &) {}

Seed &Seed::operator=(Seed &) {
  auto seed = Seed();
  return seed;
}

int Seed::GetSeed() { return seed_; }

void Seed::SetSeed(int seed) { seed_ = seed; }

void Seed::GenerateSeed() { seed_ = (int)time((time_t *)0); }

bool Seed::NeedToPrintSeed() { return need_to_print_seed_; }

void Seed::SetNeedToPrintSeed(bool need_to_print_seed) {
  need_to_print_seed_ = need_to_print_seed;
}

} // namespace utils::seed