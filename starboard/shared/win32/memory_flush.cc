// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/log.h"
#include "starboard/memory.h"

#include <windows.h>

#if !SB_CAN(MAP_EXECUTABLE_MEMORY)
#error \
    "You shouldn't implement SbMemoryFlush unless you can map " \
       "memory pages as executable"
#endif

void SbMemoryFlush(void* virtual_address, int64_t size_bytes) {
  SB_NOTIMPLEMENTED();
  // TODO: Enable the following implementation when xb1 can compile it.
  // FlushInstructionCache(GetCurrentProcess(), virtual_address, size_bytes);
}
