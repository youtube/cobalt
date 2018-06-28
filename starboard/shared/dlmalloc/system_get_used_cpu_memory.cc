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

#include "starboard/shared/dlmalloc/page_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

// Returns the number of bytes obtained from the system.  The total
// number of bytes allocated by malloc, realloc etc., is less than this
// value. Unlike mallinfo, this function returns only a precomputed
// result, so can be called frequently to monitor memory consumption.
// Even if locks are otherwise defined, this function does not use them,
// so results might not be up to date.
//
// See http://gee.cs.oswego.edu/pub/misc/malloc.h for more details.
size_t SB_ALLOCATOR(malloc_footprint)();

#ifdef __cplusplus
}  // extern "C"
#endif

int64_t SbSystemGetUsedCPUMemory() {
  return static_cast<int64_t>(SB_ALLOCATOR(malloc_footprint)());
}
