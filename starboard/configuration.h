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

// --- Common Defines --------------------------------------------------------

// The minimum API version allowed by this version of the Starboard headers,
// inclusive.
#define SB_MINIMUM_API_VERSION 11

// The maximum API version allowed by this version of the Starboard headers,
// inclusive.
#define SB_MAXIMUM_API_VERSION 14

// The API version that is currently open for changes, and therefore is not
// stable or frozen. Production-oriented ports should avoid declaring that they
// implement the experimental Starboard API version.
#define SB_EXPERIMENTAL_API_VERSION 14

// The next API version to be frozen, but is still subject to emergency
// changes. It is reasonable to base a port on the Release Candidate API
// version, but be aware that small incompatible changes may still be made to
// it.
// The following will be uncommented when an API version is a release candidate.
// #define SB_RELEASE_CANDIDATE_API_VERSION 13

// --- Experimental Feature Defines ------------------------------------------

// Note that experimental feature comments will be moved into
// starboard/CHANGELOG.md when released.  Thus, you can find examples of the
// format your feature comments should take by checking that file.

// EXAMPLE:
//   // Introduce new experimental feature.
//   //   Add a function, `SbMyNewFeature()` to `starboard/feature.h` which
//   //   exposes functionality for my new feature.
//   #define SB_MY_EXPERIMENTAL_FEATURE_VERSION SB_EXPERIMENTAL_API_VERSION

// --- Release Candidate Feature Defines -------------------------------------

// --- Common Detected Features ----------------------------------------------

#if defined(__GNUC__)
#define SB_IS_COMPILER_GCC 1
#elif defined(_MSC_VER)
#define SB_IS_COMPILER_MSVC 1
#endif

// --- Common Helper Macros --------------------------------------------------

#if SB_API_VERSION < 13
#define SB_TRUE 1
#define SB_FALSE 0
#else
#define SB_TRUE #error "The macro SB_TRUE is deprecated."
#define SB_FALSE #error "The macro SB_FALSE is deprecated."
#endif

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

// Determines at compile-time the size of a data type, or 0 if the data type
// that was specified was invalid.
#define SB_SIZE_OF(DATATYPE) \
  ((defined SB_SIZE_OF_##DATATYPE) ? (SB_SIZE_OF_##DATATYPE) : (0))

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
extern "C++" {
namespace starboard {
template <bool>
struct CompileAssert {};
}  // namespace starboard
}  // extern "C++"

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

#if SB_API_VERSION < 13
// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define SB_DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&) = delete;         \
  void operator=(const TypeName&) = delete
#else
#define SB_DISALLOW_COPY_AND_ASSIGN \
  #error "The SB_DISALLOW_COPY_AND_ASSIGN macro is deprecated."
#endif  // SB_API_VERSION < 13

// An enumeration of values for the kSbPreferredByteOrder configuration
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

#if SB_API_VERSION < 12
// After version 12, we start to use runtime constants
// instead of macros for certain platform dependent configurations. This file
// substitutes configuration macros for the corresponding runtime constants so
// we don't reference these constants when they aren't defined.
#include "starboard/shared/starboard/configuration_constants_compatibility_defines.h"
#endif  // SB_API_VERSION < 12

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
#if SB_API_VERSION < 13
#if !defined(SB_OVERRIDE)
#if defined(COMPILER_MSVC)
#define SB_OVERRIDE override
#elif defined(__clang__)
#define SB_OVERRIDE override
#else
#define SB_OVERRIDE
#endif
#endif  // SB_OVERRIDE
#else
#define SB_OVERRIDE \
  #error "The SB_OVERRIDE macro is deprecated. Please use \"override\" instead."
#endif  // SB_API_VERSION < 13

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

// Returns the alignment required for any instance of the type indicated by
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

#if !SB_IS(ARCH_ARM) && !SB_IS(ARCH_ARM64) && !SB_IS(ARCH_X86) && \
    !SB_IS(ARCH_X64)
#error "Your platform doesn't define a known architecture."
#endif

#if SB_IS(32_BIT) == SB_IS(64_BIT)
#error "Your platform must be exactly one of { 32-bit, 64-bit }."
#endif

#if SB_API_VERSION >= 12

#if !SB_IS(BIG_ENDIAN) && !SB_IS(LITTLE_ENDIAN) || \
    (SB_IS(BIG_ENDIAN) == SB_IS(LITTLE_ENDIAN))
#error "Your platform's endianness must be defined as big or little."
#endif

#else  // SB_API_VERSION < 12

#if SB_IS(ARCH_X86) && SB_IS(64_BIT)
#undef SB_IS_ARCH_X86
#define SB_IS_ARCH_X64 1
#endif  // SB_IS(ARCH_X86) && SB_IS(64_BIT)

#if SB_IS(ARCH_ARM) && SB_IS(64_BIT)
#undef SB_IS_ARCH_ARM
#define SB_IS_ARCH_ARM64 1
#endif  // SB_IS(ARCH_ARM) && SB_IS(64_BIT)

#if SB_HAS(32_BIT_LONG)
#define SB_SIZE_OF_LONG 4
#elif SB_HAS(64_BIT_LONG)
#define SB_SIZE_OF_LONG 8
#endif  // SB_HAS(32_BIT_LONG)

#if SB_HAS(32_BIT_POINTERS)
#define SB_SIZE_OF_POINTER 4
#elif SB_HAS(64_BIT_POINTERS)
#define SB_SIZE_OF_POINTER 8
#endif  // SB_HAS(32_BIT_POINTER)

SB_COMPILE_ASSERT(sizeof(long) == SB_SIZE_OF_LONG,  // NOLINT(runtime/int)
                  SB_SIZE_OF_LONG_is_inconsistent_with_sizeof_long);

#if !defined(SB_IS_BIG_ENDIAN)
#error "Your platform must define SB_IS_BIG_ENDIAN."
#endif

#if defined(SB_IS_LITTLE_ENDIAN)
#error "SB_IS_LITTLE_ENDIAN is set based on SB_IS_BIG_ENDIAN."
#endif

#endif  // SB_API_VERSION >= 12

#if (SB_SIZE_OF(POINTER) != 4) && (SB_SIZE_OF(POINTER) != 8)
#error "Your platform's pointer sizes must be either 32 bit or 64 bit."
#endif

#if (SB_SIZE_OF(LONG) != 4) && (SB_SIZE_OF(LONG) != 8)
#error "Your platform's long size must be either 32 bit or 64 bit."
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

#if !SB_HAS(STD_UNORDERED_HASH)

#if !defined(SB_HASH_MAP_INCLUDE)
#error \
    "Your platform must define SB_HASH_MAP_INCLUDE or "\
    "define SB_HAS_STD_UNORDERED_HASH 1."
#endif

#if !defined(SB_HASH_NAMESPACE)
#error \
    "Your platform must define SB_HASH_NAMESPACE or "\
    "define SB_HAS_STD_UNORDERED_HASH 1."
#endif

#if !defined(SB_HASH_SET_INCLUDE)
#error \
    "Your platform must define SB_HASH_SET_INCLUDE or "\
    "define SB_HAS_STD_UNORDERED_HASH 1."
#endif

#endif  // !SB_HAS(STD_UNORDERED_HASH)

#if SB_API_VERSION >= 12

#if defined(SB_DEFAULT_MMAP_THRESHOLD)
#error \
    "SB_DEFAULT_MMAP_THRESHOLD should not be defined in Starboard " \
"versions 12 and later. Instead, define kSbDefaultMmapThreshold in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#if defined(SB_FILE_MAX_NAME)
#error \
    "SB_FILE_MAX_NAME should not be defined in Starboard " \
"versions 12 and later. Instead, define kSbFileMaxName in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#if defined(SB_FILE_MAX_OPEN)
#error \
    "SB_FILE_MAX_OPEN should not be defined in Starboard " \
"versions 12 and later. Instead, define kSbFileMaxOpen in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#if defined(SB_FILE_ALT_SEP_CHAR)
#error \
    "SB_FILE_ALT_SEP_CHAR should not be defined in Starboard " \
"versions 12 and later. Instead, define kSbFileAltSepChar in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#if defined(SB_FILE_ALT_SEP_STRING)
#error \
    "SB_FILE_ALT_SEP_STRING should not be defined in Starboard " \
"versions 12 and later. Instead, define kSbFileAltSepString in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#if defined(SB_FILE_MAX_PATH)
#error \
    "SB_FILE_MAX_PATH should not be defined in Starboard " \
"versions 12 and later. Instead, define kSbFileMaxPath in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#if defined(SB_FILE_SEP_CHAR)
#error \
    "SB_FILE_SEP_CHAR should not be defined in Starboard " \
"versions 12 and later. Instead, define kSbFileSepChar in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#if defined(SB_FILE_SEP_STRING)
#error \
    "SB_FILE_SEP_STRING should not be defined in Starboard " \
"versions 12 and later. Instead, define kSbFileSepString in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#if defined(SB_HAS_AC3_AUDIO)
#error \
    "SB_HAS_AC3_AUDIO should not be defined in Starboard " \
"versions 12 and later. Instead, define kSbHasAc3Audio in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#if defined(SB_HAS_MEDIA_WEBM_VP9_SUPPORT)
#error \
    "SB_HAS_MEDIA_WEBM_VP9_SUPPORT should not be defined in Starboard " \
"versions 12 and later. Instead, define kSbHasMediaWebmVp9Support in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#if defined(SB_HAS_THREAD_PRIORITY_SUPPORT)
#error \
    "SB_HAS_THREAD_PRIORITY_SUPPORT should not be defined in Starboard " \
"versions 12 and later. Instead, define kSbHasThreadPrioritySupport in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#if defined(SB_MALLOC_ALIGNMENT)
#error \
    "SB_MALLOC_ALIGNMENT should not be defined in Starboard " \
"versions 12 and later. Instead, define kSbMallocAlignment in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#if defined(SB_MAX_THREADS)
#error \
    "SB_MAX_THREADS should not be defined in Starboard " \
"versions 12 and later. Instead, define kSbMaxThreads in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#if defined(SB_MAX_THREAD_LOCAL_KEYS)
#error \
    "SB_MAX_THREAD_LOCAL_KEYS should not be defined in Starboard " \
"versions 12 and later. Instead, define kSbMaxThreadLocalKeys in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#if defined(SB_MAX_THREAD_NAME_LENGTH)
#error \
    "SB_MAX_THREAD_NAME_LENGTH should not be defined in Starboard " \
"versions 12 and later. Instead, define kSbMaxThreadNameLength in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#if defined(SB_MEDIA_MAXIMUM_VIDEO_FRAMES)
#error \
    "SB_MEDIA_MAXIMUM_VIDEO_FRAMES should not be defined in Starboard " \
"versions 12 and later."
#endif

#if defined(SB_MEDIA_MAXIMUM_VIDEO_PREROLL_FRAMES)
#error \
    "SB_MEDIA_MAXIMUM_VIDEO_PREROLL_FRAMES should not be defined in " \
"Starboard versions 12 and later."
#endif

#if defined(SB_MEDIA_MAX_AUDIO_BITRATE_IN_BITS_PER_SECOND)
#error \
    "SB_MEDIA_MAX_AUDIO_BITRATE_IN_BITS_PER_SECOND should not be defined in " \
"Starboard versions 12 and later. Instead, define " \
"kSbMediaMaxAudioBitrateInBitsPerSecond in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#if defined(SB_MEDIA_MAX_VIDEO_BITRATE_IN_BITS_PER_SECOND)
#error \
    "SB_MEDIA_MAX_VIDEO_BITRATE_IN_BITS_PER_SECOND should not be defined in " \
"Starboard versions 12 and later. Instead, define " \
"kSbMediaMaxVideoBitrateInBitsPerSecond in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#if defined(SB_MEDIA_VIDEO_FRAME_ALIGNMENT)
#error \
    "SB_MEDIA_VIDEO_FRAME_ALIGNMENT should not be defined in Starboard " \
"versions 12 and later. Instead, define kSbMediaVideoFrameAlignment in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#if defined(SB_MEMORY_LOG_PATH)
#error \
    "SB_MEMORY_LOG_PATH should not be defined in Starboard " \
"versions 12 and later. Instead, define kSbMemoryLogPath in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#if defined(SB_MEMORY_PAGE_SIZE)
#error \
    "SB_MEMORY_PAGE_SIZE should not be defined in Starboard " \
"versions 12 and later. Instead, define kSbMemoryPageSize in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#if defined(SB_NETWORK_RECEIVE_BUFFER_SIZE)
#error \
    "SB_NETWORK_RECEIVE_BUFFER_SIZE should not be defined in Starboard " \
"versions 12 and later. Instead, define kSbNetworkReceiveBufferSize in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#if defined(SB_PATH_SEP_CHAR)
#error \
    "SB_PATH_SEP_CHAR should not be defined in Starboard " \
"versions 12 and later. Instead, define kSbPathSepChar in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#if defined(SB_PATH_SEP_STRING)
#error \
    "SB_PATH_SEP_STRING should not be defined in Starboard " \
"versions 12 and later. Instead, define kSbPathSepString in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#if defined(SB_PREFERRED_RGBA_BYTE_ORDER)
#error \
    "SB_PREFERRED_RGBA_BYTE_ORDER should not be defined in Starboard " \
"versions 12 and later. Instead, define kSbPreferredRgbaByteOrder in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#if defined(SB_USER_MAX_SIGNED_IN)
#error \
    "SB_USER_MAX_SIGNED_IN should not be defined in Starboard " \
"versions 12 and later. Instead, define kSbUserMaxSignedIn in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#else  // SB_API_VERSION >= 12

#if !defined(SB_FILE_MAX_NAME) || SB_FILE_MAX_NAME < 2
#error "Your platform must define SB_FILE_MAX_NAME > 1."
#endif

#if !defined(SB_FILE_MAX_PATH) || SB_FILE_MAX_PATH < 2
#error "Your platform must define SB_FILE_MAX_PATH > 1."
#endif

#if !defined(SB_HAS_AC3_AUDIO)
#define SB_HAS_AC3_AUDIO 1
#elif !SB_HAS(AC3_AUDIO)
#error "SB_HAS_AC3_AUDIO is required in this API version."
#endif

#if SB_API_VERSION >= 12
#if defined(SB_HAS_ASYNC_AUDIO_FRAMES_REPORTING)
#error "SB_HAS_ASYNC_AUDIO_FRAMES_REPORTING has been deprecated."
#endif  // defined(SB_HAS_ASYNC_AUDIO_FRAMES_REPORTING)
#endif

#if !defined(SB_HAS_THREAD_PRIORITY_SUPPORT)
#error "Your platform must define SB_HAS_THREAD_PRIORITY_SUPPORT."
#endif

#if !defined(SB_MAX_THREADS)
#error "Your platform must define SB_MAX_THREADS."
#endif

#if !defined(SB_MAX_THREAD_LOCAL_KEYS)
#error "Your platform must define SB_MAX_THREAD_LOCAL_KEYS."
#endif

#if !defined(SB_FILE_ALT_SEP_CHAR)
#error "Your platform must define SB_FILE_ALT_SEP_CHAR."
#endif

#if !defined(SB_FILE_ALT_SEP_STRING)
#error "Your platform must define SB_FILE_ALT_SEP_STRING."
#endif

#if !defined(SB_FILE_SEP_CHAR)
#error "Your platform must define SB_FILE_SEP_CHAR."
#endif

#if !defined(SB_FILE_SEP_STRING)
#error "Your platform must define SB_FILE_SEP_STRING."
#endif

#if !defined(SB_MAX_THREAD_NAME_LENGTH)
#error "Your platform must define SB_MAX_THREAD_NAME_LENGTH."
#endif

#if !defined(SB_PATH_SEP_CHAR)
#error "Your platform must define SB_PATH_SEP_CHAR."
#endif

#if !defined(SB_PATH_SEP_STRING)
#error "Your platform must define SB_PATH_SEP_STRING."
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

#endif  // SB_API_VERSION >= 12

#if (SB_API_VERSION < 12 && !defined(SB_HAS_MICROPHONE))
#error \
    "Your platform must define SB_HAS_MICROPHONE in API versions 11 or earlier."
#endif

#if SB_API_VERSION < 12 && !defined(SB_HAS_TIME_THREAD_NOW)
#error \
    "Your platform must define SB_HAS_TIME_THREAD_NOW in API versions 3 to 11."
#endif

#if defined(SB_IS_PLAYER_COMPOSITED) || defined(SB_IS_PLAYER_PUNCHED_OUT) || \
    defined(SB_IS_PLAYER_PRODUCING_TEXTURE)
#error "New versions of Starboard specify player output mode at runtime."
#endif

#if !defined(SB_HAS_BILINEAR_FILTERING_SUPPORT)
#error "Your platform must define SB_HAS_BILINEAR_FILTERING_SUPPORT."
#endif

#if !defined(SB_HAS_NV12_TEXTURE_SUPPORT)
#error "Your platform must define SB_HAS_NV12_TEXTURE_SUPPORT."
#endif

#if SB_API_VERSION >= 12
#if defined(SB_MUST_FREQUENTLY_FLIP_DISPLAY_BUFFER)
#error "SB_MUST_FREQUENTLY_FLIP_DISPLAY_BUFFER is deprecated."
#error "Use `CobaltExtensionGraphicsApi` instead."
#error "See [`CobaltExtensionGraphicsApi`](../extension/graphics.h)."
#endif
#else
#if !defined(SB_MUST_FREQUENTLY_FLIP_DISPLAY_BUFFER)
#error "Your platform must define SB_MUST_FREQUENTLY_FLIP_DISPLAY_BUFFER."
#endif
#endif

#if SB_API_VERSION >= 12
#if defined(COBALT_MAX_CPU_USAGE_IN_BYTES)
#error "|max_cobalt_cpu_usage| is deprecated "
#error "SbSystemGetTotalCPUMemory() instead."
#endif
#if defined(COBALT_MAX_GPU_USAGE_IN_BYTES)
#error "|max_cobalt_gpu_usage| is deprecated. "
#error "Implement SbSystemGetTotalGPUMemory() instead."
#endif
#endif  // SB_API_VERSION >= 12

#if defined(COBALT_MEDIA_BUFFER_NON_VIDEO_BUDGET)
#error "COBALT_MEDIA_BUFFER_NON_VIDEO_BUDGET is deprecated."
#error "Implement |SbMediaGetAudioBufferBudget| instead."
#endif  // defined(COBALT_MEDIA_BUFFER_NON_VIDEO_BUDGET)

#if defined(COBALT_MEDIA_BUFFER_ALIGNMENT)
#error "COBALT_MEDIA_BUFFER_ALIGNMENT is deprecated."
#error "Implement |SbMediaGetBufferAlignment| instead."
#endif  // defined(COBALT_MEDIA_BUFFER_ALIGNMENT

#if defined(COBALT_MEDIA_BUFFER_ALLOCATION_UNIT)
#error "COBALT_MEDIA_BUFFER_ALLOCATION_UNIT is deprecated."
#error "Implement |SbMediaGetBufferAllocationUnit| instead."
#endif  // defined(COBALT_MEDIA_BUFFER_ALLOCATION_UNIT

#if defined( \
    COBALT_MEDIA_SOURCE_GARBAGE_COLLECTION_DURATION_THRESHOLD_IN_SECONDS)
#error "COBALT_MEDIA_SOURCE_GARBAGE_COLLECTION_DURATION_THRESHOLD_IN_SECONDS"
#error "is deprecated. Implement"
#error "|SbMediaGetBufferGarbageCollectionDurationThreshold| instead."
#endif  // defined(
// COBALT_MEDIA_SOURCE_GARBAGE_COLLECTION_DURATION_THRESHOLD_IN_SECONDS)

#if defined(COBALT_MEDIA_BUFFER_PADDING)
#error "COBALT_MEDIA_BUFFER_PADDING is deprecated."
#error "Implement |SbMediaGetBufferPadding| instead."
#endif  // defined(COBALT_MEDIA_BUFFER_PADDING)

#if defined(COBALT_MEDIA_BUFFER_STORAGE_TYPE_FILE)
#error "COBALT_MEDIA_BUFFER_STORAGE_TYPE_FILE is deprecated."
#error "Implement |SbMediaGetBufferStorageType| instead."
#endif  // defined(COBALT_MEDIA_BUFFER_STORAGE_TYPE_FILE)

#if defined(COBALT_MEDIA_BUFFER_STORAGE_TYPE_MEMORY)
#error "COBALT_MEDIA_BUFFER_STORAGE_TYPE_MEMORY is deprecated."
#error "Implement |SbMediaGetBufferStorageType| instead."
#endif  // defined(COBALT_MEDIA_BUFFER_STORAGE_TYPE_MEMORY)

#if defined(COBALT_MEDIA_BUFFER_INITIAL_CAPACITY)
#error "COBALT_MEDIA_BUFFER_INITIAL_CAPACITY is deprecated."
#error "implement |SbMediaGetInitialBufferCapacity| instead."
#endif  // defined(COBALT_MEDIA_BUFFER_INITIAL_CAPACITY)

#if defined(COBALT_MEDIA_BUFFER_MAX_CAPACITY_1080P)
#error "COBALT_MEDIA_BUFFER_MAX_CAPACITY_1080P is deprecated."
#error "Implement |SbMediaGetMaxBufferCapacity| instead."
#endif  // defined(COBALT_MEDIA_BUFFER_MAX_CAPACITY_1080P)

#if defined(COBALT_MEDIA_BUFFER_MAX_CAPACITY_4K)
#error "COBALT_MEDIA_BUFFER_MAX_CAPACITY_4K is deprecated."
#error "Implement |SbMediaGetMaxBufferCapacity| instead."
#endif  // defined(COBALT_MEDIA_BUFFER_MAX_CAPACITY_4K)

#if defined(COBALT_MEDIA_BUFFER_PROGRESSIVE_BUDGET)
#error "COBALT_MEDIA_BUFFER_PROGRESSIVE_BUDGET is deprecated."
#error "Implement |SbMediaGetProgressiveBufferBudget| instead."
#endif  // defined(COBALT_MEDIA_BUFFER_PROGRESSIVE_BUDGET)

#if defined(COBALT_MEDIA_BUFFER_VIDEO_BUDGET_1080P)
#error "COBALT_MEDIA_BUFFER_VIDEO_BUDGET_1080P is deprecated."
#error "Implement |SbMediaGetVideoBufferBudget| instead."
#endif  // defined(COBALT_MEDIA_BUFFER_VIDEO_BUDGET_1080P)

#if defined(COBALT_MEDIA_BUFFER_VIDEO_BUDGET_4K)
#error "COBALT_MEDIA_BUFFER_VIDEO_BUDGET_4K is deprecated."
#error "Implement |SbMediaGetVideoBufferBudget| instead."
#endif  // defined(COBALT_MEDIA_BUFFER_VIDEO_BUDGET_4K)

#if defined(COBALT_MEDIA_BUFFER_POOL_ALLOCATE_ON_DEMAND)
#error "COBALT_MEDIA_BUFFER_POOL_ALLOCATE_ON_DEMAND is deprecated."
#error "Implement |SbMediaIsBufferPoolAllocateOnDemand| instead."
#endif  // defined(COBALT_MEDIA_BUFFER_POOL_ALLOCATE_ON_DEMAND)

#if defined(SB_MEDIA_SOURCE_BUFFER_STREAM_AUDIO_MEMORY_LIMIT)
#error "SB_MEDIA_SOURCE_BUFFER_STREAM_AUDIO_MEMORY_LIMIT is deprecated."
#error "Implement function |SbMediaGetAudioBufferBudget| instead."
#endif  // defined(SB_MEDIA_SOURCE_BUFFER_STREAM_AUDIO_MEMORY_LIMIT)

#if defined(SB_MEDIA_SOURCE_BUFFER_STREAM_VIDEO_MEMORY_LIMIT)
#error "SB_MEDIA_SOURCE_BUFFER_STREAM_VIDEO_MEMORY_LIMIT is deprecated."
#error "Implement function |SbMediaGetVideoBufferBudget| instead."
#endif  // defined(SB_MEDIA_SOURCE_BUFFER_STREAM_VIDEO_MEMORY_LIMIT)

#if defined(SB_MEDIA_MAIN_BUFFER_BUDGET)
#error "SB_MEDIA_MAIN_BUFFER_BUDGET is deprecated."
#endif  // defined(SB_MEDIA_MAIN_BUFFER_BUDGET)

#if defined(SB_MEDIA_GPU_BUFFER_BUDGET)
#error "SB_MEDIA_GPU_BUFFER_BUDGET is deprecated."
#endif  // defined(SB_MEDIA_GPU_BUFFER_BUDGET)

#if defined(SB_HAS_AUDIOLESS_VIDEO)
#error "SB_HAS_AUDIOLESS_VIDEO is deprecated."
#endif  // defined(SB_HAS_AUDIOLESS_VIDEO)

#if !defined(SB_HAS_MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)
#define SB_HAS_MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT 1
#elif !SB_HAS(MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)
#error \
    "SB_HAS_MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT is required in this API " \
        "version."
#endif

#if defined(SB_HAS_DRM_SESSION_CLOSED)
#error "SB_HAS_DRM_SESSION_CLOSED should not be defined for API version >= 10."
#endif  // defined(SB_HAS_DRM_SESSION_CLOSED)

#if SB_API_VERSION < 12
#if !defined(SB_HAS_ON_SCREEN_KEYBOARD)
#error "Your platform must define SB_HAS_ON_SCREEN_KEYBOARD."
#endif  // !defined(SB_HAS_ON_SCREEN_KEYBOARD)
#endif  // SB_API_VERSION < 12

#if defined(SB_HAS_PLAYER_FILTER_TESTS)
#error "SB_HAS_PLAYER_FILTER_TESTS should not be defined in API versions >= 10."
#endif  // defined(SB_HAS_PLAYER_FILTER_TESTS)

#if defined(SB_HAS_PLAYER_ERROR_MESSAGE)
#error \
    "SB_HAS_PLAYER_ERROR_MESSAGE should not be defined in API versions " \
       ">= 10."
#endif  // defined(SB_HAS_PLAYER_ERROR_MESSAGE)

#if SB_API_VERSION >= 12
#if !defined(SB_HAS_PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
#define SB_HAS_PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT 1
#elif !SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
#error \
    "SB_HAS_PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT is required in" \
    " this API version."
#endif
#endif  // SB_API_VERSION >= 12

#if SB_API_VERSION >= 12 && SB_HAS(BLITTER)
#error \
    "Blitter API is no longer supported. All blitter functions in " \
"'starboard/blitter.h' are deprecated."
#endif  // Deprecate Blitter API

#if SB_API_VERSION >= 12 && SB_HAS_QUIRK(SEEK_TO_KEYFRAME)
#error \
    "SB_HAS_QUIRK_SEEK_TO_KEYFRAME is deprecated in Starboard 12 or later." \
         " Please see configuration-public.md for more details."
#endif  // SB_API_VERSION >= 12 && SB_HAS_QUIRK(SEEK_TO_KEYFRAME)

// --- Derived Configuration -------------------------------------------------

#if SB_API_VERSION < 12

// Whether the current platform is little endian.
#if SB_IS(BIG_ENDIAN)
#define SB_IS_LITTLE_ENDIAN 0
#else
#define SB_IS_LITTLE_ENDIAN 1
#endif

#endif  // SB_API_VERSION < 12

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
#if defined(SB_GN_GL_TYPE_IS_NONE)
#define SB_HAS_GLES2 !SB_GN_GL_TYPE_IS_NONE
#else
#define SB_HAS_GLES2 !SB_GYP_GL_TYPE_IS_NONE
#endif
#endif

// --- Deprecated Feature Macros -----------------------------------------------

// Deprecated feature macros are no longer referenced by application code, and
// will be removed in a later Starboard API version. Any Starboard
// implementation that supports any of these macros should be modified to no
// longer rely on them, and operate with the assumption that their values are
// always 1.

#endif  // STARBOARD_CONFIGURATION_H_
