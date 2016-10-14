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

// The API version implemented by this platform.
#define SB_API_VERSION 1

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
#define SB_HAS_NV12_TEXTURE_SUPPORT 1

// Whether the current platform should frequently flip their display buffer.
// If this is not required (e.g. SB_MUST_FREQUENTLY_FLIP_DISPLAY_BUFFER is set
// to 0), then optimizations where the display buffer is not flipped if the
// scene hasn't changed are enabled.
#define SB_MUST_FREQUENTLY_FLIP_DISPLAY_BUFFER 0

// --- Media Configuration ---------------------------------------------------

// Specifies whether this platform has support for a possibly-decrypting
// elementary stream player for at least H.264/AAC (and AES-128-CTR, if
// decrypting). A player is responsible for ingesting an audio and video
// elementary stream, optionally-encrypted, and ultimately producing
// synchronized audio/video. If a player is defined, it must choose one of the
// supported composition methods below.
#define SB_HAS_PLAYER 1

// Specifies whether this platform's player will produce an OpenGL texture that
// the client must draw every frame with its graphics rendering. It may be that
// we get a texture handle, but cannot perform operations like GlReadPixels on
// it if it is DRM-protected.
#define SB_IS_PLAYER_PRODUCING_TEXTURE 0

// Specifies whether this platform's player is composited with a formal
// compositor, where the client must specify how video is to be composited into
// the graphicals scene.
#define SB_IS_PLAYER_COMPOSITED 0

// Specifies whether this platform's player uses a "punch-out" model, where
// video is rendered to the far background, and the graphics plane is
// automatically composited on top of the video by the platform. The client must
// punch an alpha hole out of the graphics plane for video to show through.  In
// this case, changing the video bounds must be tightly synchronized between the
// player and the graphics plane.
#define SB_IS_PLAYER_PUNCHED_OUT 1

// Specifies the maximum amount of memory used by audio buffers of media source
// before triggering a garbage collection.  A large value will cause more memory
// being used by audio buffers but will also make JavaScript app less likely to
// re-download audio data.  Note that the JavaScript app may experience
// significant difficulty if this value is too low.
#define SB_MEDIA_SOURCE_BUFFER_STREAM_AUDIO_MEMORY_LIMIT (3U * 1024U * 1024U)

// Specifies the maximum amount of memory used by video buffers of media source
// before triggering a garbage collection.  A large value will cause more memory
// being used by video buffers but will also make JavaScript app less likely to
// re-download video data.  Note that the JavaScript app may experience
// significant difficulty if this value is too low.
#define SB_MEDIA_SOURCE_BUFFER_STREAM_VIDEO_MEMORY_LIMIT (16U * 1024U * 1024U)

// Specifies how much memory to reserve up-front for the main media buffer
// (usually resides inside the CPU memory) used by media source and demuxers.
// The main media buffer can work in one of the following two ways:
// 1. If GPU buffer is used (i.e. SB_MEDIA_GPU_BUFFER_BUDGET is non-zero), the
//    main buffer will be used as a cache so a media buffer will be copied from
//    GPU memory to main memory before sending to the decoder for further
//    processing.  In this case this macro should be set to a value that is
//    large enough to hold all media buffers being decoded.
// 2. If GPU buffer is not used (i.e. SB_MEDIA_GPU_BUFFER_BUDGET is zero) all
//    media buffers will reside in the main memory buffer.  In this case the
//    macro should be set to a value that is greater than the sum of the above
//    source buffer stream memory limits with extra room to take account of
//    fragmentations and memory used by demuxers.
#define SB_MEDIA_MAIN_BUFFER_BUDGET (128U * 1024U * 1024U)

// Specifies how much GPU memory to reserve up-front for media source buffers.
// This should only be set to non-zero on system with limited CPU memory and
// excess GPU memory so the app can store media buffer in GPU memory.
// SB_MEDIA_MAIN_BUFFER_BUDGET has to be set to a non-zero value to avoid
// media buffers being decoded when being stored in GPU.
#define SB_MEDIA_GPU_BUFFER_BUDGET 0U

// Specifies whether this platform has webm/vp9 support.  This should be set to
// non-zero on platforms with webm/vp9 support.
#define SB_HAS_MEDIA_WEBM_VP9_SUPPORT 0

// Specifies the stack size for threads created inside media stack.  Set to 0 to
// use the default thread stack size.  Set to non-zero to explicitly set the
// stack size for media stack threads.
#define SB_MEDIA_THREAD_STACK_SIZE 0U

// --- Decoder-only Params ---

// Specifies how media buffers must be aligned on this platform as some
// decoders may have special requirement on the alignment of buffers being
// decoded.
#define SB_MEDIA_BUFFER_ALIGNMENT 128U

// Specifies how video frame buffers must be aligned on this platform.
#define SB_MEDIA_VIDEO_FRAME_ALIGNMENT 256U

// The encoded video frames are compressed in different ways, their decoding
// time can vary a lot.  Occasionally a single frame can take longer time to
// decode than the average time per frame.  The player has to cache some frames
// to account for such inconsistency.  The number of frames being cached are
// controlled by the following two macros.
//
// Specify the number of video frames to be cached before the playback starts.
// Note that set this value too large may increase the playback start delay.
#define SB_MEDIA_MAXIMUM_VIDEO_PREROLL_FRAMES 4

// Specify the number of video frames to be cached during playback.  A large
// value leads to more stable fps but also causes the app to use more memory.
#define SB_MEDIA_MAXIMUM_VIDEO_FRAMES 12

// --- Network Configuration -------------------------------------------------

// Specifies whether this platform supports IPV6.
#define SB_HAS_IPV6 1

// Specifies whether this platform supports pipe.
#define SB_HAS_PIPE 1

// --- Tuneable Parameters ---------------------------------------------------

// Specifies the network receive buffer size.
#define SB_NETWORK_RECEIVE_BUFFER_SIZE (16 * 1024)

// --- User Configuration ----------------------------------------------------

// The maximum number of users that can be signed in at the same time.
#define SB_USER_MAX_SIGNED_IN 1

// --- Platform Specific Audits ----------------------------------------------

#if !defined(__GNUC__)
#error "Tizen builds need a GCC-like compiler (for the moment)."
#endif

#endif  // STARBOARD_RASPI_SHARED_CONFIGURATION_PUBLIC_H_
