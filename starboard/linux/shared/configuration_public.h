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

// The Starboard configuration for Desktop Linux. Other devices will have
// specific Starboard implementations, even if they ultimately are running some
// version of Linux -- but they may base their configuration on the Desktop
// Linux configuration headers.

// Other source files should never include this header directly, but should
// include the generic "starboard/configuration.h" instead.

#ifndef STARBOARD_LINUX_SHARED_CONFIGURATION_PUBLIC_H_
#define STARBOARD_LINUX_SHARED_CONFIGURATION_PUBLIC_H_

// --- System Header Configuration -------------------------------------------

// Whether the current platform provides the standard header sys/types.h.
#define SB_HAS_SYS_TYPES_H 1

// Type detection for wchar_t.
#if defined(__WCHAR_MAX__) && \
    (__WCHAR_MAX__ == 0x7fffffff || __WCHAR_MAX__ == 0xffffffff)
#define SB_IS_WCHAR_T_UTF32 1
#elif defined(__WCHAR_MAX__) && \
    (__WCHAR_MAX__ == 0x7fff || __WCHAR_MAX__ == 0xffff)
#define SB_IS_WCHAR_T_UTF16 1
#endif

// Chrome only defines this for ARMEL.
#if defined(__ARMEL__)
// Chrome has an exclusion for iOS here, we should too when we support iOS.
#define SB_IS_WCHAR_T_UNSIGNED 1
#endif

// --- Media Configuration ---------------------------------------------------

// The path of video_dmp_writer.h. Defined here to avoid errors building on
// platforms that do not include the video_dmp library.
#define SB_PLAYER_DMP_WRITER_INCLUDE_PATH \
  "starboard/shared/starboard/player/video_dmp_writer.h"

#endif  // STARBOARD_LINUX_SHARED_CONFIGURATION_PUBLIC_H_
