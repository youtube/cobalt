// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: Starboard Memory module
//
// Defines functions for memory allocation, alignment, copying, and comparing.
//
// # Porters
//
// All of the "Unchecked" and "Free" functions must be implemented, but they
// should not be called directly. The Starboard platform wraps them with extra
// accounting under certain circumstances.
//
// # Porters and Application Developers
//
// Nobody should call the "Checked", "Unchecked" or "Free" functions directly
// because that evades Starboard's memory tracking. In both port
// implementations and Starboard client application code, you should always
// call SbMemoryAllocate and SbMemoryDeallocate rather than
// SbMemoryAllocateUnchecked and SbMemoryFree.
//
// - The "checked" functions are SbMemoryAllocateChecked(),
//   SbMemoryReallocateChecked(), and SbMemoryAllocateAlignedChecked().
// - The "unchecked" functions are SbMemoryAllocateUnchecked(),
//   SbMemoryReallocateUnchecked(), and SbMemoryAllocateAlignedUnchecked().
// - The "free" functions are SbMemoryFree() and SbMemoryFreeAligned().

#ifndef STARBOARD_MEMORY_H_
#define STARBOARD_MEMORY_H_

#include <string.h>
#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#include "starboard/export.h"
#include "starboard/system.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// TODO: Remove the definition once the memory_mapped_file.h extension
// is deprecated.
// The bitwise OR of these flags should be passed to SbMemoryMap to indicate
// how the mapped memory can be used.
typedef enum SbMemoryMapFlags {
  // No flags set: Reserves virtual address space. SbMemoryProtect() can later
  // make it accessible.
  kSbMemoryMapProtectReserved = 0,
  kSbMemoryMapProtectRead = 1 << 0,   // Mapped memory can be read.
  kSbMemoryMapProtectWrite = 1 << 1,  // Mapped memory can be written to.
  kSbMemoryMapProtectExec = 1 << 2,   // Mapped memory can be executed.
  kSbMemoryMapProtectReadWrite =
      kSbMemoryMapProtectRead | kSbMemoryMapProtectWrite,
} SbMemoryMapFlags;

#ifdef __cplusplus
}  // extern "C"
#endif
#endif  // STARBOARD_MEMORY_H_
