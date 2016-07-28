// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MATH_VECTOR2D_CONVERSIONS_H_
#define COBALT_MATH_VECTOR2D_CONVERSIONS_H_

#include "cobalt/math/vector2d.h"
#include "cobalt/math/vector2d_f.h"

namespace cobalt {
namespace math {

// Returns a Vector2d with each component from the input Vector2dF floored.
Vector2d ToFlooredVector2d(const Vector2dF& vector2d);

// Returns a Vector2d with each component from the input Vector2dF ceiled.
Vector2d ToCeiledVector2d(const Vector2dF& vector2d);

// Returns a Vector2d with each component from the input Vector2dF rounded.
Vector2d ToRoundedVector2d(const Vector2dF& vector2d);

}  // namespace math
}  // namespace cobalt

#endif  // COBALT_MATH_VECTOR2D_CONVERSIONS_H_
