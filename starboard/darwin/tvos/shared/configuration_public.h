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

// This header provides shared settings across all architectures supported
// on tvOS. Architecture-specific headers include this one.

// Other source files should never include this header directly, but should
// include the generic "starboard/configuration.h" instead.

#ifndef STARBOARD_DARWIN_TVOS_SHARED_CONFIGURATION_PUBLIC_H_
#define STARBOARD_DARWIN_TVOS_SHARED_CONFIGURATION_PUBLIC_H_

// --- System Header Configuration -------------------------------------------

#define DEPRECATED_SCOPED_PTR

// Any system headers listed here that are not provided by the platform will be
// emulated in starboard/types.h.

// Whether the current platform provides ssize_t.
#define SB_HAS_SSIZE_T 1

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

// Chrome only defines this if ARMEL is defined.
// https://developer.apple.com/library/content/documentation/Xcode/Conceptual/iPhoneOSABIReference/Articles/ARM64FunctionCallingConventions.html#//apple_ref/doc/uid/TP40013702-SW1
// "In iOS, as with other Darwin platforms, both char and wchar_t are signed
// types."
#if defined(__ARMEL__)
#define SB_IS_WCHAR_T_SIGNED 1
#endif

// --- Compiler Configuration ------------------------------------------------

// The platform's annotation for marking a symbol as exported outside of the
// current shared library.
#define SB_EXPORT_PLATFORM __attribute__((visibility("default")))

// The platform's annotation for marking a symbol as imported from outside of
// the current linking unit.
#define SB_IMPORT_PLATFORM

// --- Media Configuration ---------------------------------------------------

// tvOS uses AVPlayer to play back HLS streams, and no video decoding is done
// in javascript or otherwise.
#define SB_HAS_PLAYER_WITH_URL 1

// The path of url_player.h. Use this macro in public interfaces to avoid
// exposing it.
#define SB_URL_PLAYER_INCLUDE_PATH "starboard/shared/uikit/url_player.h"

// --- Network Configuration -------------------------------------------------

// Specifies whether this platform supports IPV6.
#define SB_HAS_IPV6 1

// --- Platform Specific Quirks ----------------------------------------------

// The Darwin platform doesn't have the function pthread_condattr_setclock(),
// which is used to switch pthread condition variable timed waits to use
// a monotonic clock instead of the system clock, which is sensitive to
// system time adjustments.  This isn't a [huge] problem on Darwin because
// its timed wait function immediately converts to relative wait time anyway:
//   https://opensource.apple.com/source/Libc/Libc-167/pthreads.subproj/pthread_cond.c
#define SB_HAS_QUIRK_NO_CONDATTR_SETCLOCK_SUPPORT 1

// This quirk is used to switch the headers included in
// starboard/shared/linux/socket_get_interface_address.cc for darwin system
// headers. It may be removed at some point in favor of a different solution.
#define SB_HAS_QUIRK_SOCKET_BSD_HEADERS 1

// This quirk is used to disable thread affinity support since Darwin platforms
// do not support the CPU_* functions referenced in
// starboard/shared/pthread/thread_create.cc.
#define SB_HAS_QUIRK_THREAD_AFFINITY_UNSUPPORTED 1

// Quirk for sending on connected UDP sockets.
// When defined (set to 1), this macro allows calling SbSocketSendTo
// with a nullptr as the destination address for connected UDP sockets.
// In such cases, the underlying implementation will use the pre-established
// remote address, simplifying the send operation on supported platforms.
#define SB_HAS_QUIRK_SENDING_CONNECTED_UDP_SOCKETS 1

#define LIBEVENT_PLATFORM_HEADER \
  "starboard/shared/uikit/libevent/libevent-starboard.h"

#endif  // STARBOARD_DARWIN_TVOS_SHARED_CONFIGURATION_PUBLIC_H_
