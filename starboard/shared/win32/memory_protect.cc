// Copyright 2018 Google Inc. All Rights Reserved.
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

bool SbMemoryProtect(void* virtual_address, int64_t size_bytes, int flags) {
  ULONG new_protection = 0;

  switch (flags) {
    case 0:
      new_protection = PAGE_NOACCESS;
      break;
    case kSbMemoryMapProtectRead:
      new_protection = PAGE_READONLY;
      break;

    // Windows does not provide write only mode privileges
    // are escalated to read/write.
    case kSbMemoryMapProtectWrite:
    case kSbMemoryMapProtectReadWrite:
      new_protection = PAGE_READWRITE;
      break;

#if SB_CAN(MAP_EXECUTABLE_MEMORY)
    case kSbMemoryMapProtectExec:
      new_protection = PAGE_EXECUTE;
      break;
    case kSbMemoryMapProtectRead | kSbMemoryMapProtectExec:
      new_protection = PAGE_EXECUTE_READ;
      break;
#endif

    // No other protections are supported, see
    // https://msdn.microsoft.com/en-us/library/windows/desktop/mt169846.
    default:
      return false;
  }

  ULONG old_protection;
  return VirtualProtectFromApp(virtual_address, size_bytes, new_protection,
                               &old_protection) != 0;
}
