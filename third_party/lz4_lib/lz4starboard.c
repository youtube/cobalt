#include "starboard/memory.h"

void* LZ4_malloc(size_t s) {
  return SbMemoryAllocate(s);
}

void* LZ4_calloc(size_t n, size_t s) {
  return SbMemoryCalloc(n, s);
}

void LZ4_free(void* p) {
  SbMemoryDeallocate(p);
}
