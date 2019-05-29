// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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
#include "starboard/types.h"

// This workaround is for protoc auto-generated files which use memset.
#if SB_HAS_QUIRK(MEMSET_IN_SYSTEM_HEADERS)
  namespace std {
    inline namespace _LIBCPP_NAMESPACE {
      inline void *SbMemorySet(void* destination, int byte_value,
                               size_t count) {
        return ::SbMemorySet(destination, byte_value, count);
      }
    }
  }
#else
#ifndef memset
#define memset SbMemorySet
#endif  // memset
#endif  // SB_HAS_QUIRK(MEMSET_IN_SYSTEM_HEADERS)
