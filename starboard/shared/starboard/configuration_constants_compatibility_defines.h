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

#if SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION

#error \
    "This file is only relevant for Starboard versions before 12. Please do " \
"not include this file otherwise."

#else  // SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION

#define kSbDefaultMmapThreshold SB_DEFAULT_MMAP_THRESHOLD

#define kSbMallocAlignment SB_MALLOC_ALIGNMENT

#define kSbMaxThreadNameLength SB_MAX_THREAD_NAME_LENGTH

#define kSbMediaMaxAudioBitrateInBitsPerSecond \
  SB_MEDIA_MAX_AUDIO_BITRATE_IN_BITS_PER_SECOND

#define kSbMediaMaxVideoBitrateInBitsPerSecond \
  SB_MEDIA_MAX_VIDEO_BITRATE_IN_BITS_PER_SECOND

#define kSbMediaVideoFrameAlignment SB_MEDIA_VIDEO_FRAME_ALIGNMENT

#define kSbMemoryLogPath SB_MEMORY_LOG_PATH

#define kSbMemoryPageSize SB_MEMORY_PAGE_SIZE

#endif  // SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION

#endif  // STARBOARD_SHARED_STARBOARD_CONFIGURATION_CONSTANTS_COMPATIBILITY_DEFINES_H_
