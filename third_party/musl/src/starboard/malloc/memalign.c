#include <stdlib.h>
#include "starboard/memory.h"

void *__memalign(size_t align, size_t len) {
  return SbMemoryAllocateAligned(align, len);
}
