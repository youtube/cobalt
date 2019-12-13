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

// This file defines all configuration constants for a platform.

#include "starboard/configuration_constants.h"

#if SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION

// Determines the threshhold of allocation size that should be done with mmap
// (if available), rather than allocated within the core heap.
const size_t kSbDefaultMmapThreshold = 256 * 1024U;

// Determines the alignment that allocations should have on this platform.
const size_t kSbMallocAlignment = 16;

// The maximum length of a name for a thread, including the NULL-terminator.
const int32_t kSbMaxThreadNameLength = 16;

// Defines the path where memory debugging logs should be written to.
const char* kSbMemoryLogPath = "/tmp/starboard";

// The maximum audio bitrate the platform can decode.  The following value
// equals to 5M bytes per seconds which is more than enough for compressed
// audio.
const uint32_t kSbMediaMaxAudioBitrateInBitsPerSecond = 40 * 1024 * 1024;

// The maximum video bitrate the platform can decode.  The following value
// equals to 25M bytes per seconds which is more than enough for compressed
// video.
const uint32_t kSbMediaMaxVideoBitrateInBitsPerSecond = 200 * 1024 * 1024;

// The memory page size, which controls the size of chunks on memory that
// allocators deal with, and the alignment of those chunks. This doesn't have to
// be the hardware-defined physical page size, but it should be a multiple of
// it.
const size_t kSbMemoryPageSize = 4096;

#endif  // SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION
