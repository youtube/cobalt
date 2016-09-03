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

#include "starboard/configuration.h"

SB_C_INLINE int PoemAbs(int x) {
  if (x < 0)
    return -x;
  return x;
}

#if defined(POEM_FULL_EMULATION) && (POEM_FULL_EMULATION)

#include "starboard/string.h"
#include "starboard/system.h"

// number conversion functions
#define strtol(s, o, b) SbStringParseSignedInteger(s, o, b)
#define atoi(v) SbStringAToI(v)
#define atol(v) SbStringAToL(v)
#define strtol(s, o, b) SbStringParseSignedInteger(s, o, b)
#define strtoul(s, o, b) SbStringParseUnsignedInteger(s, o, b)
#define strtoull(s, o, b) SbStringParseUInt64(s, o, b)
#define strtod(s, o) SbStringParseDouble(s, o)

#define qsort(b, ec, ew, c) SbSystemSort(b, ec, ew, c);

#define abs(x) PoemAbs(x)

#endif  // POEM_FULL_EMULATION

#endif  // STARBOARD_CLIENT_PORTING_POEM_STDLIB_POEM_H_
