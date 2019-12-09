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

#define SB_TRUE 1
#define SB_FALSE 0

// --- Common Defines --------------------------------------------------------

// The minimum API version allowed by this version of the Starboard headers,
// inclusive.
#define SB_MINIMUM_API_VERSION 6

// The maximum API version allowed by this version of the Starboard headers,
// inclusive.
#define SB_MAXIMUM_API_VERSION 12

// The API version that is currently open for changes, and therefore is not
// stable or frozen. Production-oriented ports should avoid declaring that they
// implement the experimental Starboard API version.
#define SB_EXPERIMENTAL_API_VERSION 12

// The next API version to be frozen, but is still subject to emergency
// changes. It is reasonable to base a port on the Release Candidate API
// version, but be aware that small incompatible changes may still be made to
// it.
// The following will be uncommented when an API version is a release candidate.
#define SB_RELEASE_CANDIDATE_API_VERSION 11

// --- Experimental Feature Defines ------------------------------------------

// Note that experimental feature comments will be moved into
// starboard/CHANGELOG.md when released.  Thus, you can find examples of the
// format your feature comments should take by checking that file.

// EXAMPLE:
//   // Introduce new experimental feature.
//   //   Add a function, `SbMyNewFeature()` to `starboard/feature.h` which
//   //   exposes functionality for my new feature.
//   #define SB_MY_EXPERIMENTAL_FEATURE_VERSION SB_EXPERIMENTAL_API_VERSION

// Add support for platform-based UI navigation.
// The system can be disabled by implementing the function
// `SbUiNavGetInterface()` to return `false`.  Platform-based UI navigation
// allows the platform to receive feedback on where UI elements are located and
// also lets the platform control what is selected and what the scroll
// parameters are.
#define SB_UI_NAVIGATION_VERSION SB_EXPERIMENTAL_API_VERSION

// Require the OpenGL, Blitter, and Skia renderers on all platforms.
// The system must implement `SbGetGlesInterface()` in `starboard/gles.h`
// or use the provided stub implementation, and must do the same for
// the blitter functions in `starboad/blitter.h`. The provided blitter stubs
// will return responses that denote failures. The system should implement
// `SbGetGlesInterface()` to return `nullptr` when OpenGL is not supported and
// implement `SbBlitterIsBlitterSupported()` to return false when blitter is
// not supported, as the stubs do.
#define SB_ALL_RENDERERS_REQUIRED_VERSION SB_EXPERIMENTAL_API_VERSION

// Require the captions API.
// The system must implement the captions functions in
// `starboard/accessibility.h` or use the provided stub implementations.
// System caption can be disabled by implementing the function
// `SbAccessibilityGetCaptionSettings(SbAccessibilityCaptionSettings*
// caption_settings)` to return false as the stub implementation does.
// This change also deprecates the SB_HAS_CAPTIONS flag.
#define SB_CAPTIONS_REQUIRED_VERSION SB_EXPERIMENTAL_API_VERSION

// Require compilation with Ipv6.
// Cobalt must be able to determine at runtime if the system supportes Ipv6.
// Ipv6 can be disabled by defining SB_HAS_IPV6 to 0.
#define SB_IPV6_REQUIRED_VERSION SB_EXPERIMENTAL_API_VERSION

// Require the microphone API.
// The system must implement the microphone functions in
// `starboard/microphone.h` or use the provided stub functions.
// The microphone can be disabled by having `SbMicrophoneCreate()` return
// |kSbMicrophoneInvalid|.
// This change also deprecates the SB_HAS_MICROPHONE flag.
#define SB_MICROPHONE_REQUIRED_VERSION SB_EXPERIMENTAL_API_VERSION

// Require the memory mapping API.
// The system must implement the memory mapping functions in
// `starboard/memory.h` and `starboard/shared/dlmalloc.h` or use the provided
// stub implementations.
// This change also deprecates the SB_HAS_MMAP flag.
#define SB_MMAP_REQUIRED_VERSION SB_EXPERIMENTAL_API_VERSION

// Require the on screen keyboard API.
// The system must implement the on screen keyboard functions in
// `starboard/window.h` or use the provided stub implementations.
// The on screen keyboard can be disabled by implementing the function
// `SbWindowOnScreenKeyboardIsSupported()` to return false
// as the stub implementation does.
#define SB_ON_SCREEN_KEYBOARD_REQUIRED_VERSION SB_EXPERIMENTAL_API_VERSION

// Require speech recognizer API.
// The system must implement the functions in `starboard/speech_recognizer.h`
// or use the provided stub implementations.
// The speech recognizer can be disabled by implementing the function
// `SbSpeechRecognizerIsSupported()` to return `false` as the stub
// implementation does.
#define SB_SPEECH_RECOGNIZER_REQUIRED_VERSION SB_EXPERIMENTAL_API_VERSION

// Require the speech synthesis API.
// The system must implement the speech synthesis function in
// `starboard/speech_synthesis.h` or use the provided stub implementations.
// Speech synthesis can be disabled by implementing the function
// `SbSpeechSynthesisIsSupported()` to return false as the stub
// implementation does.
#define SB_SPEECH_SYNTHESIS_REQUIRED_VERSION SB_EXPERIMENTAL_API_VERSION

// Require the time thread now API.
// The system must implement the time thread now functions in
// `starboard/time.h` or use the provided stub implementations.
// Time thread now can be disabled by implementing the function
// `SbTimeIsTimeThreadNowSupported()` to return false as the stub
// implementation does.
#define SB_TIME_THREAD_NOW_REQUIRED_VERSION SB_EXPERIMENTAL_API_VERSION

// Introduce the Starboard function SbFileAtomicReplace() to provide the ability
// to atomically replace the content of a file.
#define SB_FILE_ATOMIC_REPLACE_VERSION SB_EXPERIMENTAL_API_VERSION

// Introduces new system property kSbSystemPathStorageDirectory.
// Path to directory for permanent storage. Both read and write
// access are required.
#define SB_STORAGE_PATH_VERSION SB_EXPERIMENTAL_API_VERSION

// Deprecate the usage of SB_MUST_FREQUENTLY_FLIP_DISPLAY_BUFFER.
#define SB_MUST_FREQUENTLY_FLIP_DISPLAY_BUFFER_DEPRECATED_VERSION \
  SB_EXPERIMENTAL_API_VERSION

// The variable 'cobalt_minimum_frame_time_in_milliseconds' is deprecated
// in favor of the usage of
// 'CobaltExtensionGraphicsApi::GetMinimumFrameIntervalInMilliseconds' API.
// The declaration of 'GetMinimumFrameIntervalInMilliseconds' can be found
// in cobalt/renderer/backend/graphics_context.h
#define SB_COBALT_MINIMUM_FRAME_TIME_DEPRECATED_VERSION \
  SB_EXPERIMENTAL_API_VERSION

// Introduce Starboard Application Binary Interface (SABI) files.
//   SABI files are used to describe the configuration for targets such that two
//   targets, built with the same SABI file and varying toolchains, have
//   compatible Starboard APIs and ABIs.
//
//   With this define, we have:
//     1) Moved architecture specific defines and configurations from
//        configuration_public.h and *.gyp[i] files into SABI files.
//     2) Included the appropriate SABI file in each platform configuration.
//     3) Included the //starboard/sabi/sabi.gypi file in each platform
//        configuration which consumes SABI file fields and defines a set of
//        constants that are accessible when building.
//     4) Provided a set of tests that ensure the toolchain being used produces
//        an executable or shared library that conforms to the included SABI
//        file.
//
//  For further information on what is provided by SABI files, or how these
//  values are consumed, take a look at //starboard/sabi.
#define SB_SABI_FILE_VERSION SB_EXPERIMENTAL_API_VERSION

// Updates the API guarantees of SbMutexAcquireTry.
// SbMutexAcquireTry now has undefined behavior when it is invoked on a mutex
// that has already been locked by the calling thread. In addition, since
// SbMutexAcquireTry was used in SbMutexDestroy, SbMutexDestroy now has
// undefined behavior when invoked on a locked mutex.
#define SB_MUTEX_ACQUIRE_TRY_API_CHANGE_VERSION SB_EXPERIMENTAL_API_VERSION

// Migrate the Starboard configuration variables from macros to extern consts.
//
// The migration allows Cobalt to make platform level decisions at runtime
// instead of compile time which lets us create a more comprehensive Cobalt
// binary.
//
// This means Cobalt must remove all references to these macros that would not
// translate well to constants, i.e. in compile time references or initializing
// arrays. Therefore, we needed to change the functionality of the function
// `SbDirectoryGetNext` in "starboard/directory.h". Because we do not want to
// use variable length arrays, we pass in a c-string and length to the function
// to achieve the same result as before when passing in a `SbDirectoryEntry`.
//
// A platform will define the extern constants declared in
// "starboard/configuration_constants.h". The definitions are done in
// "starboard/<PLATFORM_PATH>/configuration_constants.cc".
//
// The exact mapping between macros and extern variables can be found in
// "starboard/shared/starboard/configuration_constants_compatibility_defines.h"
// though the naming scheme is very nearly the same: the old SB_FOO macro will
// always become the constant kSbFoo.
#define SB_FEATURE_RUNTIME_CONFIGS_VERSION SB_EXPERIMENTAL_API_VERSION

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

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define SB_DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&) = delete;         \
  void operator=(const TypeName&) = delete

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

#if SB_API_VERSION < SB_FEATURE_RUNTIME_CONFIGS_VERSION
// After SB_FEATURE_RUNTIME_CONFIGS_VERSION, we start to use runtime constants
// instead of macros for certain platform dependent configurations. This file
// substitutes configuration macros for the corresponding runtime constants so
// we don't reference these constants when they aren't defined.
#include "starboard/shared/starboard/configuration_constants_compatibility_defines.h"
#endif  // SB_API_VERSION < SB_FEATURE_RUNTIME_CONFIGS_VERSION

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

#if SB_API_VERSION >= 11
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
#else   // SB_API_VERSION >= 11

#if !defined(SB_HASH_MAP_INCLUDE)
#error "Your platform must define SB_HASH_MAP_INCLUDE."
#endif

#if !defined(SB_HASH_NAMESPACE)
#error "Your platform must define SB_HASH_NAMESPACE."
#endif

#if !defined(SB_HASH_SET_INCLUDE)
#error "Your platform must define SB_HASH_SET_INCLUDE."
#endif

#endif  // SB_API_VERSION >= 11
#if !defined(SB_FILE_MAX_PATH) || SB_FILE_MAX_PATH < 2
#error "Your platform must define SB_FILE_MAX_PATH > 1."
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

#if SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION

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

#if defined(SB_FILE_SEP_CHAR)
#error \
    "SB_FILE_SEP_CHAR should not be defined in Starboard " \
"versions 12 and later. Instead, define kSbFileSepChar in " \
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
"versions 12 and later. Instead, define kSbMediaMaximumVideoFrames in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#if defined(SB_MEDIA_MAXIMUM_VIDEO_PREROLL_FRAMES)
#error \
    "SB_MEDIA_MAXIMUM_VIDEO_PREROLL_FRAMES should not be defined in " \
"Starboard versions 12 and later. Instead, define " \
"kSbMediaMaximumVideoPrerollFrames in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
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

#if defined(SB_USER_MAX_SIGNED_IN)
#error \
    "SB_USER_MAX_SIGNED_IN should not be defined in Starboard " \
"versions 12 and later. Instead, define kSbUserMaxSignedIn in " \
"starboard/<PLATFORM_PATH>/configuration_constants.cc."
#endif

#else  // SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION

#if !defined(SB_FILE_MAX_NAME) || SB_FILE_MAX_NAME < 2
#error "Your platform must define SB_FILE_MAX_NAME > 1."
#endif

#if !defined(SB_MAX_THREADS)
#error "Your platform must define SB_MAX_THREADS."
#endif

#if !defined(SB_MAX_THREAD_LOCAL_KEYS)
#error "Your platform must define SB_MAX_THREAD_LOCAL_KEYS."
#endif

#if !defined(SB_FILE_SEP_CHAR)
#error "Your platform must define SB_FILE_SEP_CHAR."
#endif

#if !defined(SB_MAX_THREAD_NAME_LENGTH)
#error "Your platform must define SB_MAX_THREAD_NAME_LENGTH."
#endif

#endif  // SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION

#if (SB_API_VERSION < 12 && !defined(SB_HAS_MICROPHONE))
#error \
    "Your platform must define SB_HAS_MICROPHONE in API versions 11 or earlier."
#endif

#if SB_API_VERSION < SB_TIME_THREAD_NOW_REQUIRED_VERSION && \
    !defined(SB_HAS_TIME_THREAD_NOW)
#error \
    "Your platform must define SB_HAS_TIME_THREAD_NOW in API versions 3 to 11."
#endif

#if defined(SB_IS_PLAYER_COMPOSITED) || defined(SB_IS_PLAYER_PUNCHED_OUT) || \
    defined(SB_IS_PLAYER_PRODUCING_TEXTURE)
#error "New versions of Starboard specify player output mode at runtime."
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

#if SB_API_VERSION >= SB_MUST_FREQUENTLY_FLIP_DISPLAY_BUFFER_DEPRECATED_VERSION
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

#if SB_API_VERSION >= 11
#if defined(SB_HAS_MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)
#if !SB_HAS(MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)
#error \
    "SB_HAS_MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT is required in this API " \
        "version."
#endif  // !SB_HAS(MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)
#else   // defined(SB_HAS_MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)
#define SB_HAS_MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT 1
#endif  // defined(SB_HAS_MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)
#endif  // SB_API_VERSION >= 11

#if SB_API_VERSION >= 10
#if defined(SB_HAS_DRM_SESSION_CLOSED)
#if !SB_HAS(DRM_SESSION_CLOSED)
#error "SB_HAS_DRM_SESSION_CLOSED is required in this API version."
#endif  // !SB_HAS(DRM_SESSION_CLOSED)
#else   // defined(SB_HAS_DRM_SESSION_CLOSED)
#define SB_HAS_DRM_SESSION_CLOSED 1
#endif  // defined(SB_HAS_DRM_SESSION_CLOSED)
#endif  // SB_API_VERSION >= 10

#if SB_API_VERSION < SB_SPEECH_RECOGNIZER_IS_REQUIRED && SB_API_VERSION >= 5
#if !defined(SB_HAS_SPEECH_RECOGNIZER)
#error "Your platform must define SB_HAS_SPEECH_RECOGNIZER."
#endif  // !defined(SB_HAS_SPEECH_RECOGNIZER)
#endif  // SB_API_VERSION < SB_SPEECH_RECOGNIZER_IS_REQUIRED && SB_API_VERSION
        // >= 5

#if SB_API_VERSION < SB_ON_SCREEN_KEYBOARD_REQUIRED_VERSION && \
    SB_API_VERSION >= 8
#if !defined(SB_HAS_ON_SCREEN_KEYBOARD)
#error "Your platform must define SB_HAS_ON_SCREEN_KEYBOARD."
#endif  // !defined(SB_HAS_ON_SCREEN_KEYBOARD)
#endif  // SB_API_VERSION < SB_ON_SCREEN_KEYBOARD_REQUIRED_VERSION &&
        // SB_API_VERSION >= 8

#if SB_HAS(ON_SCREEN_KEYBOARD) && (SB_API_VERSION < 8)
#error "SB_HAS_ON_SCREEN_KEYBOARD not supported in this API version."
#endif

#if SB_HAS(CAPTIONS) && (SB_API_VERSION < 10)
#error "SB_HAS_CAPTIONS not supported in this API version."
#endif

#if SB_API_VERSION >= 10
#define SB_HAS_AUDIOLESS_VIDEO 1
#endif

#if SB_API_VERSION >= 10
#define SB_HAS_PLAYER_FILTER_TESTS 1
#endif

#if SB_API_VERSION >= 10
#define SB_HAS_PLAYER_ERROR_MESSAGE 1
#endif

#if SB_API_VERSION < 10
#if !SB_HAS_QUIRK(SUPPORT_INT16_AUDIO_SAMPLES)
#define SB_HAS_QUIRK_SUPPORT_INT16_AUDIO_SAMPLES 1
#endif  // !SB_HAS_QUIRK(SUPPORT_INT16_AUDIO_SAMPLES)
#endif  // SB_API_VERSION < 10

#if SB_API_VERSION >= 10
#if !defined(SB_HAS_ASYNC_AUDIO_FRAMES_REPORTING)
#error \
    "Your platform must define SB_HAS_ASYNC_AUDIO_FRAMES_REPORTING in API "\
    "version 10 or later."
#endif  // !defined(SB_HAS_ASYNC_AUDIO_FRAMES_REPORTING)
#endif  // SB_API_VERSION >= 10

#if SB_API_VERSION >= 11
#if defined(SB_HAS_AC3_AUDIO)
#if !SB_HAS(AC3_AUDIO)
#error "SB_HAS_AC3_AUDIO is required in this API version."
#endif  // !SB_HAS(AC3_AUDIO)
#else   // defined(SB_HAS_AC3_AUDIO)
#define SB_HAS_AC3_AUDIO 1
#endif  // defined(SB_HAS_AC3_AUDIO)
#endif  // SB_API_VERSION >= 11
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
#if SB_HAS(GLES2) || SB_API_VERSION >= SB_ALL_RENDERERS_REQUIRED_VERSION || \
    SB_HAS(BLITTER)
#define SB_HAS_GRAPHICS 1
#else
#define SB_HAS_GRAPHICS 0
#endif
#endif

// Deprecated feature macros
// These feature macros are deprecated in Starboard version 6 and later, and are
// no longer referenced by application code.  They will be removed in a future
// version.  Any Starboard implementation that supports Starboard version 6 or
// later should be modified to no longer depend on these macros, with the
// assumption that their values are always 1.
#if defined(SB_HAS_AUDIO_SPECIFIC_CONFIG_AS_POINTER)
#if !SB_HAS(AUDIO_SPECIFIC_CONFIG_AS_POINTER)
#error \
    "SB_HAS_AUDIO_SPECIFIC_CONFIG_AS_POINTER is required for Starboard 6 " \
       "or later."
#endif  // !SB_HAS(AUDIO_SPECIFIC_CONFIG_AS_POINTER)
#else   // defined(SB_HAS_AUDIO_SPECIFIC_CONFIG_AS_POINTER)
#define SB_HAS_AUDIO_SPECIFIC_CONFIG_AS_POINTER 1
#endif  // defined(SB_HAS_AUDIO_SPECIFIC_CONFIG_AS_POINTER)

#if defined(SB_HAS_DRM_KEY_STATUSES)
#if !SB_HAS(DRM_KEY_STATUSES)
#error "SB_HAS_DRM_KEY_STATUSES is required for Starboard 6 or later."
#endif  // !SB_HAS(DRM_KEY_STATUSES)
#else   // defined(SB_HAS_DRM_KEY_STATUSES)
#define SB_HAS_DRM_KEY_STATUSES 1
#endif  // defined(SB_HAS_DRM_KEY_STATUSES)

#endif  // STARBOARD_CONFIGURATION_H_
