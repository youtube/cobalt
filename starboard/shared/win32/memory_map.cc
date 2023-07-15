// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/memory.h"

#include <windows.h>

#include "starboard/common/log.h"

void* SbMemoryMap(int64_t size_bytes, int flags, const char* name) {
  if (size_bytes == 0) {
    return SB_MEMORY_MAP_FAILED;
  }
  ULONG protect = PAGE_NOACCESS;
  // |flags| is a bitmask of SbMemoryMapFlags, but |protect| is not a bitmask.
  switch (flags) {
    case kSbMemoryMapProtectReserved: {
      protect = PAGE_NOACCESS;
      break;
    }
    case kSbMemoryMapProtectRead:
      protect = PAGE_READONLY;
      break;
    case kSbMemoryMapProtectWrite:
      // Windows does not provide write only mode privileges
      // are escalated to read/write.
    case kSbMemoryMapProtectReadWrite:
      protect = PAGE_READWRITE;
      break;
    default:
      SB_NOTIMPLEMENTED() << "memory flag " << flags << " not supported.";
      return SB_MEMORY_MAP_FAILED;
  }

  void* memory = VirtualAllocFromApp(
      NULL, size_bytes,
      (flags == kSbMemoryMapProtectReserved) ? MEM_RESERVE : MEM_COMMIT,
      protect);
  return memory;
}
