#include <stdlib.h>

void* LZ4_malloc(size_t s) {
  return malloc(s);
}

void* LZ4_calloc(size_t n, size_t s) {
  return calloc(n, s);
}

void LZ4_free(void* p) {
  free(p);
}
