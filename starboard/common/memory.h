// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include <stdlib.h>
#include <string.h>

#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"

#ifndef STARBOARD_COMMON_MEMORY_H_
#define STARBOARD_COMMON_MEMORY_H_

namespace starboard {

// Checks whether |memory| is aligned to |alignment| bytes.
static SB_C_FORCE_INLINE bool MemoryIsAligned(const void* memory,
                                              size_t alignment) {
  return ((uintptr_t)memory) % alignment == 0;
}

// Rounds |size| up to kSbMemoryPageSize.
static SB_C_FORCE_INLINE size_t MemoryAlignToPageSize(size_t size) {
  return (size + kSbMemoryPageSize - 1) & ~(kSbMemoryPageSize - 1);
}

// Returns true if the first |count| bytes of |buffer| are set to zero.
static SB_C_FORCE_INLINE bool MemoryIsZero(const void* buffer, size_t count) {
  if (count == 0) {
    return true;
  }
  const char* char_buffer = (const char*)(buffer);
  return char_buffer[0] == 0 &&
         memcmp(char_buffer, char_buffer + 1, count - 1) == 0;
}

// Aliases not to break CI tests.
// See https://paste.googleplex.com/4527409416241152
// TODO: b/441955897 - Update CI test to use flattened namespace
namespace common {
using ::starboard::MemoryIsZero;
}

}  // namespace starboard

#endif  // STARBOARD_COMMON_MEMORY_H_
