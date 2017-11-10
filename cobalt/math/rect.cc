// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/math/rect.h"

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "cobalt/math/clamp.h"
#include "cobalt/math/insets.h"
#include "cobalt/math/rect_base_impl.h"
#include "cobalt/math/rect_f.h"

namespace cobalt {
namespace math {

template class RectBase<Rect, Point, Size, Insets, Vector2d, int>;

typedef class RectBase<Rect, Point, Size, Insets, Vector2d, int> RectBaseT;

Rect::operator RectF() const {
  return RectF(
      static_cast<float>(origin().x()), static_cast<float>(origin().y()),
      static_cast<float>(size().width()), static_cast<float>(size().height()));
}

std::string Rect::ToString() const {
  return base::StringPrintf("%s %s", origin().ToString().c_str(),
                            size().ToString().c_str());
}

Rect Rect::RoundFromRectF(RectF rect) {
  DLOG_IF(WARNING, !rect.IsExpressibleAsRect())
      << "RectF " << rect.ToString() << " will be clamped since it is too big.";

  // Save the result in a 64-bit integer, so the result of simple arithmetic
  // operations does not overflow.
  int64_t left = ToRoundedInt(rect.x());
  int64_t bottom = ToRoundedInt(rect.bottom());
  int64_t right = ToRoundedInt(rect.right());
  int64_t top = ToRoundedInt(rect.y());

  int64_t width = right - left;
  int64_t height = bottom - top;

  const int64_t kMaxInt = std::numeric_limits<int>::max();
  const int64_t kMinInt = std::numeric_limits<int>::min();
  width = Clamp(width, kMinInt, kMaxInt);
  height = Clamp(height, kMinInt, kMaxInt);

  return Rect(static_cast<int>(left), static_cast<int>(top),
              static_cast<int>(width), static_cast<int>(height));
}

Rect operator+(const Rect& lhs, const Vector2d& rhs) {
  Rect result(lhs);
  result += rhs;
  return result;
}

Rect operator-(const Rect& lhs, const Vector2d& rhs) {
  Rect result(lhs);
  result -= rhs;
  return result;
}

Rect IntersectRects(const Rect& a, const Rect& b) {
  Rect result = a;
  result.Intersect(b);
  return result;
}

Rect UnionRects(const Rect& a, const Rect& b) {
  Rect result = a;
  result.Union(b);
  return result;
}

Rect SubtractRects(const Rect& a, const Rect& b) {
  Rect result = a;
  result.Subtract(b);
  return result;
}

Rect RoundOut(const RectF& r) {
  Point origin(std::floor(r.x()), std::floor(r.y()));

  Size size(std::ceil(r.right()) - origin.x(),
            std::ceil(r.bottom()) - origin.y());

  return Rect(origin, size);
}

Rect BoundingRect(const Point& p1, const Point& p2) {
  int rx = std::min(p1.x(), p2.x());
  int ry = std::min(p1.y(), p2.y());
  int rr = std::max(p1.x(), p2.x());
  int rb = std::max(p1.y(), p2.y());
  return Rect(rx, ry, rr - rx, rb - ry);
}

}  // namespace math
}  // namespace cobalt
