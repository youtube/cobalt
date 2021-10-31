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

// The C library used must provide these headers to be standard conforming.

#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if SB_HAS(SYS_TYPES_H)
#include <sys/types.h>
#endif  // SB_HAS(SYS_TYPES_H)

#ifdef __cplusplus
extern "C" {
#endif

// --- Standard Include Emulation ----------------------------------------------

#if !SB_HAS(SSIZE_T)
#if SB_IS(32_BIT)
typedef int32_t ssize_t;
#define SSIZE_MAX INT_MAX
#elif SB_IS(64_BIT)
typedef int64_t ssize_t;
#define SSIZE_MAX LONG_MAX
#endif
#endif  // !SB_HAS(SSIZE_T)

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4310)  // Cast truncates constant value.
#endif

// Simulate needed portions of limits.h for platforms that don't provide it.

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

// --- Standard Include Emulation Audits ---------------------------------------

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
