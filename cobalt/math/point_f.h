// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MATH_POINT_F_H_
#define COBALT_MATH_POINT_F_H_

#include <iosfwd>
#include <string>

#include "cobalt/math/point_base.h"
#include "cobalt/math/vector2d_f.h"

namespace cobalt {
namespace math {

// A floating-point version of Point.
class PointF : public PointBase<PointF, float, Vector2dF> {
 public:
  PointF() : PointBase<PointF, float, Vector2dF>(0, 0) {}
  PointF(float x, float y) : PointBase<PointF, float, Vector2dF>(x, y) {}
  ~PointF() {}

  void Scale(float scale) { Scale(scale, scale); }

  void Scale(float x_scale, float y_scale) {
    SetPoint(x() * x_scale, y() * y_scale);
  }

  // Returns a string representation of point.
  std::string ToString() const;
};

inline bool operator==(const PointF& lhs, const PointF& rhs) {
  return lhs.x() == rhs.x() && lhs.y() == rhs.y();
}

inline bool operator!=(const PointF& lhs, const PointF& rhs) {
  return !(lhs == rhs);
}

inline PointF operator+(const PointF& lhs, const Vector2dF& rhs) {
  PointF result(lhs);
  result += rhs;
  return result;
}

inline PointF operator-(const PointF& lhs, const Vector2dF& rhs) {
  PointF result(lhs);
  result -= rhs;
  return result;
}

inline Vector2dF operator-(const PointF& lhs, const PointF& rhs) {
  return Vector2dF(lhs.x() - rhs.x(), lhs.y() - rhs.y());
}

inline PointF PointAtOffsetFromOrigin(const Vector2dF& offset_from_origin) {
  return PointF(offset_from_origin.x(), offset_from_origin.y());
}

PointF ScalePoint(const PointF& p, float x_scale, float y_scale);

inline PointF ScalePoint(const PointF& p, float scale) {
  return ScalePoint(p, scale, scale);
}

extern template class PointBase<PointF, float, Vector2dF>;

}  // namespace math
}  // namespace cobalt

#endif  // COBALT_MATH_POINT_F_H_
