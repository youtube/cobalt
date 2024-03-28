// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_POINT_H_
#define UI_GFX_GEOMETRY_POINT_H_

#include <iosfwd>
#include <string>

#include "ui/gfx/geometry/point_base.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d.h"

namespace gfx {

// A point has an x and y coordinate.
class Point : public PointBase<Point, int, Vector2d> {
 public:
  Point() : PointBase<Point, int, Vector2d>(0, 0) {}
  Point(int x, int y) : PointBase<Point, int, Vector2d>(x, y) {}

  ~Point() {}

  operator PointF() const {
    return PointF(static_cast<float>(x()), static_cast<float>(y()));
  }

  // Returns a string representation of point.
  std::string ToString() const;
};

inline bool operator==(const Point& lhs, const Point& rhs) {
  return lhs.x() == rhs.x() && lhs.y() == rhs.y();
}

inline bool operator!=(const Point& lhs, const Point& rhs) {
  return !(lhs == rhs);
}

inline Point operator+(const Point& lhs, const Vector2d& rhs) {
  Point result(lhs);
  result += rhs;
  return result;
}

inline Point operator-(const Point& lhs, const Vector2d& rhs) {
  Point result(lhs);
  result -= rhs;
  return result;
}

inline Vector2d operator-(const Point& lhs, const Point& rhs) {
  return Vector2d(lhs.x() - rhs.x(), lhs.y() - rhs.y());
}

inline Point PointAtOffsetFromOrigin(const Vector2d& offset_from_origin) {
  return Point(offset_from_origin.x(), offset_from_origin.y());
}

extern template class PointBase<Point, int, Vector2d>;

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_POINT_H_
