#include "utils.hpp"

namespace utils {

unsigned int hash(const std::string &s) {
  unsigned int h = 0;

  for (char c : s) {
    h += static_cast<unsigned int>(c);
    h <<= 1;
    if (h & (255 + 1))
      h |= 1;
  }
  return h & 255;
}

} // namespace utils