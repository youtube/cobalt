// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include <cinttypes>
#include <climits>
#include <cwchar>

#include "starboard/configuration.h"

namespace nplb {
namespace {

static_assert(sizeof(char) == SB_SIZE_OF_CHAR);
static_assert(sizeof(double) == SB_SIZE_OF_DOUBLE);
static_assert(sizeof(float) == SB_SIZE_OF_FLOAT);
static_assert(sizeof(int) == SB_SIZE_OF_INT);
static_assert(sizeof(int*) == SB_SIZE_OF_POINTER);
static_assert(sizeof(long) == SB_SIZE_OF_LONG);        // NOLINT(runtime/int)
static_assert(sizeof(long long) == SB_SIZE_OF_LLONG);  // NOLINT(runtime/int)
static_assert(sizeof(short) == SB_SIZE_OF_SHORT);      // NOLINT(runtime/int)

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
static_assert(sizeof(wchar_t) == 2);
#endif

#if SB_IS(WCHAR_T_UTF32)
static_assert(sizeof(wchar_t) == 4);
#endif

#if SB_IS(WCHAR_T_SIGNED)
static_assert((wchar_t)(-1) < 0);
#endif

#if SB_IS(WCHAR_T_UNSIGNED)
static_assert((wchar_t)(-1) > 0);
#endif

}  // namespace
}  // namespace nplb
