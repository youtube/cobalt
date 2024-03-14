// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include <sys/mman.h>
#include <windows.h>

#include "starboard/configuration.h"

extern "C" {

void* mmap(void* addr, size_t len, int prot, int flags, int fd, off_t off) {
  if (addr != NULL) {
    return MAP_FAILED;
  }
  if (fd != -1) {
    return MAP_FAILED;
  }
  if (len == 0) {
    return MAP_FAILED;
  }

  ULONG allocation_type = MEM_RESERVE;
  ULONG protect = PAGE_NOACCESS;

  if (prot & PROT_READ) {
    protect = PAGE_READONLY;
    allocation_type = MEM_COMMIT;
  }
  if (prot & PROT_WRITE) {
    // Windows does not provide write only mode privileges
    // are escalated to read/write.
    protect = PAGE_READWRITE;
    allocation_type = MEM_COMMIT;
  }

  void* p = VirtualAllocFromApp(NULL, len, allocation_type, protect);
  if (!p) {
    return MAP_FAILED;
  }
  return p;
}

int munmap(void* addr, size_t len) {
  if (VirtualFree(addr, len, MEM_DECOMMIT)) {
    return 0;
  }
  return -1;
}

int msync(void* addr, size_t len, int flags) {
  // TODO: Enable the following implementation when xb1 can compile it.
  // FlushInstructionCache(GetCurrentProcess(), virtual_address, size_bytes);
  return 0;
}

int mprotect(void* addr, size_t len, int prot) {
  ULONG new_protection = 0;

  if (prot == PROT_NONE) {
    // After this call, the address will be in reserved state.
    if (VirtualFree(addr, len, MEM_DECOMMIT)) {
      return 0;
    }
    return -1;
  }

  if (prot == PROT_READ) {
    new_protection = PAGE_READONLY;
  } else if (prot | PROT_WRITE) {
    // Windows does not provide write only mode privileges
    // are escalated to read/write.
    new_protection = PAGE_READWRITE;
  }

#if SB_CAN(MAP_EXECUTABLE_MEMORY)
  if (prot == PROT_EXEC) {
    new_protection = PAGE_EXECUTE;
  } else if (prot == (PROT_READ | PROT_EXEC)) {
    new_protection = PAGE_EXECUTE_READ;
  }
#endif

  ULONG old_protection;
  // Changing protection from No-Access to others needs the memory to be
  // committed first. Commit committed pages will not reset them to zero.
  VirtualAllocFromApp(addr, len, MEM_COMMIT, PAGE_READONLY);
  bool res = VirtualProtectFromApp(addr, len, new_protection, &old_protection);
  if (res) {
    return 0;
  }
  return -1;
}

}  // extern "C"
