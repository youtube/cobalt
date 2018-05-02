// Copyright 2016 Google Inc. All Rights Reserved.
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

// A poem (POsix EMulation) for functions in stdlib.h

#ifndef STARBOARD_CLIENT_PORTING_POEM_STDLIB_POEM_H_
#define STARBOARD_CLIENT_PORTING_POEM_STDLIB_POEM_H_

#if defined(STARBOARD)

#include "starboard/configuration.h"

#ifdef __cplusplus
extern "C" {
#endif

static SB_C_INLINE int PoemAbs(int x) {
  if (x < 0)
    return -x;
  return x;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#if !defined(POEM_NO_EMULATION)

#include "starboard/string.h"
#include "starboard/system.h"
#ifdef __cplusplus
#include <string>
#endif

// number conversion functions
#undef strtol
#define strtol(s, o, b) SbStringParseSignedInteger(s, o, b)
#undef atoi
#define atoi(v) SbStringAToI(v)
#undef atol
#define atol(v) SbStringAToL(v)
#undef strtoul
#define strtoul(s, o, b) SbStringParseUnsignedInteger(s, o, b)
#undef strtoull
#define strtoull(s, o, b) SbStringParseUInt64(s, o, b)
#undef strtod
#define strtod(s, o) SbStringParseDouble(s, o)

#undef qsort
#define qsort(b, ec, ew, c) SbSystemSort(b, ec, ew, c);

#undef abs
#define abs(x) PoemAbs(x)

#endif  // POEM_NO_EMULATION

#endif  // STARBOARD

#endif  // STARBOARD_CLIENT_PORTING_POEM_STDLIB_POEM_H_
