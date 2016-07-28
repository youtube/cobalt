// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MATH_POINT_CONVERSIONS_H_
#define COBALT_MATH_POINT_CONVERSIONS_H_

#include "cobalt/math/point.h"
#include "cobalt/math/point_f.h"

namespace cobalt {
namespace math {

// Returns a Point with each component from the input PointF floored.
Point ToFlooredPoint(const PointF& point);

// Returns a Point with each component from the input PointF ceiled.
Point ToCeiledPoint(const PointF& point);

// Returns a Point with each component from the input PointF rounded.
Point ToRoundedPoint(const PointF& point);

}  // namespace math
}  // namespace cobalt

#endif  // COBALT_MATH_POINT_CONVERSIONS_H_
