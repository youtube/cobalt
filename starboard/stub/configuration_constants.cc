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

// The current platform's maximum length of the name of a single directory
// entry, not including the absolute path.
const int32_t kSbFileMaxName = 64;

// The current platform's maximum length of an absolute path.
const uint32_t kSbFileMaxPath = 4096;

// The current platform's file path component separator character. This is the
// character that appears after a directory in a file path. For example, the
// absolute canonical path of the file "/path/to/a/file.txt" uses '/' as a path
// component separator character.
const char kSbFileSepChar = '/';

// The string form of SB_FILE_SEP_CHAR.
const char* kSbFileSepString = "/";

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

// Defines the maximum number of simultaneous threads for this platform. Some
// platforms require sharing thread handles with other kinds of system handles,
// like mutexes, so we want to keep this manageable.
const uint32_t kSbMaxThreads = 90;

// The maximum size the cache directory is allowed to use in bytes.
const uint32_t kSbMaxSystemPathCacheDirectorySize = 24 << 20;  // 24MiB

// Whether this platform can map executable memory. This is required for
// platforms that want to JIT.
SB_EXPORT extern const bool kSbCanMapExecutableMemory = false;
