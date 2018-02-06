// Copyright 2016 Google Inc. All Rights Reserved.
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

#include <sys/mman.h>

#include <iomanip>

#include "starboard/log.h"

#if !SB_CAN(MAP_EXECUTABLE_MEMORY)
#error "You shouldn't implement SbMemoryFlush unless you can map " \
       "memory pages as executable"
#endif

void SbMemoryFlush(void* virtual_address, int64_t size_bytes) {
  char* memory = reinterpret_cast<char*>(virtual_address);
#if !SB_IS(ARCH_ARM) && !SB_IS(ARCH_MIPS)
  int result = msync(memory, size_bytes, MS_SYNC);
  SB_DCHECK(result == 0) << "msync failed: 0x" << std::hex << result << " ("
                         << std::dec << result << "d)";
#endif

#if !defined(__has_builtin)
#define __has_builtin(a) (0)
#endif

#if __has_builtin(__builtin___clear_cache)
  __builtin___clear_cache(memory, memory + size_bytes);
#elif defined(__clear_cache)
  __clear_cache(memory, memory + size_bytes);
#endif
}
