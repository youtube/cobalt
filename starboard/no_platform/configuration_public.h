// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

// The Starboard configuration for a stub implementation designed to be
// built on Desktop Linux. Other devices will have real Starboard
// implementations.

// Other source files should never include this header directly, but should
// include the generic "starboard/configuration.h" instead.

#ifndef STARBOARD_NO_PLATFORM_CONFIGURATION_PUBLIC_H_
#define STARBOARD_NO_PLATFORM_CONFIGURATION_PUBLIC_H_

// --- System Header Configuration -------------------------------------------
// This platform should not have further defines, as it should not be built.

// Define that satisfies //starboard/configuration.h
// Type detection for wchar_t.
#if defined(__WCHAR_MAX__) && \
    (__WCHAR_MAX__ == 0x7fffffff || __WCHAR_MAX__ == 0xffffffff)
#define SB_IS_WCHAR_T_UTF32 1
#elif defined(__WCHAR_MAX__) && \
    (__WCHAR_MAX__ == 0x7fff || __WCHAR_MAX__ == 0xffff)
#define SB_IS_WCHAR_T_UTF16 1
#endif

#endif  // STARBOARD_NO_PLATFORM_CONFIGURATION_PUBLIC_H_
