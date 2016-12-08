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

// A poem (POsix EMulation) for functions in string.h

#ifndef STARBOARD_CLIENT_PORTING_POEM_MATH_POEM_H_
#define STARBOARD_CLIENT_PORTING_POEM_MATH_POEM_H_

#if defined(STARBOARD)

#include "starboard/double.h"

#ifdef __cplusplus
extern "C" {
#endif

// Takes floor of a float |f|.  Meant to be a drop-in replacement for |floorf|
static SB_C_INLINE float PoemSingleFloor(const float f) {
  double d(f);
  return SbDoubleFloor(d);
}

#ifdef __cplusplus
}  // extern "C"
#endif

#if !defined(POEM_NO_EMULATION)

#define fabs(x) SbDoubleAbsolute(x)
#define floor(x) SbDoubleFloor(x)
#define floorf(x) PoemSingleFloor(x)
#define pow(x, y) SbDoubleExponent(x, y)

#include <math.h>
#define ceil(x) ceil(x)
#define fmod(x, y) fmod(x, y)
#define modf(x, y) modf(x, y)
#define log(x) log(x)
#define sqrt(x) sqrt(x)
#define sin(x) sin(x)
#define cos(x) cos(x)
#define tan(x) tan(x)
#define atan(x) atan(x)
#define atan2(x, y) atan2(x, y)
#define asin(x) asin(x)
#define acos(x) acos(x)
#endif  // POEM_NO_EMULATION

#endif  // STARBOARD

#endif  // STARBOARD_CLIENT_PORTING_POEM_MATH_POEM_H_
