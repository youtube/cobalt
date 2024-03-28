// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines a simple integer rectangle class.  The containment semantics
// are array-like; that is, the coordinate (x, y) is considered to be
// contained by the rectangle, but the coordinate (x + width, y) is not.
// The class will happily let you create malformed rectangles (that is,
// rectangles with negative width and/or height), but there will be assertions
// in the operations (such as Contains()) to complain in this case.

#ifndef UI_GFX_GEOMETRY_RECT_H_
#define UI_GFX_GEOMETRY_RECT_H_

#include <cmath>
#include <iosfwd>
#include <string>

#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect_base.h"
#include "ui/gfx/geometry/rect_base_impl.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

namespace gfx {

class Insets;

class Rect : public RectBase<Rect, Point, Size, Insets, Vector2d, int> {
 public:
  Rect() : RectBase<Rect, Point, Size, Insets, Vector2d, int>(Point()) {}

  Rect(int width, int height)
      : RectBase<Rect, Point, Size, Insets, Vector2d, int>(
            Size(width, height)) {}

  Rect(int x, int y, int width, int height)
      : RectBase<Rect, Point, Size, Insets, Vector2d, int>(
            Point(x, y), Size(width, height)) {}

  explicit Rect(const Size& size)
      : RectBase<Rect, Point, Size, Insets, Vector2d, int>(size) {}

  Rect(const Point& origin, const Size& size)
      : RectBase<Rect, Point, Size, Insets, Vector2d, int>(origin, size) {}

  ~Rect() {}

  std::string ToString() const;
};

inline bool operator==(const Rect& lhs, const Rect& rhs) {
  return lhs.origin() == rhs.origin() && lhs.size() == rhs.size();
}

inline bool operator!=(const Rect& lhs, const Rect& rhs) {
  return !(lhs == rhs);
}

Rect operator+(const Rect& lhs, const Vector2d& rhs);
Rect operator-(const Rect& lhs, const Vector2d& rhs);

inline std::ostream& operator<<(std::ostream& os, const Rect& rect) {
  os << rect.ToString();
  return os;
}

inline Rect operator+(const Vector2d& lhs, const Rect& rhs) {
  return rhs + lhs;
}

Rect IntersectRects(const Rect& a, const Rect& b);
Rect UnionRects(const Rect& a, const Rect& b);
Rect SubtractRects(const Rect& a, const Rect& b);

// Constructs a rectangle with |p1| and |p2| as opposite corners.
//
// This could also be thought of as "the smallest rect that contains both
// points", except that we consider points on the right/bottom edges of the
// rect to be outside the rect.  So technically one or both points will not be
// contained within the rect, because they will appear on one of these edges.
Rect BoundingRect(const Point& p1, const Point& p2);

inline Rect ScaleToEnclosingRect(const Rect& rect, float x_scale,
                                 float y_scale) {
  int x = static_cast<int>(std::floor(rect.x() * x_scale));
  int y = static_cast<int>(std::floor(rect.y() * y_scale));
  int r = rect.width() == 0
              ? x
              : static_cast<int>(std::ceil(rect.right() * x_scale));
  int b = rect.height() == 0
              ? y
              : static_cast<int>(std::ceil(rect.bottom() * y_scale));
  return Rect(x, y, r - x, b - y);
}

inline Rect ScaleToEnclosingRect(const Rect& rect, float scale) {
  return ScaleToEnclosingRect(rect, scale, scale);
}

inline Rect ScaleToEnclosedRect(const Rect& rect, float x_scale,
                                float y_scale) {
  int x = static_cast<int>(std::ceil(rect.x() * x_scale));
  int y = static_cast<int>(std::ceil(rect.y() * y_scale));
  int r = rect.width() == 0
              ? x
              : static_cast<int>(std::floor(rect.right() * x_scale));
  int b = rect.height() == 0
              ? y
              : static_cast<int>(std::floor(rect.bottom() * y_scale));
  return Rect(x, y, r - x, b - y);
}

inline Rect ScaleToEnclosedRect(const Rect& rect, float scale) {
  return ScaleToEnclosedRect(rect, scale, scale);
}

// extern template class RectBase<Rect, Point, Size, Insets, Vector2d, int>;

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_RECT_H_
