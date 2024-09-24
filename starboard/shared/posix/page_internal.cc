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

#include "starboard/shared/posix/page_internal.h"

#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include "starboard/atomic.h"
#include "starboard/common/log.h"
#include "starboard/common/memory.h"
#include "starboard/configuration_constants.h"

namespace {

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
  if (sb_flags & kSbMemoryMapProtectExec) {
    mmap_protect |= PROT_EXEC;
    flag_set = true;
  }
  if (!flag_set) {
    mmap_protect = PROT_NONE;
  }
  return mmap_protect;
}

}  // namespace

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
