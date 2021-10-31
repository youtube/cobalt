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

#if SB_API_VERSION >= 12

// Determines the threshold of allocation size that should be done with mmap
// (if available), rather than allocated within the core heap.
extern const size_t kSbDefaultMmapThreshold;

// The current platform's maximum length of the name of a single directory
// entry, not including the absolute path.
extern const int32_t kSbFileMaxName;

// The current platform's maximum number of files that can be opened at the
// same time by one process.
extern const uint32_t kSbFileMaxOpen;

// The current platform's alternate file path component separator character.
// This is like SB_FILE_SEP_CHAR, except if your platform supports an alternate
// character, then you can place that here. For example, on windows machines,
// the primary separator character is probably '\', but the alternate is '/'.
extern const char kSbFileAltSepChar;

// The string form of SB_FILE_ALT_SEP_CHAR.
extern const char* kSbFileAltSepString;

// The current platform's maximum length of an absolute path.
extern const uint32_t kSbFileMaxPath;

// The current platform's file path component separator character. This is the
// character that appears after a directory in a file path. For example, the
// absolute canonical path of the file "/path/to/a/file.txt" uses '/' as a path
// component separator character.
extern const char kSbFileSepChar;

// The string form of SB_FILE_SEP_CHAR.
extern const char* kSbFileSepString;

// Allow ac3 and ec3 support
extern const bool kSbHasAc3Audio;

// Specifies whether this platform has webm/vp9 support.  This should be set to
// non-zero on platforms with webm/vp9 support.
extern const bool kSbHasMediaWebmVp9Support;

// Whether the current platform supports thread priorities.
extern const bool kSbHasThreadPrioritySupport;

// Determines the alignment that allocations should have on this platform.
extern const size_t kSbMallocAlignment;

// The maximum number of thread local storage keys supported by this platform.
// This comes from _POSIX_THREAD_KEYS_MAX. The value of PTHREAD_KEYS_MAX is
// higher, but unit tests show that the implementation doesn't support nearly
// as many keys.
extern const uint32_t kSbMaxThreadLocalKeys;

// The maximum length of the name for a thread, including the NULL-terminator.
extern const int32_t kSbMaxThreadNameLength;

// Defines the path where memory debugging logs should be written to.
extern const char* kSbMemoryLogPath;

// The maximum audio bitrate the platform can decode.  The following value
// equals to 5M bytes per seconds which is more than enough for compressed
// audio.
extern const uint32_t kSbMediaMaxAudioBitrateInBitsPerSecond;

// The maximum video bitrate the platform can decode.  The following value
// equals to 8M bytes per seconds which is more than enough for compressed
// video.
extern const uint32_t kSbMediaMaxVideoBitrateInBitsPerSecond;

// Specifies how video frame buffers must be aligned on this platform.
extern const uint32_t kSbMediaVideoFrameAlignment;

// The memory page size, which controls the size of chunks on memory that
// allocators deal with, and the alignment of those chunks. This doesn't have to
// be the hardware-defined physical page size, but it should be a multiple of
// it.
extern const size_t kSbMemoryPageSize;

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
extern const uint32_t kSbNetworkReceiveBufferSize;

// Defines the maximum number of simultaneous threads for this platform. Some
// platforms require sharing thread handles with other kinds of system handles,
// like mutexes, so we want to keep this manageable.
extern const uint32_t kSbMaxThreads;

// Specifies the preferred byte order of color channels in a pixel. Refer to
// starboard/configuration.h for the possible values. EGL/GLES platforms should
// generally prefer a byte order of RGBA, regardless of endianness.
extern const int kSbPreferredRgbaByteOrder;

// The current platform's search path component separator character. When
// specifying an ordered list of absolute paths of directories to search for a
// given reason, this is the character that appears between entries. For
// example, the search path of "/etc/search/first:/etc/search/second" uses ':'
// as a search path component separator character.
#ifdef __cplusplus
extern "C" {
#endif
extern const char kSbPathSepChar;
#ifdef __cplusplus
}  // extern "C"
#endif

// The string form of SB_PATH_SEP_CHAR.
extern const char* kSbPathSepString;

// The maximum number of users that can be signed in at the same time.
extern const uint32_t kSbUserMaxSignedIn;

#endif  // SB_API_VERSION >= 12

#endif  // STARBOARD_CONFIGURATION_CONSTANTS_H_
