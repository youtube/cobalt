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

// A description of the current platform in lurid detail such that common code
// never needs to actually know what the current operating system and
// architecture are. It is both very pragmatic and canonical in that if any
// application code finds itself needing to make a platform decision, it should
// always define a Starboard Configuration feature instead. This implies the
// continued existence of very narrowly-defined configuration features, but it
// retains porting control in Starboard.

#ifndef STARBOARD_CONFIGURATION_H_
#define STARBOARD_CONFIGURATION_H_

#if !defined(STARBOARD)
#error "You must define STARBOARD in Starboard builds."
#endif

// --- Common Defines --------------------------------------------------------

// The minimum API version allowed by this version of the Starboard headers,
// inclusive.
#define SB_MINIMUM_API_VERSION 1

// The maximum API version allowed by this version of the Starboard headers,
// inclusive.
#define SB_MAXIMUM_API_VERSION 1

// --- Common Detected Features ----------------------------------------------

#if defined(__GNUC__)
#define SB_IS_COMPILER_GCC 1
#elif defined(_MSC_VER)
#define SB_IS_COMPILER_MSVC 1
#endif

// --- Common Helper Macros --------------------------------------------------

// Determines a compile-time capability of the system.
#define SB_CAN(SB_FEATURE) (defined(SB_CAN_##SB_FEATURE) && SB_CAN_##SB_FEATURE)

// Determines at compile-time whether this platform has a standard feature or
// header available.
#define SB_HAS(SB_FEATURE) (defined(SB_HAS_##SB_FEATURE) && SB_HAS_##SB_FEATURE)

// Determines at compile-time an inherent aspect of this platform.
#define SB_IS(SB_FEATURE) (defined(SB_IS_##SB_FEATURE) && SB_IS_##SB_FEATURE)

// Determines at compile-time if this platform implements a given Starboard API
// version number (or above).
#define SB_VERSION(SB_API) (SB_API_VERSION >= SB_API)

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

// Tells the compiler a function is using a printf-style format string.
// |format_param| is the one-based index of the format string parameter;
// |dots_param| is the one-based index of the "..." parameter.
// For v*printf functions (which take a va_list), pass 0 for dots_param.
// (This is undocumented but matches what the system C headers do.)
// (Partially taken from base/compiler_specific.h)
#if SB_IS(COMPILER_GCC)
#define SB_PRINTF_FORMAT(format_param, dots_param) \
  __attribute__((format(printf, format_param, dots_param)))
#else
#define SB_PRINTF_FORMAT(format_param, dots_param)
#endif

// Trivially references a parameter that is otherwise unreferenced, preventing a
// compiler warning on some platforms.
#if SB_IS(COMPILER_MSVC)
#define SB_UNREFERENCED_PARAMETER(x) (x)
#else
#define SB_UNREFERENCED_PARAMETER(x) \
  do {                               \
    (void)(x);                       \
  } while (0)
#endif

// Makes a pointer-typed parameter restricted so that the compiler can make
// certain optimizations because it knows the pointers are unique.
#if defined(__cplusplus)
#if SB_IS(COMPILER_MSVC)
#define SB_RESTRICT __restrict
#elif SB_IS(COMPILER_GCC)
#define SB_RESTRICT __restrict__
#else  // COMPILER
#define SB_RESTRICT
#endif  // COMPILER
#else   // __cplusplus
#define SB_RESTRICT restrict
#endif  // __cplusplus

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

// Causes the annotated (at the end) function to generate a warning if the
// result is not accessed.
#if defined(COMPILER_GCC)
#define SB_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define SB_WARN_UNUSED_RESULT
#endif

// Declares a function as overriding a virtual function on compilers that
// support it.
#if defined(COMPILER_MSVC)
#define SB_OVERRIDE override
#elif defined(__clang__)
#define SB_OVERRIDE override
#else
#define SB_OVERRIDE
#endif

// Declare numeric literals of specific 64-bit types.
#if defined(_MSC_VER)
#define SB_INT64_C(x) x##I64
#define SB_UINT64_C(x) x##UI64
#else  // defined(_MSC_VER)
#define SB_INT64_C(x) x##LL
#define SB_UINT64_C(x) x##ULL
#endif  // defined(_MSC_VER)

// Standard CPP trick to stringify an evaluated macro definition.
#define SB_STRINGIFY(x) SB_STRINGIFY2(x)
#define SB_STRINGIFY2(x) #x

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

#if !SB_HAS(DECODER) && !SB_HAS(PLAYER)
#error "Your platform must have either a decoder or a player (or both)."
#endif

#if SB_HAS(PLAYER)
#if !SB_IS(PLAYER_COMPOSITED) && !SB_IS(PLAYER_PUNCHED_OUT) && \
    !SB_IS(PLAYER_PRODUCING_TEXTURE)
#error "Your player must choose a composition method."
#endif
#if SB_IS(PLAYER_COMPOSITED) && SB_IS(PLAYER_PUNCHED_OUT)
#error "Your player can't be both composited and punched-out."
#elif SB_IS(PLAYER_COMPOSITED) && SB_IS(PLAYER_PRODUCING_TEXTURE)
#error "Your player can't be both composited and producing a texture."
#elif SB_IS(PLAYER_PUNCHED_OUT) && SB_IS(PLAYER_PRODUCING_TEXTURE)
#error "Your player can't be both punched-out and producing a texture."
#endif
#else  // SB_HAS(PLAYER)
#if SB_IS(PLAYER_COMPOSITED) || SB_IS(PLAYER_PUNCHED_OUT) || \
    SB_IS(PLAYER_PRODUCING_TEXTURE)
#error "Your player can't have a composition method if it doesn't exist."
#endif

#if !SB_HAS(DECODER)
#error "Your platform must have either a decoder or a player."
#endif
#endif  // SB_HAS(PLAYER)

#if (SB_HAS(MANY_CORES) &&                                       \
     (SB_HAS(2_CORES) || SB_HAS(4_CORES) || SB_HAS(6_CORES))) || \
    (SB_HAS(2_CORES) && (SB_HAS(4_CORES) || SB_HAS(6_CORES))) || \
    (SB_HAS(4_CORES) && SB_HAS(6_CORES))
#error "Only one of SB_HAS_{MANY, 2, 4, 6}_CORES can be defined per platform."
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

// --- Gyp Derived Configuration -----------------------------------------------

// Specifies whether this platform has a performant OpenGL ES 2 implementation,
// which allows client applications to use GL rendering paths.  Derived from
// the gyp variable 'gl_type' which indicates what kind of GL implementation
// is available.
#if !defined(SB_HAS_GLES2)
#define SB_HAS_GLES2 !SB_GYP_GL_TYPE_IS_NONE
#endif

#endif  // STARBOARD_CONFIGURATION_H_
