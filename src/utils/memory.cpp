#include "memory.hpp"
#include "../fatal/fatal.hpp"
#include <stdlib.h>

char *emalloc(size_t n) {
  char *tmp;
  static unsigned long count = 0;

  if (n == 0)
    return nullptr; /* robert shelton 10/20/06 */

  if (!(tmp = (char *)malloc(n))) {
    printf("spin++: allocated %ld Gb, wanted %d bytes more\n",
           count / (1024 * 1024 * 1024), (int)n);
    loger::fatal("not enough memory");
  }
  count += (unsigned long)n;
  memset(tmp, 0, n);
  return tmp;
}