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
  bool NeedToPrintSeed();
  void SetNeedToPrintSeed(bool need_to_print_seed);

  static Seed &getInstance() {
    static Seed instance;
    return instance;
  }

private:
  bool need_to_print_seed_;
  int seed_;
};

} // namespace utils::seed