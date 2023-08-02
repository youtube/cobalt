// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MATH_QUAD_F_H_
#define COBALT_MATH_QUAD_F_H_

#include <algorithm>
#include <cmath>
#include <iosfwd>
#include <string>

#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/point_f.h"
#include "cobalt/math/rect_f.h"

namespace cobalt {
namespace math {

// A Quad is defined by four corners, allowing it to have edges that are not
// axis-aligned, unlike a Rect.
class QuadF {
 public:
  QuadF() {}
  QuadF(const PointF& p1, const PointF& p2, const PointF& p3, const PointF& p4)
      : p1_(p1), p2_(p2), p3_(p3), p4_(p4) {}

  // Creates a quad by multiplying the corner points of the given rectangle by
  // the given matrix.
  QuadF(const Matrix3F& matrix, const RectF& rect)
      : p1_(matrix * PointF(rect.x(), rect.y())),
        p2_(matrix * PointF(rect.right(), rect.y())),
        p3_(matrix * PointF(rect.right(), rect.bottom())),
        p4_(matrix * PointF(rect.x(), rect.bottom())) {}

  explicit QuadF(const RectF& rect)
      : p1_(rect.x(), rect.y()),
        p2_(rect.right(), rect.y()),
        p3_(rect.right(), rect.bottom()),
        p4_(rect.x(), rect.bottom()) {}

  void operator=(const RectF& rect);

  void SetQuad(const PointF& p1, const PointF& p2, const PointF& p3,
               const PointF& p4) {
    set_p1(p1);
    set_p2(p2);
    set_p3(p3);
    set_p4(p4);
  }

  void set_p1(const PointF& p) { p1_ = p; }
  void set_p2(const PointF& p) { p2_ = p; }
  void set_p3(const PointF& p) { p3_ = p; }
  void set_p4(const PointF& p) { p4_ = p; }

  const PointF& p1() const { return p1_; }
  const PointF& p2() const { return p2_; }
  const PointF& p3() const { return p3_; }
  const PointF& p4() const { return p4_; }

  // Returns true if the quad is an axis-aligned rectangle.
  bool IsRectilinear() const;

  // Returns true if the points of the quad are in counter-clockwise order. This
  // assumes that the quad is convex, and that no three points are collinear.
  bool IsCounterClockwise() const;

  // Returns true if the |point| is contained within the quad, or lies on on
  // edge of the quad. This assumes that the quad is convex.
  bool Contains(const PointF& point) const;

  // Returns a rectangle that bounds the four points of the quad. The points of
  // the quad may lie on the right/bottom edge of the resulting rectangle,
  // rather than being strictly inside it.
  RectF BoundingBox() const {
    float rl = std::min(std::min(p1_.x(), p2_.x()), std::min(p3_.x(), p4_.x()));
    float rr = std::max(std::max(p1_.x(), p2_.x()), std::max(p3_.x(), p4_.x()));
    float rt = std::min(std::min(p1_.y(), p2_.y()), std::min(p3_.y(), p4_.y()));
    float rb = std::max(std::max(p1_.y(), p2_.y()), std::max(p3_.y(), p4_.y()));
    return RectF(rl, rt, rr - rl, rb - rt);
  }

  // Add a vector to the quad, offsetting each point in the quad by the vector.
  void operator+=(const Vector2dF& rhs);
  // Subtract a vector from the quad, offsetting each point in the quad by the
  // inverse of the vector.
  void operator-=(const Vector2dF& rhs);

  // Scale each point in the quad by the |scale| factor.
  void Scale(float scale) { Scale(scale, scale); }

  // Scale each point in the quad by the scale factors along each axis.
  void Scale(float x_scale, float y_scale);

  // Returns a string representation of quad.
  std::string ToString() const;

 private:
  PointF p1_;
  PointF p2_;
  PointF p3_;
  PointF p4_;
};

inline bool operator==(const QuadF& lhs, const QuadF& rhs) {
  return lhs.p1() == rhs.p1() && lhs.p2() == rhs.p2() && lhs.p3() == rhs.p3() &&
         lhs.p4() == rhs.p4();
}

inline bool operator!=(const QuadF& lhs, const QuadF& rhs) {
  return !(lhs == rhs);
}

// Add a vector to a quad, offsetting each point in the quad by the vector.
QuadF operator+(const QuadF& lhs, const Vector2dF& rhs);
// Subtract a vector from a quad, offsetting each point in the quad by the
// inverse of the vector.
QuadF operator-(const QuadF& lhs, const Vector2dF& rhs);

}  // namespace math
}  // namespace cobalt

#endif  // COBALT_MATH_QUAD_F_H_
