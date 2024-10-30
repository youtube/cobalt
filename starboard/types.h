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

#include <float.h>     // NOLINT
#include <inttypes.h>  // NOLINT
#include <limits.h>    // NOLINT
#include <stdarg.h>    // NOLINT
#include <stdbool.h>   // NOLINT
#include <stddef.h>    // NOLINT
#include <stdint.h>    // NOLINT

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

// A value that represents an int that is probably invalid.
#define kSbInvalidInt INT32_MIN

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
