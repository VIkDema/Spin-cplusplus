#pragma once

namespace utils::seed {
class Seed {
private:
  Seed();
  Seed(const Seed &);
  Seed &operator=(Seed &);

public:
  int GetSeed();
  void SetSeed(int seed);
  void GenerateSeed();

  static Seed &getInstance() {
    static Seed instance;
    return instance;
  }

private:
  int seed_;
};

} // namespace utils::seed