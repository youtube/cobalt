// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MATH_RECT_CONVERSIONS_H_
#define COBALT_MATH_RECT_CONVERSIONS_H_

#include "cobalt/math/rect.h"
#include "cobalt/math/rect_f.h"

namespace cobalt {
namespace math {

// Returns the smallest Rect that encloses the given RectF.
Rect ToEnclosingRect(const RectF& rect);

// Returns the largest Rect that is enclosed by the given RectF.
Rect ToEnclosedRect(const RectF& rect);

// Returns the Rect after snapping the corners of the RectF to an integer grid.
// This should only be used when the RectF you provide is expected to be an
// integer rect with floating-point error. If it is an arbitrary RectF, then
// you should use a different method.
Rect ToNearestRect(const RectF& rect);

// Returns true if the Rect produced after snapping the corners of the RectF
// to an integer grid is within |distance|.
bool IsNearestRectWithinDistance(const RectF& rect, float distance);

// Returns a Rect obtained by flooring the values of the given RectF.
// Please prefer the previous two functions in new code.
Rect ToFlooredRectDeprecated(const RectF& rect);

}  // namespace math
}  // namespace cobalt

#endif  // COBALT_MATH_RECT_CONVERSIONS_H_
