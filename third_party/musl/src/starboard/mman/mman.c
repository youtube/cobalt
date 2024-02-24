#include <sys/mman.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#if SB_API_VERSION < 16
#include "starboard/memory.h"

static SbMemoryMapFlags ToSbMemoryMapFlags(int prot) {
  SbMemoryMapFlags sb_prot = kSbMemoryMapProtectReserved;
  if (prot & PROT_READ) {
    sb_prot |= kSbMemoryMapProtectRead;
  }
  if (prot & PROT_WRITE) {
    sb_prot |= kSbMemoryMapProtectWrite;
  }
#if SB_CAN(MAP_EXECUTABLE_MEMORY)
  if (prot & PROT_EXEC) {
    sb_prot |= kSbMemoryMapProtectExec;
  }
#endif

  return sb_prot;
}

void *mmap (void* addr, size_t len, int prot, int flags, int fd, off_t off) {
  if (addr != NULL) {
    return MAP_FAILED;
  }
  if (fd != -1) {
      return MAP_FAILED;
  }
  void*p  = SbMemoryMap(len , ToSbMemoryMapFlags(prot), "musl allocation");
  if (!p) {
    return MAP_FAILED;
  }
  return p;
}

int munmap (void* addr, size_t len) {
  if (SbMemoryUnmap(addr, len)) {
    return 0;
  }
  return -1;
}

int mprotect (void* addr, size_t len, int prot) {
  if (SbMemoryProtect(addr, len, ToSbMemoryMapFlags(prot))) {
    return 0;
  }
  return -1;
}

int msync (void * addr, size_t len, int flags) {
  SbMemoryFlush(addr, len);
  return 0;
}

#endif  // SB_API_VERSION < 16
