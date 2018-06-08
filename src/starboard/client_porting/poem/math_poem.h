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
  double d = (double)f;
  return SbDoubleFloor(d);
}

#ifdef __cplusplus
}  // extern "C"
#endif

#if !defined(POEM_NO_EMULATION)

#include <math.h>
#undef fabs
#define fabs(x) SbDoubleAbsolute(x)
#undef floor
#define floor(x) SbDoubleFloor(x)
#undef floorf
#define floorf(x) PoemSingleFloor(x)
#undef pow
#define pow(x, y) SbDoubleExponent(x, y)

#undef ceil
#define ceil(x) ceil(x)
#undef fmod
#define fmod(x, y) fmod(x, y)
#undef modf
#define modf(x, y) modf(x, y)
#undef log
#define log(x) log(x)
#undef sqrt
#define sqrt(x) sqrt(x)
#undef sin
#define sin(x) sin(x)
#undef cos
#define cos(x) cos(x)
#undef tan
#define tan(x) tan(x)
#undef atan
#define atan(x) atan(x)
#undef atan2
#define atan2(x, y) atan2(x, y)
#undef asin
#define asin(x) asin(x)
#undef acos
#define acos(x) acos(x)
#endif  // POEM_NO_EMULATION

#endif  // STARBOARD

#endif  // STARBOARD_CLIENT_PORTING_POEM_MATH_POEM_H_
