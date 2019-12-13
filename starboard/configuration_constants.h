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

// Module Overview: Starboard Configuration Variables module
//
// Declares all configuration variables we will need to use at runtime.
// These variables describe the current platform in detail to allow cobalt to
// make runtime decisions based on per platform configurations.

#ifndef STARBOARD_CONFIGURATION_CONSTANTS_H_
#define STARBOARD_CONFIGURATION_CONSTANTS_H_

#include "starboard/types.h"

#if SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION

// Determines the threshhold of allocation size that should be done with mmap
// (if available), rather than allocated within the core heap.
extern const size_t kSbDefaultMmapThreshold;

// Determines the alignment that allocations should have on this platform.
extern const size_t kSbMallocAlignment;

// The maximum length of the name for a thread, including the NULL-terminator.
extern const int32_t kSbMaxThreadNameLength;

// Defines the path where memory debugging logs should be written to.
extern const char* kSbMemoryLogPath;

// The memory page size, which controls the size of chunks on memory that
// allocators deal with, and the alignment of those chunks. This doesn't have to
// be the hardware-defined physical page size, but it should be a multiple of
// it.
extern const size_t kSbMemoryPageSize;

#endif  // SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION

#endif  // STARBOARD_CONFIGURATION_CONSTANTS_H_
