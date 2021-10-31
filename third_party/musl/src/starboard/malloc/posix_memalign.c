#include <errno.h>
#include <stdlib.h>

int posix_memalign(void **res, size_t align, size_t len) {
  if (align < sizeof(void *)) return EINVAL;
  void *mem = __memalign(align, len);
  // Note that the original posix_memalign() implementation returns errno here,
  // set to either EINVAL (if the alignment argument is not a power of 2) or
  // ENOMEM (if there was insufficient memory to fulfill the allocation
  // request). These are also the two possible errors from memalign() when NULL
  // is returned. errno can't be used here because it leaks __errno_location.
  // Also, the Starboardized implementation of memalign() returns
  // SbMemoryAllocateAligned(), whose behavior is undefined when the alignment
  // is not a power of 2. So ENOMEM is returned here.
  if (!mem) return ENOMEM;
  *res = mem;
  return 0;
}
