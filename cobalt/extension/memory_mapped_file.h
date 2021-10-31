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


#ifndef COBALT_EXTENSION_MEMORY_MAPPED_FILE_H_
#define COBALT_EXTENSION_MEMORY_MAPPED_FILE_H_

#include <stdint.h>

#include "starboard/memory.h"

#ifdef __cplusplus
extern "C" {
#endif


#define kCobaltExtensionMemoryMappedFileName \
  "dev.cobalt.extension.MemoryMappedFile"

typedef struct CobaltExtensionMemoryMappedFileApi {
  // Name should be the string |kCobaltExtensionMemoryMappedFileName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // Memory maps a file at the specified |address| starting at |file_offset|
  // and  mapping |size| bytes. The |address| argument can be NULL in which
  // case new memory buffer will be allocated. If a non NULL |address| is
  // passed the memory should be resreved in advance through |SbMemoryMap|.
  // To release the memory call |SbMemoryUnmap|.
  // The |file_offset| must be a multiple of |kSbMemoryPageSize|.
  // Returns NULL or error.
  void* (*MemoryMapFile)(void* address, const char* path,
                         SbMemoryMapFlags flags, int64_t file_offset,
                         int64_t size);

} CobaltExtensionMemoryMappedFileApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // COBALT_EXTENSION_MEMORY_MAPPED_FILE_H_
