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

// Module Overview: Starboard Types module
//
// Provides a suite of standard types that should be universally available on
// all platforms, specifically focused on explicitly-sized integer types and
// booleans. This module also includes some related ubiquitous definitions like
// limits of the explicitly-sized integer types, and standard pointer and int32
// sentinel values.

#ifndef STARBOARD_TYPES_H_
#define STARBOARD_TYPES_H_

#include "starboard/configuration.h"

#if SB_HAS(STDBOOL_H) && !defined(__cplusplus)
#include <stdbool.h>
#endif  // SB_HAS(STDBOOL_H) && !defined(__cplusplus)

#if SB_HAS(STDINT_H)
#include <stdint.h>
#endif

#if SB_HAS(INTTYPES_H)
#include <inttypes.h>
#endif  // SB_HAS(STDINT_H)

#if SB_HAS(STDDEF_H)
#include <stddef.h>
#endif

#if SB_HAS(LIMITS_H)
#include <limits.h>
#endif

#if SB_HAS(STDARG_H)
#include <stdarg.h>
#endif

#if SB_HAS(FLOAT_H)
#include <float.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// --- Standard Include Emulation ----------------------------------------------

// Simulate stdint.h for platforms that don't provide it.
#if !SB_HAS(STDINT_H) && !SB_HAS(INTTYPES_H)
#if !defined(SB_INT8) || !defined(SB_UINT8) || !defined(SB_INT16) ||    \
    !defined(SB_UINT16) || !defined(SB_INT32) || !defined(SB_UINT32) || \
    !defined(SB_INT64) || !defined(SB_UINT64) || !defined(SB_INTPTR) || \
    !defined(SB_UINTPTR)
#error "No stdint.h or inttypes.h, so define SB_U?INT(8|16|32|64|PTR)."
#endif  // !defined(SB_U?INT(8|16|32|64|PTR))
typedef SB_INT8 int8_t;
typedef SB_UINT8 uint8_t;

typedef SB_INT16 int16_t;
typedef SB_UINT16 uint16_t;

typedef SB_INT32 int32_t;
typedef SB_UINT32 uint32_t;

typedef SB_INT64 int64_t;
typedef SB_UINT64 uint64_t;

typedef SB_INTPTR intptr_t;
typedef SB_UINTPTR uintptr_t;
#endif  // !SB_HAS(STDINT_H) && !SB_HAS(INTTYPES_H)

// Simulate stdbool.h for platforms that don't provide it.
#if !SB_HAS(STDBOOL_H) && !defined(__cplusplus)
#if __STDC_VERSION__ >= 199901L
#define bool _Bool
#else
#define bool int
#endif
#define false 0
#define true 1
#endif  // !SB_HAS(STDBOOL_H) && !defined(__cplusplus)

// Simulate stddef.h for platforms that don't provide it.
#if !SB_HAS(STDDEF_H)
#define NULL ((void*)0)
#endif

// Simulate needed portions of limits.h for platforms that don't provide it.

// Assume int is 32-bits until we find a platform for which that fails.
SB_COMPILE_ASSERT(sizeof(int) == sizeof(int32_t),  // NOLINT[runtime/int]
                  starboard_int_is_32_bits);
#if !defined(UINT_MAX)
#define UINT_MAX 0xFFFFFFFFU
#endif

#if !defined(INT_MIN)
#define INT_MIN 0x80000000
#endif

#if !defined(INT_MAX)
#define INT_MAX 0x7FFFFFFF
#endif

#if SB_HAS(32_BIT_LONG)

SB_COMPILE_ASSERT(sizeof(long) == sizeof(int32_t),  // NOLINT[runtime/int]
                  starboard_long_is_32_bits);
#if !defined(LONG_MIN)
#define LONG_MIN INT_MIN
#endif

#if !defined(LONG_MAX)
#define LONG_MAX INT_MAX
#endif

#if !defined(ULONG_MAX)
#define ULONG_MAX UINT_MAX
#endif

#else  // SB_HAS(32_BIT_LONG)

SB_COMPILE_ASSERT(sizeof(long) == sizeof(int64_t),  // NOLINT[runtime/int]
                  starboard_long_is_64_bits);
#if !defined(LONG_MIN)
#define LONG_MIN SB_INT64_C(0x8000000000000000)
#endif

#if !defined(LONG_MAX)
#define LONG_MAX SB_INT64_C(0x7FFFFFFFFFFFFFFF)
#endif

#if !defined(ULONG_MAX)
#define ULONG_MAX SB_INT64_C(0xFFFFFFFFFFFFFFFF)
#endif

#endif  // SB_HAS(32_BIT_LONG)

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4310)  // Cast truncates constant value.
#endif

static const int8_t kSbInt8Min = ((int8_t)0x80);
static const int8_t kSbInt8Max = ((int8_t)0x7F);
static const uint8_t kSbUInt8Max = ((uint8_t)0xFF);

static const int16_t kSbInt16Min = ((int16_t)0x8000);
static const int16_t kSbInt16Max = ((int16_t)0x7FFF);
static const uint16_t kSbUInt16Max = ((uint16_t)0xFFFF);

#define kSbInt32Min ((int32_t)0x80000000)
static const int32_t kSbInt32Max = ((int32_t)0x7FFFFFFF);
static const uint32_t kSbUInt32Max = ((uint32_t)0xFFFFFFFF);

static const int64_t kSbInt64Min = ((int64_t)SB_INT64_C(0x8000000000000000));
static const int64_t kSbInt64Max = ((int64_t)SB_INT64_C(0x7FFFFFFFFFFFFFFF));
static const uint64_t kSbUInt64Max = ((uint64_t)SB_INT64_C(0xFFFFFFFFFFFFFFFF));

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

// A value that represents an int that is probably invalid.
#define kSbInvalidInt kSbInt32Min

#if !SB_HAS(FLOAT_H)
#define DBL_MANT_DIG 53
#endif

// --- Standard Include Emulation Audits ---------------------------------------

#if !defined(UINT_MAX) || !defined(INT_MIN) || !defined(INT_MAX) || \
    !defined(LONG_MIN) || !defined(LONG_MAX)
#error "limits.h or emulation did not provide a needed limit macro."
#endif

#if (UINT_MIN + 1 == UINT_MAX - 1) || (INT_MIN + 1 == INT_MAX - 1) || \
    (LONG_MIN + 1 == LONG_MAX - 1)
// This should always evaluate to false, but ensures that the limits macros can
// be used arithmetically in the preprocessor.
#endif

#if !defined(PRId32)
#error "inttypes.h should provide the portable formatting macros."
#endif

// --- Standard Type Audits ----------------------------------------------------

#if SB_IS(WCHAR_T_UTF16)
SB_COMPILE_ASSERT(sizeof(wchar_t) == 2,
                  SB_IS_WCHAR_T_UTF16_is_inconsistent_with_sizeof_wchar_t);
#endif

#if SB_IS(WCHAR_T_UTF32)
SB_COMPILE_ASSERT(sizeof(wchar_t) == 4,
                  SB_IS_WCHAR_T_UTF32_is_inconsistent_with_sizeof_wchar_t);
#endif

#if SB_IS(WCHAR_T_SIGNED)
SB_COMPILE_ASSERT((wchar_t)(-1) < 0,
                  SB_IS_WCHAR_T_SIGNED_is_defined_incorrectly);
#endif

#if SB_IS(WCHAR_T_UNSIGNED)
SB_COMPILE_ASSERT((wchar_t)(-1) > 0,
                  SB_IS_WCHAR_T_UNSIGNED_is_defined_incorrectly);
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_TYPES_H_
