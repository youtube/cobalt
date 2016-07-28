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

// Double-precision floating point helper functions.

#ifndef STARBOARD_DOUBLE_H_
#define STARBOARD_DOUBLE_H_

#include "starboard/export.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Floors double-precision floating-point number |d| to the nearest integer.
SB_EXPORT double SbDoubleFloor(const double d);

// Returns the absolute value of the given double-precision floating-point
// number |d|, preserving NaN and Infinity.
SB_EXPORT double SbDoubleAbsolute(const double d);

// Returns |base| taken to the power of |exponent|.
SB_EXPORT double SbDoubleExponent(const double base, const double exponent);

// Determines whether double-precision floating-point number |d| represents a
// fininte number.
SB_EXPORT bool SbDoubleIsFinite(const double d);

// Determines whether double-precision floating-point number |d| represents "Not
// a Number."
SB_EXPORT bool SbDoubleIsNan(const double d);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_DOUBLE_H_
