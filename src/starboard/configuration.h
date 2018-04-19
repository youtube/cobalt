// Copyright 2015 Google Inc. All Rights Reserved.
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

// Module Overview: Starboard Configuration module
//
// Provides a description of the current platform in lurid detail so that
// common code never needs to actually know what the current operating system
// and architecture are.
//
// It is both very pragmatic and canonical in that if any application code
// finds itself needing to make a platform decision, it should always define
// a Starboard Configuration feature instead. This implies the continued
// existence of very narrowly-defined configuration features, but it retains
// porting control in Starboard.

#ifndef STARBOARD_CONFIGURATION_H_
#define STARBOARD_CONFIGURATION_H_

#if !defined(STARBOARD)
#error "You must define STARBOARD in Starboard builds."
#endif

#define SB_TRUE 1
#define SB_FALSE 0

// --- Common Defines --------------------------------------------------------

// The minimum API version allowed by this version of the Starboard headers,
// inclusive.
#define SB_MINIMUM_API_VERSION 4

// The maximum API version allowed by this version of the Starboard headers,
// inclusive.
#define SB_MAXIMUM_API_VERSION 10

// The API version that is currently open for changes, and therefore is not
// stable or frozen. Production-oriented ports should avoid declaring that they
// implement the experimental Starboard API version.
#define SB_EXPERIMENTAL_API_VERSION 10

// The next API version to be frozen, but is still subject to emergency
// changes. It is reasonable to base a port on the Release Candidate API
// version, but be aware that small incompatible changes may still be made to
// it.
// The following will be uncommented when an API version is a release candidate.
// #define SB_RELEASE_CANDIDATE_API_VERSION 9

// --- Experimental Feature Defines ------------------------------------------

// Note that experimental feature comments will be moved into
// starboard/CHANGELOG.md when released.  Thus, you can find examples of the
// format your feature comments should take by checking that file.

// EXAMPLE:
//   // Introduce new experimental feature.
//   //   Add a function, `SbMyNewFeature()` to `starboard/feature.h` which
//   //   exposes functionality for my new feature.
//   #define SB_MY_EXPERIMENTAL_FEATURE_VERSION SB_EXPERIMENTAL_API_VERSION

// API version where SbMediaTime is deprecated (for SbTime).
#define SB_DEPRECATE_SB_MEDIA_TIME_API_VERSION SB_EXPERIMENTAL_API_VERSION

// Minimum API version where supporting player error messages is required.
#define SB_PLAYER_ERROR_MESSAGE_API_VERSION SB_EXPERIMENTAL_API_VERSION

// Minimum API version for supporting system-level closed caption settings.
#define SB_ACCESSIBILITY_CAPTIONS_API_VERSION SB_EXPERIMENTAL_API_VERSION

// Minimum API version where supporting audioless video playback is required.
#define SB_AUDIOLESS_VIDEO_API_VERSION SB_EXPERIMENTAL_API_VERSION

// Minimum API version where calling SbPlayerCreate mutiple times (without
// calling SbPlayerDestroy in between) must not crash, and likewise calling
// SbAudioSinkCreate multiple times (without calling SbAudioSinkDestroy in
// between) must not crash. SbPlayerCreate may return kSbPlayerInvalid if
// additional players are not supported. SbAudioSinkCreate may return
// kSbAudionSinkInvalid if additional audio sinks are not supported.
#define SB_MULTI_PLAYER_API_VERSION SB_EXPERIMENTAL_API_VERSION

// API version where DRM session closed callback is required.
//   Add a callback to SbDrmCreateSystem that allows a DRM system to
//   signal that a DRM session has closed from the Starboard layer.
//   Previously, DRM sessions could only be closed from the application layer.
#define SB_DRM_SESSION_CLOSED_API_VERSION SB_EXPERIMENTAL_API_VERSION

// API version where kSbSystemPathSourceDirectory is removed.
//   Test code looking for its static input files should instead use the `test`
//   subdirectory in kSbSystemPathContentDirectory.
#define SB_PATH_SOURCE_DIR_REMOVED_API_VERSION SB_EXPERIMENTAL_API_VERSION

// API version where kSbSystemPropertyPlatformUuid is removed.
//   This property was only ever used in platforms using `in_app_dial`.
//   The only usage of this system property was replaced with a
//   self-contained mechanism.
#define SB_PROPERTY_UUID_REMOVED_API_VERSION SB_EXPERIMENTAL_API_VERSION

// API version where kSbMediaAudioSampleTypeInt16 is deprecated.
//   SB_HAS_QUIRK_SUPPORT_INT16_AUDIO_SAMPLES has to be defined to continue
//   support int16 audio samples after this version.
#define SB_DEPRECATE_INT16_AUDIO_SAMPLE_VERSION SB_EXPERIMENTAL_API_VERSION

// API version where SbSystemSupportsResume() is supported.
//   Platforms doesn't need to resume after suspend can return false in
//   SbSystemSupportsResume() to free up the resource used by resume after
//   suspend.
//   Please see the comment in system.h for more details.
#define SB_ALLOW_DISABLE_RESUME_VERSION SB_EXPERIMENTAL_API_VERSION

// Minimum API version for supporting the kSbKeyMicrophone keycode
#define SB_MICROPHONE_KEY_CODE_API_VERSION SB_EXPERIMENTAL_API_VERSION

// Add support for new decode target type,
// kSbDecodeTargetFormat3Plane10BitYUVI420.
//   Added kSbDecodeTargetFormat3Plane10BitYUVI420 to the SbDecodeTargetFormat
//   enum in order to support 10-bit YUV textures.
#define SB_10_BIT_YUV_I420_DECODE_TARGET_SUPPORT_API_VERSION \
  SB_EXPERIMENTAL_API_VERSION

// API version where SbAudioSinkConsumeFramesFunc() can optional take an
//   absolute timestamp to indicate when the frames are consumed.
//   Platforms that have the |frames_consumed| updated asynchronously can have
//   more accurate audio time reporting with this extra parameter.
//   Please see the comment in audio_sink.h for more details.
#define SB_ASYNC_AUDIO_FRAMES_REPORTING_API_VERSION SB_EXPERIMENTAL_API_VERSION

// --- Release Candidate Feature Defines -------------------------------------

// --- Common Detected Features ----------------------------------------------

#if defined(__GNUC__)
#define SB_IS_COMPILER_GCC 1
#elif defined(_MSC_VER)
#define SB_IS_COMPILER_MSVC 1
#endif

// --- Common Helper Macros --------------------------------------------------

// Determines a compile-time capability of the system.
#define SB_CAN(SB_FEATURE) \
  ((defined SB_CAN_##SB_FEATURE) && SB_CAN_##SB_FEATURE)

// Determines at compile-time whether this platform has a standard feature or
// header available.
#define SB_HAS(SB_FEATURE) \
  ((defined SB_HAS_##SB_FEATURE) && SB_HAS_##SB_FEATURE)

// Determines at compile-time an inherent aspect of this platform.
#define SB_IS(SB_FEATURE) ((defined SB_IS_##SB_FEATURE) && SB_IS_##SB_FEATURE)

// Determines at compile-time whether this platform has a quirk.
#define SB_HAS_QUIRK(SB_FEATURE) \
  ((defined SB_HAS_QUIRK_##SB_FEATURE) && SB_HAS_QUIRK_##SB_FEATURE)

// A constant expression that evaluates to the size_t size of a statically-sized
// array.
#define SB_ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

// A constant expression that evaluates to the int size of a statically-sized
// array.
#if defined(__cplusplus)
#define SB_ARRAY_SIZE_INT(array) static_cast<int>(SB_ARRAY_SIZE(array))
#else
#define SB_ARRAY_SIZE_INT(array) \
  ((int)(SB_ARRAY_SIZE(array)))  // NOLINT(readability/casting)
#endif

// Will cause a compiler error with |msg| if |expr| is false. |msg| must be a
// valid identifier, and must be a unique type in the scope of the declaration.
#if defined(__cplusplus)
namespace starboard {
template <bool>
struct CompileAssert {};
}  // namespace starboard

#define SB_COMPILE_ASSERT(expr, msg)                              \
  typedef ::starboard::CompileAssert<(                            \
      bool(expr))>              /* NOLINT(readability/casting) */ \
      msg[bool(expr) ? 1 : -1]  // NOLINT(readability/casting)
#else
#define SB_COMPILE_ASSERT(expr, msg) \
  extern char _COMPILE_ASSERT_##msg[(expr) ? 1 : -1]
#endif

// Standard CPP trick to stringify an evaluated macro definition.
#define SB_STRINGIFY(x) SB_STRINGIFY2(x)
#define SB_STRINGIFY2(x) #x

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define SB_DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);                  \
  void operator=(const TypeName&)

// An enumeration of values for the SB_PREFERRED_RGBA_BYTE_ORDER configuration
// variable.  Setting this up properly means avoiding slow color swizzles when
// passing pixel data from one library to another.  Note that these definitions
// are in byte-order and so are endianness-independent.
#define SB_PREFERRED_RGBA_BYTE_ORDER_RGBA 1
#define SB_PREFERRED_RGBA_BYTE_ORDER_BGRA 2
#define SB_PREFERRED_RGBA_BYTE_ORDER_ARGB 3

// --- Platform Configuration ------------------------------------------------

// Include the platform-specific configuration. This macro is set by GYP in
// starboard_base_target.gypi and passed in on the command line for all targets
// and all configurations.
#include STARBOARD_CONFIGURATION_INCLUDE

// --- Overridable Helper Macros ---------------------------------------------

// The following macros can be overridden in STARBOARD_CONFIGURATION_INCLUDE

// Makes a pointer-typed parameter restricted so that the compiler can make
// certain optimizations because it knows the pointers are unique.
#if !defined(SB_RESTRICT)
#if defined(__cplusplus)
#if SB_IS(COMPILER_MSVC)
#define SB_RESTRICT __restrict
#elif SB_IS(COMPILER_GCC)
#define SB_RESTRICT __restrict__
#else  // COMPILER
#define SB_RESTRICT
#endif  // COMPILER
#else   // __cplusplus
#define SB_RESTRICT __restrict
#endif  // __cplusplus
#endif  // SB_RESTRICT

// Tells the compiler a function is using a printf-style format string.
// |format_param| is the one-based index of the format string parameter;
// |dots_param| is the one-based index of the "..." parameter.
// For v*printf functions (which take a va_list), pass 0 for dots_param.
// (This is undocumented but matches what the system C headers do.)
// (Partially taken from base/compiler_specific.h)
#if !defined(SB_PRINTF_FORMAT)
#if SB_IS(COMPILER_GCC)
#define SB_PRINTF_FORMAT(format_param, dots_param) \
  __attribute__((format(printf, format_param, dots_param)))
#else
#define SB_PRINTF_FORMAT(format_param, dots_param)
#endif
#endif  // SB_PRINTF_FORMAT

// Trivially references a parameter that is otherwise unreferenced, preventing a
// compiler warning on some platforms.
#if !defined(SB_UNREFERENCED_PARAMETER)
#if SB_IS(COMPILER_MSVC)
#define SB_UNREFERENCED_PARAMETER(x) (x)
#else
#define SB_UNREFERENCED_PARAMETER(x) \
  do {                               \
    (void)(x);                       \
  } while (0)
#endif
#endif  // SB_UNREFERENCED_PARAMETER

// Causes the annotated (at the end) function to generate a warning if the
// result is not accessed.
#if !defined(SB_WARN_UNUSED_RESULT)
#if defined(COMPILER_GCC)
#define SB_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define SB_WARN_UNUSED_RESULT
#endif  // COMPILER_GCC
#endif  // SB_WARN_UNUSED_RESULT

// Declares a function as overriding a virtual function on compilers that
// support it.
#if !defined(SB_OVERRIDE)
#if defined(COMPILER_MSVC)
#define SB_OVERRIDE override
#elif defined(__clang__)
#define SB_OVERRIDE override
#else
#define SB_OVERRIDE
#endif
#endif  // SB_OVERRIDE

// Declare numeric literals of signed 64-bit type.
#if !defined(SB_INT64_C)
#if defined(_MSC_VER)
#define SB_INT64_C(x) x##I64
#else  // defined(_MSC_VER)
#define SB_INT64_C(x) x##LL
#endif  // defined(_MSC_VER)
#endif  // SB_INT64_C

// Declare numeric literals of unsigned 64-bit type.
#if !defined(SB_UINT64_C)
#if defined(_MSC_VER)
#define SB_UINT64_C(x) x##UI64
#else  // defined(_MSC_VER)
#define SB_UINT64_C(x) x##ULL
#endif  // defined(_MSC_VER)
#endif  // SB_INT64_C

// Macro for hinting that an expression is likely to be true.
#if !defined(SB_LIKELY)
#if SB_IS(COMPILER_GCC)
#define SB_LIKELY(x) __builtin_expect(!!(x), 1)
#elif SB_IS(COMPILER_MSVC)
#define SB_LIKELY(x) (x)
#else
#define SB_LIKELY(x) (x)
#endif  // SB_IS(COMPILER_MSVC)
#endif  // !defined(SB_LIKELY)

// Macro for hinting that an expression is likely to be false.
#if !defined(SB_UNLIKELY)
#if SB_IS(COMPILER_GCC)
#define SB_UNLIKELY(x) __builtin_expect(!!(x), 0)
#elif SB_IS(COMPILER_MSVC)
#define SB_UNLIKELY(x) (x)
#else
#define SB_UNLIKELY(x) (x)
#endif  // SB_IS(COMPILER_MSVC)
#endif  // !defined(SB_UNLIKELY)

// SB_DEPRECATED(int Foo(int bar));
//   Annotates the function as deprecated, which will trigger a compiler
//   warning when referenced.
#if !defined(SB_DEPRECATED)
#if SB_IS(COMPILER_GCC)
#define SB_DEPRECATED(FUNC) FUNC __attribute__((deprecated))
#elif SB_IS(COMPILER_MSVC)
#define SB_DEPRECATED(FUNC) __declspec(deprecated) FUNC
#else
// Empty definition for other compilers.
#define SB_DEPRECATED(FUNC) FUNC
#endif
#endif  // SB_DEPRECATED

// SB_DEPRECATED_EXTERNAL(...) annotates the function as deprecated for
// external clients, but not deprecated for starboard.
#if defined(STARBOARD_IMPLEMENTATION)
#define SB_DEPRECATED_EXTERNAL(FUNC) FUNC
#else
#define SB_DEPRECATED_EXTERNAL(FUNC) SB_DEPRECATED(FUNC)
#endif

// Macro to annotate a function as noreturn, which signals to the compiler
// that the function cannot return.
#if !defined(SB_NORETURN)
#if SB_IS(COMPILER_GCC)
#define SB_NORETURN __attribute__((__noreturn__))
#elif SB_IS(COMPILER_MSVC)
#define SB_NORETURN __declspec(noreturn)
#else
// Empty definition for other compilers.
#define SB_NORETURN
#endif
#endif  // SB_NORETURN

// Specifies the alignment for a class, struct, union, enum, class/struct field,
// or stack variable.
#if !defined(SB_ALIGNAS)
#if SB_IS(COMPILER_GCC)
#define SB_ALIGNAS(byte_alignment) __attribute__((aligned(byte_alignment)))
#elif SB_IS(COMPILER_MSVC)
#define SB_ALIGNAS(byte_alignment) __declspec(align(byte_alignment))
#else
// Fallback to the C++11 form.
#define SB_ALIGNAS(byte_alignment) alignas(byte_alignment)
#endif
#endif  // !defined(SB_ALIGNAS)

// Returns the alignment reqiured for any instance of the type indicated by
// |type|.
#if !defined(SB_ALIGNOF)
#if SB_IS(COMPILER_GCC)
#define SB_ALIGNOF(type) __alignof__(type)
#elif SB_IS(COMPILER_MSVC)
#define SB_ALIGNOF(type) (sizeof(type) - sizeof(type) + __alignof(type))
#else
// Fallback to the C++11 form.
#define SB_ALIGNOF(type) alignof(type)
#endif
#endif  // !defined(SB_ALIGNOF)

// --- Configuration Audits --------------------------------------------------

#if !defined(SB_API_VERSION)
#error "SB_API_VERSION was not defined by your platform."
#endif

#if SB_API_VERSION > SB_MAXIMUM_API_VERSION
#error "Your platform's SB_API_VERSION > SB_MAXIMUM_API_VERSION."
#endif

#if SB_API_VERSION < SB_MINIMUM_API_VERSION
#error "Your platform's SB_API_VERSION < SB_MINIMUM_API_VERSION."
#endif

#if !SB_IS(ARCH_ARM) && !SB_IS(ARCH_MIPS) && !SB_IS(ARCH_PPC) && \
    !SB_IS(ARCH_X86)
#error "Your platform doesn't define a known architecture."
#endif

#if SB_IS(32_BIT) == SB_IS(64_BIT)
#error "Your platform must be exactly one of { 32-bit, 64-bit }."
#endif

#if SB_HAS(32_BIT_POINTERS) == SB_HAS(64_BIT_POINTERS)
#error "Your platform's pointer sizes must be either 32 bit or 64 bit."
#endif

#if SB_HAS(32_BIT_LONG) == SB_HAS(64_BIT_LONG)
#error "Your platform's long size must be either 32 bit or 64 bit."
#endif

#if SB_HAS(32_BIT_LONG)
SB_COMPILE_ASSERT(sizeof(long) == 4,  // NOLINT(runtime/int)
                  SB_HAS_32_BIT_LONG_is_inconsistent_with_sizeof_long);
#endif

#if SB_HAS(64_BIT_LONG)
SB_COMPILE_ASSERT(sizeof(long) == 8,  // NOLINT(runtime/int)
                  SB_HAS_64_BIT_LONG_is_inconsistent_with_sizeof_long);
#endif

#if !defined(SB_IS_BIG_ENDIAN)
#error "Your platform must define SB_IS_BIG_ENDIAN."
#endif

#if defined(SB_IS_LITTLE_ENDIAN)
#error "SB_IS_LITTLE_ENDIAN is set based on SB_IS_BIG_ENDIAN."
#endif

#if SB_IS(WCHAR_T_UTF16) == SB_IS(WCHAR_T_UTF32)
#error "You must define either SB_IS_WCHAR_T_UTF16 or SB_IS_WCHAR_T_UTF32."
#endif

#if defined(SB_IS_WCHAR_T_SIGNED) && defined(SB_IS_WCHAR_T_UNSIGNED)
#error "You can't define SB_IS_WCHAR_T_SIGNED and SB_IS_WCHAR_T_UNSIGNED."
#endif

#if !defined(SB_C_FORCE_INLINE)
#error "Your platform must define SB_C_FORCE_INLINE."
#endif

#if !defined(SB_C_INLINE)
#error "Your platform must define SB_C_INLINE."
#endif

#if !defined(SB_EXPORT_PLATFORM)
#error "Your platform must define SB_EXPORT_PLATFORM."
#endif

#if !defined(SB_IMPORT_PLATFORM)
#error "Your platform must define SB_IMPORT_PLATFORM."
#endif

#if !defined(SB_HASH_MAP_INCLUDE)
#error "Your platform must define SB_HASH_MAP_INCLUDE."
#endif

#if !defined(SB_HASH_NAMESPACE)
#error "Your platform must define SB_HASH_NAMESPACE."
#endif

#if !defined(SB_HASH_SET_INCLUDE)
#error "Your platform must define SB_HASH_SET_INCLUDE."
#endif

#if !defined(SB_FILE_MAX_NAME) || SB_FILE_MAX_NAME < 2
#error "Your platform must define SB_FILE_MAX_NAME > 1."
#endif

#if !defined(SB_FILE_MAX_PATH) || SB_FILE_MAX_PATH < 2
#error "Your platform must define SB_FILE_MAX_PATH > 1."
#endif

#if !defined(SB_FILE_SEP_CHAR)
#error "Your platform must define SB_FILE_SEP_CHAR."
#endif

#if !defined(SB_FILE_ALT_SEP_CHAR)
#error "Your platform must define SB_FILE_ALT_SEP_CHAR."
#endif

#if !defined(SB_PATH_SEP_CHAR)
#error "Your platform must define SB_PATH_SEP_CHAR."
#endif

#if !defined(SB_FILE_SEP_STRING)
#error "Your platform must define SB_FILE_SEP_STRING."
#endif

#if !defined(SB_FILE_ALT_SEP_STRING)
#error "Your platform must define SB_FILE_ALT_SEP_STRING."
#endif

#if !defined(SB_PATH_SEP_STRING)
#error "Your platform must define SB_PATH_SEP_STRING."
#endif

#if !defined(SB_MAX_THREADS)
#error "Your platform must define SB_MAX_THREADS."
#endif

#if !defined(SB_MAX_THREAD_LOCAL_KEYS)
#error "Your platform must define SB_MAX_THREAD_LOCAL_KEYS."
#endif

#if !defined(SB_MAX_THREAD_NAME_LENGTH)
#error "Your platform must define SB_MAX_THREAD_NAME_LENGTH."
#endif

#if !defined(SB_HAS_MICROPHONE)
#error "Your platform must define SB_HAS_MICROPHONE in API versions 2 or later."
#endif

#if !defined(SB_HAS_TIME_THREAD_NOW)
#error "Your platform must define SB_HAS_TIME_THREAD_NOW in API 3 or later."
#endif

#if defined(SB_IS_PLAYER_COMPOSITED) || defined(SB_IS_PLAYER_PUNCHED_OUT) || \
    defined(SB_IS_PLAYER_PRODUCING_TEXTURE)
#error "New versions of Starboard specify player output mode at runtime."
#endif

#if SB_HAS(PLAYER_WITH_URL) && SB_API_VERSION < 8
#error "SB_HAS_PLAYER_WITH_URL is not supported in this API version."
#endif

#if (SB_HAS(MANY_CORES) && (SB_HAS(1_CORE) || SB_HAS(2_CORES) ||    \
                            SB_HAS(4_CORES) || SB_HAS(6_CORES))) || \
    (SB_HAS(1_CORE) &&                                              \
     (SB_HAS(2_CORES) || SB_HAS(4_CORES) || SB_HAS(6_CORES))) ||    \
    (SB_HAS(2_CORES) && (SB_HAS(4_CORES) || SB_HAS(6_CORES))) ||    \
    (SB_HAS(4_CORES) && SB_HAS(6_CORES))
#error "Only one SB_HAS_{MANY, 1, 2, 4, 6}_CORE[S] can be defined per platform."
#endif

#if !defined(SB_HAS_THREAD_PRIORITY_SUPPORT)
#error "Your platform must define SB_HAS_THREAD_PRIORITY_SUPPORT."
#endif

#if !defined(SB_PREFERRED_RGBA_BYTE_ORDER)
// Legal values for SB_PREFERRED_RGBA_BYTE_ORDER are defined in this file above
// as SB_PREFERRED_RGBA_BYTE_ORDER_*.
// If your platform uses GLES, you should set this to
// SB_PREFERRED_RGBA_BYTE_ORDER_RGBA.
#error "Your platform must define SB_PREFERRED_RGBA_BYTE_ORDER."
#endif

#if SB_PREFERRED_RGBA_BYTE_ORDER != SB_PREFERRED_RGBA_BYTE_ORDER_RGBA && \
    SB_PREFERRED_RGBA_BYTE_ORDER != SB_PREFERRED_RGBA_BYTE_ORDER_BGRA && \
    SB_PREFERRED_RGBA_BYTE_ORDER != SB_PREFERRED_RGBA_BYTE_ORDER_ARGB
#error "SB_PREFERRED_RGBA_BYTE_ORDER has been assigned an invalid value."
#endif

#if !defined(SB_HAS_BILINEAR_FILTERING_SUPPORT)
#error "Your platform must define SB_HAS_BILINEAR_FILTERING_SUPPORT."
#endif

#if !defined(SB_HAS_NV12_TEXTURE_SUPPORT)
#error "Your platform must define SB_HAS_NV12_TEXTURE_SUPPORT."
#endif

#if !defined(SB_MUST_FREQUENTLY_FLIP_DISPLAY_BUFFER)
#error "Your platform must define SB_MUST_FREQUENTLY_FLIP_DISPLAY_BUFFER."
#endif

#if !defined(SB_MEDIA_MAX_AUDIO_BITRATE_IN_BITS_PER_SECOND)
#error \
    "Your platform must define SB_MEDIA_MAX_AUDIO_BITRATE_IN_BITS_PER_SECOND."
#endif  // !defined(SB_MEDIA_MAX_AUDIO_BITRATE_IN_BITS_PER_SECOND)

#if !defined(SB_MEDIA_MAX_VIDEO_BITRATE_IN_BITS_PER_SECOND)
#error \
    "Your platform must define SB_MEDIA_MAX_VIDEO_BITRATE_IN_BITS_PER_SECOND."
#endif  // !defined(SB_MEDIA_MAX_VIDEO_BITRATE_IN_BITS_PER_SECOND)

#if defined(SB_MEDIA_SOURCE_BUFFER_STREAM_AUDIO_MEMORY_LIMIT)
#error "SB_MEDIA_SOURCE_BUFFER_STREAM_AUDIO_MEMORY_LIMIT is deprecated."
#error "Use gyp variable |cobalt_media_buffer_non_video_budget| instead."
#endif  // defined(SB_MEDIA_SOURCE_BUFFER_STREAM_AUDIO_MEMORY_LIMIT)

#if defined(SB_MEDIA_SOURCE_BUFFER_STREAM_VIDEO_MEMORY_LIMIT)
#error "SB_MEDIA_SOURCE_BUFFER_STREAM_VIDEO_MEMORY_LIMIT is deprecated."
#error "Use gyp variable |cobalt_media_buffer_video_budget_1080p| instead."
#error "Use gyp variable |cobalt_media_buffer_video_budget_4k| instead."
#endif  // defined(SB_MEDIA_SOURCE_BUFFER_STREAM_VIDEO_MEMORY_LIMIT)

#if defined(SB_MEDIA_MAIN_BUFFER_BUDGET)
#error "SB_MEDIA_MAIN_BUFFER_BUDGET is deprecated."
#endif  // defined(SB_MEDIA_MAIN_BUFFER_BUDGET)

#if defined(SB_MEDIA_GPU_BUFFER_BUDGET)
#error "SB_MEDIA_GPU_BUFFER_BUDGET is deprecated."
#endif  // defined(SB_MEDIA_GPU_BUFFER_BUDGET)

#if SB_API_VERSION >= 6
#if defined(SB_HAS_DRM_KEY_STATUSES)
#if !SB_HAS(DRM_KEY_STATUSES)
#error "SB_HAS_DRM_KEY_STATUSES is required for Starboard 6 or later."
#endif  // !SB_HAS(DRM_KEY_STATUSES)
#else   // defined(SB_HAS_DRM_KEY_STATUSES)
#define SB_HAS_DRM_KEY_STATUSES 1
#endif  // defined(SB_HAS_DRM_KEY_STATUSES)
#endif  // SB_API_VERSION >= 6

#if SB_API_VERSION >= SB_DRM_SESSION_CLOSED_API_VERSION
#if defined(SB_HAS_DRM_SESSION_CLOSED)
#if !SB_HAS(DRM_SESSION_CLOSED)
#error "SB_HAS_DRM_SESSION_CLOSED is required in this API version."
#endif  // !SB_HAS(DRM_SESSION_CLOSED)
#else   // defined(SB_HAS_DRM_SESSION_CLOSED)
#define SB_HAS_DRM_SESSION_CLOSED 1
#endif  // defined(SB_HAS_DRM_SESSION_CLOSED)
#endif  // SB_API_VERSION >= SB_DRM_SESSION_CLOSED_API_VERSION

#if SB_API_VERSION >= 5
#if !defined(SB_HAS_SPEECH_RECOGNIZER)
#error "Your platform must define SB_HAS_SPEECH_RECOGNIZER."
#endif  // !defined(SB_HAS_SPEECH_RECOGNIZER)
#endif  // SB_API_VERSION >= 5

#if SB_API_VERSION >= 8
#if !defined(SB_HAS_ON_SCREEN_KEYBOARD)
#error "Your platform must define SB_HAS_ON_SCREEN_KEYBOARD."
#endif  // !defined(SB_HAS_ON_SCREEN_KEYBOARD)
#if !defined(SB_HAS_PLAYER_WITH_URL)
#error "Your platform must define SB_HAS_PLAYER_WITH_URL."
#endif  // !defined(SB_HAS_PLAYER_WITH_URL)
#endif  // SB_API_VERSION >= 8

#if SB_HAS(ON_SCREEN_KEYBOARD) && (SB_API_VERSION < 8)
#error "SB_HAS_ON_SCREEN_KEYBOARD not supported in this API version."
#endif

#if SB_HAS(CAPTIONS) && \
    (SB_API_VERSION < SB_ACCESSIBILITY_CAPTIONS_API_VERSION)
#error "SB_HAS_CAPTIONS not supported in this API version."
#endif

#if SB_API_VERSION >= SB_AUDIOLESS_VIDEO_API_VERSION
#define SB_HAS_AUDIOLESS_VIDEO 1
#endif

#if SB_API_VERSION >= SB_PLAYER_ERROR_MESSAGE_API_VERSION
#define SB_HAS_PLAYER_ERROR_MESSAGE 1
#endif

#if SB_API_VERSION < SB_DEPRECATE_INT16_AUDIO_SAMPLE_VERSION
#if !SB_HAS_QUIRK(SUPPORT_INT16_AUDIO_SAMPLES)
#define SB_HAS_QUIRK_SUPPORT_INT16_AUDIO_SAMPLES 1
#endif  // !SB_HAS_QUIRK(SUPPORT_INT16_AUDIO_SAMPLES)
#endif  // SB_API_VERSION < SB_DEPRECATE_INT16_AUDIO_SAMPLE_VERSION

#if SB_API_VERSION >= SB_ASYNC_AUDIO_FRAMES_REPORTING_API_VERSION
#if !defined(SB_HAS_ASYNC_AUDIO_FRAMES_REPORTING)
#error Your platform must define SB_HAS_ASYNC_AUDIO_FRAMES_REPORTING in API \
    version SB_ASYNC_AUDIO_FRAMES_REPORTING_API_VERSION or later.
#endif  // !defined(SB_HAS_ASYNC_AUDIO_FRAMES_REPORTING)
#endif  // SB_API_VERSION >= SB_ASYNC_AUDIO_FRAMES_REPORTING_API_VERSION

// --- Derived Configuration -------------------------------------------------

// Whether the current platform is little endian.
#if SB_IS(BIG_ENDIAN)
#define SB_IS_LITTLE_ENDIAN 0
#else
#define SB_IS_LITTLE_ENDIAN 1
#endif

// Whether the current platform has 64-bit atomic operations.
#if SB_IS(64_BIT)
#define SB_HAS_64_BIT_ATOMICS 1
#else
#define SB_HAS_64_BIT_ATOMICS 0
#endif

// Whether we use __PRETTY_FUNCTION__ or __FUNCTION__ for logging.
#if SB_IS(COMPILER_GCC)
#define SB_FUNCTION __PRETTY_FUNCTION__
#else
#define SB_FUNCTION __FUNCTION__
#endif

// --- Gyp Derived Configuration -----------------------------------------------

// Specifies whether this platform has a performant OpenGL ES 2 implementation,
// which allows client applications to use GL rendering paths.  Derived from
// the gyp variable `gl_type` which indicates what kind of GL implementation
// is available.
#if !defined(SB_HAS_GLES2)
#define SB_HAS_GLES2 !SB_GYP_GL_TYPE_IS_NONE
#endif

// Specifies whether this platform has any kind of supported graphics system.
#if !defined(SB_HAS_GRAPHICS)
#if SB_HAS(GLES2) || SB_HAS(BLITTER)
#define SB_HAS_GRAPHICS 1
#else
#define SB_HAS_GRAPHICS 0
#endif
#endif

// Specifies whether the starboard media pipeline components (SbPlayerPipeline
// and StarboardDecryptor) are used.  Set to 0 means they are not used.
#define SB_CAN_MEDIA_USE_STARBOARD_PIPELINE \
  SB_GYP_CAN_MEDIA_USE_STARBOARD_PIPELINE

#endif  // STARBOARD_CONFIGURATION_H_
