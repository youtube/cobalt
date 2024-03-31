// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

// Other source files should never include this header directly, but should
// include the generic "starboard/configuration.h" instead.

#ifndef STARBOARD_WIN_SHARED_CONFIGURATION_PUBLIC_H_
#define STARBOARD_WIN_SHARED_CONFIGURATION_PUBLIC_H_

// --- System Header Configuration -------------------------------------------

// Any system headers listed here that are not provided by the platform will be
// emulated in starboard/types.h.

// Whether the current platform provides the standard header sys/types.h.
#define SB_HAS_SYS_TYPES_H 0

// Whether the current platform provides ssize_t.
#define SB_HAS_SSIZE_T 0

#if !defined(__WCHAR_MAX__)
#include <wchar.h>
#define __WCHAR_MAX__ WCHAR_MAX
#endif

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

// --- Compiler Configuration ------------------------------------------------

// The platform's annotation for forcing a C function to be inlined.
//   https://msdn.microsoft.com/en-us/library/bw1hbe6y.aspx#Anchor_1
#define SB_C_FORCE_INLINE __forceinline

// The platform's annotation for marking a C function as forcibly not
// inlined.
#define SB_C_NOINLINE __declspec(noinline)

// The platform's annotation for marking a symbol as exported outside of the
// current shared library.
#define SB_EXPORT_PLATFORM __declspec(dllexport)

// The platform's annotation for marking a symbol as imported from outside of
// the current linking unit.
#define SB_IMPORT_PLATFORM

// --- I/O Configuration -----------------------------------------------------

// Whether the current platform supports on screen keyboard.
#define SB_HAS_ON_SCREEN_KEYBOARD 0

// --- Memory Configuration --------------------------------------------------

// Whether this platform can map executable memory. Implies SB_HAS_MMAP. This is
// required for platforms that want to JIT.
#define SB_CAN_MAP_EXECUTABLE_MEMORY 1

// --- Network Configuration -------------------------------------------------

// Specifies whether this platform supports IPV6.
#define SB_HAS_IPV6 1

// Specifies whether this platform supports pipe.
#define SB_HAS_PIPE 1

#endif  // STARBOARD_WIN_SHARED_CONFIGURATION_PUBLIC_H_
