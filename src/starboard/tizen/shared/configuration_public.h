// Copyright 2016 Samsung Electronics. All Rights Reserved.
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

// The shared Starboard configuration for Raspberry Pi devices.

#ifndef STARBOARD_TIZEN_SHARED_CONFIGURATION_PUBLIC_H_
#define STARBOARD_TIZEN_SHARED_CONFIGURATION_PUBLIC_H_

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

// Whether the current platform provides the standard header wchar.h.
#define SB_HAS_WCHAR_H 1

// Whether the current platform provides the standard header limits.h.
#define SB_HAS_LIMITS_H 1

// Whether the current platform provides the standard header float.h.
#define SB_HAS_FLOAT_H 1

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

// --- Architecture Configuration --------------------------------------------

// On default Linux desktop, you must be a superuser in order to set real time
// scheduling on threads.
#define SB_HAS_THREAD_PRIORITY_SUPPORT 0

// --- Attribute Configuration -----------------------------------------------

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

// --- Filesystem Configuration ----------------------------------------------

// The current platform's maximum length of the name of a single directory
// entry, not including the absolute path.
#define SB_FILE_MAX_NAME 64

// The current platform's maximum length of an absolute path.
#define SB_FILE_MAX_PATH 4096

// The current platform's maximum number of files that can be opened at the
// same time by one process.
#define SB_FILE_MAX_OPEN 256

// The current platform's file path component separator character. This is the
// character that appears after a directory in a file path. For example, the
// absolute canonical path of the file "/path/to/a/file.txt" uses '/' as a path
// component separator character.
#define SB_FILE_SEP_CHAR '/'

// The current platform's alternate file path component separator character.
// This is like SB_FILE_SEP_CHAR, except if your platform supports an alternate
// character, then you can place that here. For example, on windows machines,
// the primary separator character is probably '\', but the alternate is '/'.
#define SB_FILE_ALT_SEP_CHAR '/'

// The current platform's search path component separator character. When
// specifying an ordered list of absolute paths of directories to search for a
// given reason, this is the character that appears between entries. For
// example, the search path of "/etc/search/first:/etc/search/second" uses ':'
// as a search path component separator character.
#define SB_PATH_SEP_CHAR ':'

// The string form of SB_FILE_SEP_CHAR.
#define SB_FILE_SEP_STRING "/"

// The string form of SB_FILE_ALT_SEP_CHAR.
#define SB_FILE_ALT_SEP_STRING "/"

// The string form of SB_PATH_SEP_CHAR.
#define SB_PATH_SEP_STRING ":"

// Whether this platform supports symbolic links.
#define SB_HAS_SYMBOLIC_LINKS 1

// --- Memory Configuration --------------------------------------------------

// The memory page size, which controls the size of chunks on memory that
// allocators deal with, and the alignment of those chunks. This doesn't have to
// be the hardware-defined physical page size, but it should be a multiple of
// it.
#define SB_MEMORY_PAGE_SIZE 4096

// Whether this platform has and should use an MMAP function to map physical
// memory to the virtual address space.
#define SB_HAS_MMAP 1

// Whether this platform can map executable memory. Implies SB_HAS_MMAP. This is
// required for platforms that want to JIT.
#define SB_CAN_MAP_EXECUTABLE_MEMORY 1

// Whether this platform has and should use an growable heap (e.g. with sbrk())
// to map physical memory to the virtual address space.
#define SB_HAS_VIRTUAL_REGIONS 0

// Specifies the alignment for IO Buffers, in bytes. Some low-level network APIs
// may require buffers to have a specific alignment, and this is the place to
// specify that.
#define SB_NETWORK_IO_BUFFER_ALIGNMENT 16

// Determines the alignment that allocations should have on this platform.
#define SB_MALLOC_ALIGNMENT ((size_t)16U)

// Determines the threshhold of allocation size that should be done with mmap
// (if available), rather than allocated within the core heap.
#define SB_DEFAULT_MMAP_THRESHOLD ((size_t)(256 * 1024U))

// Defines the path where memory debugging logs should be written to.
#define SB_MEMORY_LOG_PATH "/tmp/starboard"

// --- Thread Configuration --------------------------------------------------

// Defines the maximum number of simultaneous threads for this platform. Some
// platforms require sharing thread handles with other kinds of system handles,
// like mutexes, so we want to keep this managable.
#define SB_MAX_THREADS 90

// The maximum number of thread local storage keys supported by this platform.
#define SB_MAX_THREAD_LOCAL_KEYS 512

// The maximum length of the name for a thread, including the NULL-terminator.
#define SB_MAX_THREAD_NAME_LENGTH 16;

// --- Graphics Configuration ------------------------------------------------

// Specifies whether this platform supports a performant accelerated blitter
// API. The basic requirement is a scaled, clipped, alpha-blended blit.
#define SB_HAS_BLITTER 0

// Specifies the preferred byte order of color channels in a pixel. Refer to
// starboard/configuration.h for the possible values. EGL/GLES platforms should
// generally prefer a byte order of RGBA, regardless of endianness.
#define SB_PREFERRED_RGBA_BYTE_ORDER SB_PREFERRED_RGBA_BYTE_ORDER_RGBA

// Indicates whether or not the given platform supports bilinear filtering.
// This can be checked to enable/disable renderer tests that verify that this is
// working properly.
#define SB_HAS_BILINEAR_FILTERING_SUPPORT 1

// Indicates whether or not the given platform supports rendering of NV12
// textures. These textures typically originate from video decoders.
#define SB_HAS_NV12_TEXTURE_SUPPORT 0

// Whether the current platform should frequently flip their display buffer.
// If this is not required (e.g. SB_MUST_FREQUENTLY_FLIP_DISPLAY_BUFFER is set
// to 0), then optimizations where the display buffer is not flipped if the
// scene hasn't changed are enabled.
#define SB_MUST_FREQUENTLY_FLIP_DISPLAY_BUFFER 0

// --- Network Configuration -------------------------------------------------

// Specifies whether this platform supports IPV6.
#define SB_HAS_IPV6 0

// Specifies whether this platform supports pipe.
#define SB_HAS_PIPE 1

// --- Tuneable Parameters ---------------------------------------------------

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
#define SB_NETWORK_RECEIVE_BUFFER_SIZE (0)

// --- User Configuration ----------------------------------------------------

// The maximum number of users that can be signed in at the same time.
#define SB_USER_MAX_SIGNED_IN 1

// --- Platform Specific Audits ----------------------------------------------
#define SB_IS_TIZEN_OS 1

// Tizen uses system icu package.
// undefined reference to 'icu_46::TimeZone::createDefault()'
#define SB_HAS_QUIRK_NO_TIMEZONE_NAME_SUPPORT 1

// Tizen TV product possible to use wayland display video window
// instead of Evas_Object
#define SB_CAN_USE_WAYLAND_VIDEO_WINDOW 0

#if !defined(__GNUC__)
#error "Tizen builds need a GCC-like compiler (for the moment)."
#endif

#endif  // STARBOARD_TIZEN_SHARED_CONFIGURATION_PUBLIC_H_
