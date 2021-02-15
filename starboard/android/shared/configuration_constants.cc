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

// The current platform's maximum number of files that can be opened at the
// same time by one process.
const uint32_t kSbFileMaxOpen = 256;

// The current platform's alternate file path component separator character.
// This is like SB_FILE_SEP_CHAR, except if your platform supports an alternate
// character, then you can place that here. For example, on windows machines,
// the primary separator character is probably '\', but the alternate is '/'.
const char kSbFileAltSepChar = '/';

// The string form of SB_FILE_ALT_SEP_CHAR.
const char* kSbFileAltSepString = "/";

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

// Whether the current platform supports thread priorities.
const bool kSbHasThreadPrioritySupport = true;

// Determines the alignment that allocations should have on this platform.
const size_t kSbMallocAlignment = 16;

// The maximum number of thread local storage keys supported by this platform.
// This comes from bionic PTHREAD_KEYS_MAX in limits.h, which we've decided
// to not include here to decrease symbol pollution.
const uint32_t kSbMaxThreadLocalKeys = 128;

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

// Specifies how video frame buffers must be aligned on this platform.
const uint32_t kSbMediaVideoFrameAlignment = 256;

// The memory page size, which controls the size of chunks on memory that
// allocators deal with, and the alignment of those chunks. This doesn't have to
// be the hardware-defined physical page size, but it should be a multiple of
// it.
const size_t kSbMemoryPageSize = 4096;

// Specifies the network receive buffer size in bytes, set via
// SbSocketSetReceiveBufferSize().
//
// Setting this to 0 indicates that SbSocketSetReceiveBufferSize() should
// not be called. Use this for OSs (such as Linux) where receive buffer
// auto-tuning is better.
//
// On some platforms, this may affect max TCP window size which may
// dramatically affect throughput in the presence of latency.
//
// If your platform does not have a good TCP auto-tuning mechanism,
// a setting of (128 * 1024) here is recommended.
const uint32_t kSbNetworkReceiveBufferSize = 0;

// Defines the maximum number of simultaneous threads for this platform. Some
// platforms require sharing thread handles with other kinds of system handles,
// like mutexes, so we want to keep this manageable.
const uint32_t kSbMaxThreads = 90;

// The current platform's search path component separator character. When
// specifying an ordered list of absolute paths of directories to search for a
// given reason, this is the character that appears between entries. For
// example, the search path of "/etc/search/first:/etc/search/second" uses ':'
// as a search path component separator character.
const char kSbPathSepChar = ':';

// The string form of SB_PATH_SEP_CHAR.
const char* kSbPathSepString = ":";

// Specifies the preferred byte order of color channels in a pixel. Refer to
// starboard/configuration.h for the possible values. EGL/GLES platforms should
// generally prefer a byte order of RGBA, regardless of endianness.
const int kSbPreferredRgbaByteOrder = SB_PREFERRED_RGBA_BYTE_ORDER_RGBA;

// The maximum number of users that can be signed in at the same time.
const uint32_t kSbUserMaxSignedIn = 1;
