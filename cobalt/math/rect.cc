// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/math/rect.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "cobalt/math/insets.h"
#include "cobalt/math/rect_base_impl.h"

namespace cobalt {
namespace math {

template class RectBase<Rect, Point, Size, Insets, Vector2d, int>;

typedef class RectBase<Rect, Point, Size, Insets, Vector2d, int> RectBaseT;

std::string Rect::ToString() const {
  return base::StringPrintf("%s %s", origin().ToString().c_str(),
                            size().ToString().c_str());
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
