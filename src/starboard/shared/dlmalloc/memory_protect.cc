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

#include "starboard/configuration.h"
#include "starboard/memory.h"
#include "starboard/shared/dlmalloc/page_internal.h"

#if SB_API_VERSION >= SB_MEMORY_PROTECT_API_VERSION
bool SbMemoryProtect(void* virtual_address, int64_t size_bytes, int flags) {
  return SbPageProtect(virtual_address, size_bytes, flags);
}
#endif
