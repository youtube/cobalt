// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//
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

// Determines the threshhold of allocation size that should be done with mmap
// (if available), rather than allocated within the core heap.
const size_t kSbDefaultMmapThreshold = 256 * 1024U;

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

// Allow ac3 and ec3 support
const bool kSbHasAc3Audio = true;

// Specifies whether this platform has webm/vp9 support.  This should be set to
// non-zero on platforms with webm/vp9 support.
const bool kSbHasMediaWebmVp9Support = true;

// Determines the alignment that allocations should have on this platform.
const size_t kSbMallocAlignment = 16;

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

// Specify the number of video frames to be cached during playback.  A large
// value leads to more stable fps but also causes the app to use more memory.
const uint32_t kSbMediaMaximumVideoFrames = 12;

// The encoded video frames are compressed in different ways, their decoding
// time can vary a lot.  Occasionally a single frame can take longer time to
// decode than the average time per frame.  The player has to cache some frames
// to account for such inconsistency.  The number of frames being cached are
// controlled by the following two macros.
//
// Specify the number of video frames to be cached before the playback starts.
// Note that set this value too large may increase the playback start delay.
const uint32_t kSbMediaMaximumVideoPrerollFrames = 4;

// Specifies how video frame buffers must be aligned on this platform.
const uint32_t kSbMediaVideoFrameAlignment = 256;

// The memory page size, which controls the size of chunks on memory that
// allocators deal with, and the alignment of those chunks. This doesn't have to
// be the hardware-defined physical page size, but it should be a multiple of
// it.
const size_t kSbMemoryPageSize = 4096;

// Defines the maximum number of simultaneous threads for this platform. Some
// platforms require sharing thread handles with other kinds of system handles,
// like mutexes, so we want to keep this managable.
const uint32_t kSbMaxThreads = 90;

#if SB_API_VERSION < 16
// The maximum number of users that can be signed in at the same time.
const uint32_t kSbUserMaxSignedIn = 1;
#endif  // SB_API_VERSION < 16

// Defines maximum space in bytes the cache directory kSbSystemPathCacheDirectory can
// use. The default value is 24MiB.
const uint32_t kSbMaxSystemPathCacheDirectorySize = 24 << 20;  // 24MiB

#if SB_API_VERSION >= 16
SB_EXPORT extern const bool kSbCanMapExecutableMemory = true;
#endif
