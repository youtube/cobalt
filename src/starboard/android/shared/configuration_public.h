// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

// The Starboard configuration for generic Android. Other devices will have
// specific Starboard implementations, even if they ultimately are running some
// version of Android -- but they may base their configuration on the generic
// Android configuration headers.

// Other source files should never include this header directly, but should
// include the generic "starboard/configuration.h" instead.

#ifndef STARBOARD_ANDROID_SHARED_CONFIGURATION_PUBLIC_H_
#define STARBOARD_ANDROID_SHARED_CONFIGURATION_PUBLIC_H_

#if SB_API_VERSION != SB_EXPERIMENTAL_API_VERSION
#error \
    "This platform's sabi.json file is expected to track the experimental " \
"Starboard API version."
#endif  // SB_API_VERSION != SB_EXPERIMENTAL_API_VERSION

// --- Architecture Configuration --------------------------------------------

// Configuration parameters that allow the application to make some general
// compile-time decisions with respect to the the number of cores likely to be
// available on this platform. For a definitive measure, the application should
// still call SbSystemGetNumberOfProcessors at runtime.

// Whether the current platform's thread scheduler will automatically balance
// threads between cores, as opposed to systems where threads will only ever run
// on the specifically pinned core.
#define SB_HAS_CROSS_CORE_SCHEDULER 1

// Indicates that there is no support for alignment at greater than 16 bytes for
// items on the stack.
#define SB_HAS_QUIRK_DOES_NOT_STACK_ALIGN_OVER_16_BYTES 1

// This quirk is used to fix an issue caused by the rewriting of memset to
// SbMemorySet in third_party/protobuf/src/google/protobuf/stubs/port.h.
#define SB_HAS_QUIRK_MEMSET_IN_SYSTEM_HEADERS 1

// TODO: Pending refactor to remove the dependency on seek to keyframe.
#define SB_HAS_QUIRK_SEEK_TO_KEYFRAME 1

// --- System Header Configuration -------------------------------------------

// Any system headers listed here that are not provided by the platform will be
// emulated in starboard/types.h.

// Whether the current platform provides the standard header stdarg.h.
#define SB_HAS_STDARG_H 1

// Whether the current platform provides the standard header stdbool.h.
#define SB_HAS_STDBOOL_H 1

// Whether the current platform provides the standard header stddef.h.
#define SB_HAS_STDDEF_H 1

// Whether the current platform provides the standard header stdint.h.
#define SB_HAS_STDINT_H 1

// Whether the current platform provides the standard header inttypes.h.
#define SB_HAS_INTTYPES_H 1

// Whether the current platform provides the standard header limits.h.
#define SB_HAS_LIMITS_H 1

// Whether the current platform provides the standard header float.h.
#define SB_HAS_FLOAT_H 1

// Whether the current platform provides ssize_t.
#define SB_HAS_SSIZE_T 1

// Type detection for wchar_t.
#if defined(__WCHAR_MAX__) && \
    (__WCHAR_MAX__ == 0x7fffffff || __WCHAR_MAX__ == 0xffffffff)
#define SB_IS_WCHAR_T_UTF32 1
#elif defined(__WCHAR_MAX__) && \
    (__WCHAR_MAX__ == 0x7fff || __WCHAR_MAX__ == 0xffff)
#define SB_IS_WCHAR_T_UTF16 1
#endif

// Chrome only defines these two if ARMEL or MIPSEL are defined.
#if defined(__ARMEL__)
// Chrome has an exclusion for iOS here, we should too when we support iOS.
#define SB_IS_WCHAR_T_UNSIGNED 1
#elif defined(__MIPSEL__)
#define SB_IS_WCHAR_T_SIGNED 1
#endif

// --- Compiler Configuration ------------------------------------------------

// The platform's annotation for forcing a C function to be inlined.
#define SB_C_FORCE_INLINE __inline__ __attribute__((always_inline))

// The platform's annotation for marking a C function as suggested to be
// inlined.
#define SB_C_INLINE inline

// The platform's annotation for marking a C function as forcibly not
// inlined.
#define SB_C_NOINLINE __attribute__((noinline))

// The platform's annotation for marking a symbol as exported outside of the
// current shared library.
#define SB_EXPORT_PLATFORM __attribute__((visibility("default")))

// The platform's annotation for marking a symbol as imported from outside of
// the current linking unit.
#define SB_IMPORT_PLATFORM

// --- Extensions Configuration ----------------------------------------------

// Do not use <unordered_map> and <unordered_set> for the hash table types.
#define SB_HAS_STD_UNORDERED_HASH 0

// GCC/Clang doesn't define a long long hash function, except for Android and
// Game consoles.
#define SB_HAS_LONG_LONG_HASH 0

// GCC/Clang doesn't define a string hash function, except for Game Consoles.
#define SB_HAS_STRING_HASH 0

// Desktop Linux needs a using statement for the hash functions.
#define SB_HAS_HASH_USING 0

// Set this to 1 if hash functions for custom types can be defined as a
// hash_value() function. Otherwise, they need to be placed inside a
// partially-specified hash struct template with an operator().
#define SB_HAS_HASH_VALUE 0

// Set this to 1 if use of hash_map or hash_set causes a deprecation warning
// (which then breaks the build).
#define SB_HAS_HASH_WARNING 1

// The location to include hash_map on this platform.
#define SB_HASH_MAP_INCLUDE <ext/hash_map>

// C++'s hash_map and hash_set are often found in different namespaces depending
// on the compiler.
#define SB_HASH_NAMESPACE __gnu_cxx

// The location to include hash_set on this platform.
#define SB_HASH_SET_INCLUDE <ext/hash_set>

// Define this to how this platform copies varargs blocks.
#define SB_VA_COPY(dest, source) va_copy(dest, source)

// --- Graphics Configuration ------------------------------------------------

// Specifies whether this platform supports a performant accelerated blitter
// API. The basic requirement is a scaled, clipped, alpha-blended blit.
#define SB_HAS_BLITTER 0

// Indicates whether or not the given platform supports bilinear filtering.
// This can be checked to enable/disable renderer tests that verify that this is
// working properly.
#define SB_HAS_BILINEAR_FILTERING_SUPPORT 1

// Indicates whether or not the given platform supports rendering of NV12
// textures. These textures typically originate from video decoders.
#define SB_HAS_NV12_TEXTURE_SUPPORT 1

// --- I/O Configuration -----------------------------------------------------
// Whether the current platform has speech synthesis.
#define SB_HAS_SPEECH_SYNTHESIS 1

// --- Decoder-only Params ---

// --- Memory Configuration --------------------------------------------------

// Whether this platform can map executable memory. Implies the platform can map
// memory. This is required for platforms that want to JIT.
#define SB_CAN_MAP_EXECUTABLE_MEMORY 1

// --- Network Configuration -------------------------------------------------

// Specifies whether this platform supports IPV6.
#define SB_HAS_IPV6 1

// Specifies whether this platform supports pipe.
#define SB_HAS_PIPE 1

// --- Timing API ------------------------------------------------------------

// Whether this platform has an API to retrieve how long the current thread
// has spent in the executing state.
#define SB_HAS_TIME_THREAD_NOW 1

// --- Thread Configuration --------------------------------------------------

// --- Tuneable Parameters ---------------------------------------------------

// --- User Configuration ----------------------------------------------------

// --- Platform Specific Audits ----------------------------------------------

#if !defined(__GNUC__)
#error "Android builds need a GCC-like compiler (for the moment)."
#endif

#endif  // STARBOARD_ANDROID_SHARED_CONFIGURATION_PUBLIC_H_
