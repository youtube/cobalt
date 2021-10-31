// Copyright 2016 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Adapted from:
// lbshell/src/platform/linux/posix_emulation/lb_shell/lb_memory_pages_linux.cc

#include "starboard/shared/dlmalloc/page_internal.h"

#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include "starboard/atomic.h"
#include "starboard/common/log.h"
#include "starboard/common/memory.h"
#include "starboard/configuration_constants.h"
#include "starboard/memory.h"

namespace {

int32_t s_tracked_page_count = 0;

int32_t GetPageCount(size_t byte_count) {
  return static_cast<int32_t>(
      starboard::common::MemoryAlignToPageSize(byte_count) / kSbMemoryPageSize);
}

int SbMemoryMapFlagsToMmapProtect(int sb_flags) {
  bool flag_set = false;
  int mmap_protect = 0;
  if (sb_flags == kSbMemoryMapProtectReserved) {
    return PROT_NONE;
  }
  if (sb_flags & kSbMemoryMapProtectRead) {
    mmap_protect |= PROT_READ;
    flag_set = true;
  }
  if (sb_flags & kSbMemoryMapProtectWrite) {
    mmap_protect |= PROT_WRITE;
    flag_set = true;
  }
#if SB_CAN(MAP_EXECUTABLE_MEMORY)
  if (sb_flags & kSbMemoryMapProtectExec) {
    mmap_protect |= PROT_EXEC;
    flag_set = true;
  }
#endif
  if (!flag_set) {
    mmap_protect = PROT_NONE;
  }
  return mmap_protect;
}

}  // namespace

void* SbPageMap(size_t size_bytes, int flags, const char* unused_name) {
  void* ret = SbPageMapUntracked(size_bytes, flags, NULL);
  if (ret != SB_MEMORY_MAP_FAILED) {
    SbAtomicNoBarrier_Increment(&s_tracked_page_count,
                                GetPageCount(size_bytes));
  }
  return ret;
}

void* SbPageMapUntracked(size_t size_bytes,
                         int flags,
                         const char* unused_name) {
#if SB_CAN(MAP_EXECUTABLE_MEMORY)
  if (flags & kSbMemoryMapProtectExec) {
    // Cobalt does not allow mapping executable memory directly.
    return SB_MEMORY_MAP_FAILED;
  }
#endif
  int mmap_protect = SbMemoryMapFlagsToMmapProtect(flags);
  void* mem = mmap(0, size_bytes, mmap_protect, MAP_PRIVATE | MAP_ANON, -1, 0);
  return mem;
}

void* SbPageMapFile(void* addr,
                    const char* path,
                    SbMemoryMapFlags flags,
                    int64_t file_offset,
                    int64_t size) {
  int fd = open(path, O_RDONLY);
  if (fd == -1) {
    return nullptr;
  }

  void* p = nullptr;
  if (addr != nullptr) {
    p = mmap(addr, size, SbMemoryMapFlagsToMmapProtect(flags),
             MAP_PRIVATE | MAP_FIXED, fd, file_offset);
    if (p == MAP_FAILED) {
      close(fd);
      return nullptr;
    }
  } else {
    p = mmap(addr, size, SbMemoryMapFlagsToMmapProtect(flags), MAP_PRIVATE, fd,
             file_offset);
    if (p == MAP_FAILED) {
      close(fd);
      return nullptr;
    }
  }
  // It is OK to close the file descriptor as the memory
  // mapping keeps the file open.
  close(fd);
  return p;
}

bool SbPageUnmap(void* ptr, size_t size_bytes) {
  SbAtomicNoBarrier_Increment(&s_tracked_page_count, -GetPageCount(size_bytes));
  return SbPageUnmapUntracked(ptr, size_bytes);
}

bool SbPageUnmapUntracked(void* ptr, size_t size_bytes) {
  return munmap(ptr, size_bytes) == 0;
}

bool SbPageProtect(void* virtual_address, int64_t size_bytes, int flags) {
  int mmap_protect = SbMemoryMapFlagsToMmapProtect(flags);
  return mprotect(virtual_address, size_bytes, mmap_protect) == 0;
}

size_t SbPageGetTotalPhysicalMemoryBytes() {
  // Limit ourselves to remain similar to more constrained platforms.
  return 1024U * 1024 * 1024;
}

int64_t SbPageGetUnallocatedPhysicalMemoryBytes() {
  // Computes unallocated memory as the total system memory (our fake 1GB limit)
  // minus the # of resident pages.

  // statm provides info about our memory usage.
  // Columns are: size, resident, share, text, lib, data, and dt.
  // Just consider "resident" pages for our purposes.
  const char* kStatmPath = "/proc/self/statm";
  FILE* f = fopen(kStatmPath, "r");
  if (!f) {
    SB_DLOG(FATAL) << "Failed to open " << kStatmPath;
    return 0;
  }
  size_t program_size = 0;
  size_t resident = 0;

  fscanf(f, "%zu %zu", &program_size, &resident);
  fclose(f);
  return SbPageGetTotalPhysicalMemoryBytes() - resident * kSbMemoryPageSize;
}

size_t SbPageGetMappedBytes() {
  return static_cast<size_t>(SbAtomicNoBarrier_Load(&s_tracked_page_count) *
                             kSbMemoryPageSize);
}
