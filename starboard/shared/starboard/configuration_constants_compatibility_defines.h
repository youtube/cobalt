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

// This file defines macros to provide convenience for backwards compatibility
// after the change that migrated certain configuration macros to extern
// variables.

#ifndef STARBOARD_SHARED_STARBOARD_CONFIGURATION_CONSTANTS_COMPATIBILITY_DEFINES_H_
#define STARBOARD_SHARED_STARBOARD_CONFIGURATION_CONSTANTS_COMPATIBILITY_DEFINES_H_

#if SB_API_VERSION >= 12

#error \
    "This file is only relevant for Starboard versions before 12. Please do " \
"not include this file otherwise."

#else  // SB_API_VERSION >= 12

#define kSbDefaultMmapThreshold SB_DEFAULT_MMAP_THRESHOLD

#define kSbFileMaxName SB_FILE_MAX_NAME

#define kSbFileMaxOpen SB_FILE_MAX_OPEN

#define kSbFileAltSepChar SB_FILE_ALT_SEP_CHAR

#define kSbFileAltSepString SB_FILE_ALT_SEP_STRING

#define kSbFileMaxPath SB_FILE_MAX_PATH

#define kSbFileSepChar SB_FILE_SEP_CHAR

#define kSbFileSepString SB_FILE_SEP_STRING

#define kSbHasAc3Audio SB_HAS_AC3_AUDIO

#define kSbHasMediaWebmVp9Support SB_HAS_MEDIA_WEBM_VP9_SUPPORT

#define kSbHasThreadPrioritySupport SB_HAS_THREAD_PRIORITY_SUPPORT

#define kSbMallocAlignment SB_MALLOC_ALIGNMENT

#define kSbMaxThreadLocalKeys SB_MAX_THREAD_LOCAL_KEYS

#define kSbMaxThreadNameLength SB_MAX_THREAD_NAME_LENGTH

#define kSbMediaMaxAudioBitrateInBitsPerSecond \
  SB_MEDIA_MAX_AUDIO_BITRATE_IN_BITS_PER_SECOND

#define kSbMediaMaxVideoBitrateInBitsPerSecond \
  SB_MEDIA_MAX_VIDEO_BITRATE_IN_BITS_PER_SECOND

#define kSbMediaVideoFrameAlignment SB_MEDIA_VIDEO_FRAME_ALIGNMENT

#define kSbMemoryLogPath SB_MEMORY_LOG_PATH

#define kSbMemoryPageSize SB_MEMORY_PAGE_SIZE

#define kSbNetworkReceiveBufferSize SB_NETWORK_RECEIVE_BUFFER_SIZE

#define kSbMaxThreads SB_MAX_THREADS

#define kSbPathSepChar SB_PATH_SEP_CHAR

#define kSbPathSepString SB_PATH_SEP_STRING

#define kSbPreferredRgbaByteOrder SB_PREFERRED_RGBA_BYTE_ORDER

#define kSbUserMaxSignedIn SB_USER_MAX_SIGNED_IN

#endif  // SB_API_VERSION >= 12

#endif  // STARBOARD_SHARED_STARBOARD_CONFIGURATION_CONSTANTS_COMPATIBILITY_DEFINES_H_
