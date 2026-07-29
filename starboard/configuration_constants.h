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

#include <stddef.h>
#include <stdint.h>

#include "starboard/export.h"

// The current platform's maximum length of the name of a single directory
// entry, not including the absolute path.
SB_EXPORT extern const int32_t kSbFileMaxName;

// The current platform's maximum length of an absolute path.
SB_EXPORT extern const uint32_t kSbFileMaxPath;

// The current platform's file path component separator character. This is the
// character that appears after a directory in a file path. For example, the
// absolute canonical path of the file "/path/to/a/file.txt" uses '/' as a path
// component separator character.
SB_EXPORT extern const char kSbFileSepChar;

// The string form of SB_FILE_SEP_CHAR.
SB_EXPORT extern const char* kSbFileSepString;

// The maximum audio bitrate the platform can decode.  The following value
// equals to 5M bytes per seconds which is more than enough for compressed
// audio.
SB_EXPORT extern const uint32_t kSbMediaMaxAudioBitrateInBitsPerSecond;

// The maximum video bitrate the platform can decode.  The following value
// equals to 8M bytes per seconds which is more than enough for compressed
// video.
SB_EXPORT extern const uint32_t kSbMediaMaxVideoBitrateInBitsPerSecond;

// The memory page size, which controls the size of chunks on memory that
// allocators deal with, and the alignment of those chunks. This doesn't have to
// be the hardware-defined physical page size, but it should be a multiple of
// it.
SB_EXPORT extern const size_t kSbMemoryPageSize;

// Defines the maximum number of simultaneous threads for this platform. Some
// platforms require sharing thread handles with other kinds of system handles,
// like mutexes, so we want to keep this manageable.
SB_EXPORT extern const uint32_t kSbMaxThreads;

// The maximum size the cache directory is allowed to use in bytes.
SB_EXPORT extern const uint32_t kSbMaxSystemPathCacheDirectorySize;

// Whether this platform can map executable memory. This is required for
// platforms that want to JIT.
SB_EXPORT extern const bool kSbCanMapExecutableMemory;

#endif  // STARBOARD_CONFIGURATION_CONSTANTS_H_
