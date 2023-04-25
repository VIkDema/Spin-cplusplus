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

} // namespace utils::seed