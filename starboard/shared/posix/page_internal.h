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

#ifndef STARBOARD_SHARED_POSIX_PAGE_INTERNAL_H_
#define STARBOARD_SHARED_POSIX_PAGE_INTERNAL_H_

#include <stddef.h>
#include <stdint.h>

#include "starboard/extension/memory_mapped_file.h"
#include "starboard/shared/internal_only.h"

#ifdef __cplusplus
extern "C" {
#endif

// Memory maps a file to the specified |addr| starting with |file_offset| and
// mapping |size| bytes. The |addr| should be reserved before calling. If
// NULL |addr| is passed a new memory block would be allocated and the address
// returned. The file_offset must be a multiple of |kSbMemoryPageSize|.
// On error returns NULL.
void* SbPageMapFile(void* addr,
                    const char* path,
                    SbMemoryMapFlags flags,
                    int64_t file_offset,
                    int64_t size);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_POSIX_PAGE_INTERNAL_H_
