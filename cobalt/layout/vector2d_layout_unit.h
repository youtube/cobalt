// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines a simple LayoutUnit based vector class.  This class is used to
// indicate a distance in two dimensions between two points. Subtracting two
// points should produce a vector, and adding a vector to a point produces the
// point at the vector's distance from the original point.

#ifndef COBALT_LAYOUT_VECTOR2D_LAYOUT_UNIT_H_
#define COBALT_LAYOUT_VECTOR2D_LAYOUT_UNIT_H_

#include <iosfwd>
#include <string>

#include "base/logging.h"
#include "cobalt/layout/layout_unit.h"
#include "cobalt/math/vector2d_f.h"

namespace cobalt {
namespace layout {

class Vector2dLayoutUnit {
 public:
  Vector2dLayoutUnit() : x_(0), y_(0) {}
  Vector2dLayoutUnit(LayoutUnit x, LayoutUnit y) : x_(x), y_(y) {}

  void SetVector(LayoutUnit x, LayoutUnit y) {
    x_ = x;
    y_ = y;
  }

  LayoutUnit x() const { return x_; }
  void set_x(LayoutUnit x) { x_ = x; }

  LayoutUnit y() const { return y_; }
  void set_y(LayoutUnit y) { y_ = y; }

  // True if both components of the vector are 0.
  bool IsZero() const;

  // Add the components of the |other| vector to the current vector.
  void Add(const Vector2dLayoutUnit& other);
  // Subtract the components of the |other| vector from the current vector.
  void Subtract(const Vector2dLayoutUnit& other);

  void operator+=(const Vector2dLayoutUnit& other) { Add(other); }
  void operator-=(const Vector2dLayoutUnit& other) { Subtract(other); }

  LayoutUnit operator[](int i) const {
    DCHECK_LE(0, i);
    DCHECK_GE(1, i);
    return i == 0 ? x_ : y_;
  }
  LayoutUnit& operator[](int i) {
    DCHECK_LE(0, i);
    DCHECK_GE(1, i);
    return i == 0 ? x_ : y_;
  }

  void SetToMin(const Vector2dLayoutUnit& other) {
    x_ = x_ <= other.x_ ? x_ : other.x_;
    y_ = y_ <= other.y_ ? y_ : other.y_;
  }

  void SetToMax(const Vector2dLayoutUnit& other) {
    x_ = x_ >= other.x_ ? x_ : other.x_;
    y_ = y_ >= other.y_ ? y_ : other.y_;
  }

  std::string ToString() const;

  operator math::Vector2dF() const {
    return math::Vector2dF(x_.toFloat(), y_.toFloat());
  }

 private:
  LayoutUnit x_;
  LayoutUnit y_;
};

inline bool operator==(const Vector2dLayoutUnit& lhs,
                       const Vector2dLayoutUnit& rhs) {
  return lhs.x() == rhs.x() && lhs.y() == rhs.y();
}

inline bool operator!=(const Vector2dLayoutUnit& lhs,
                       const Vector2dLayoutUnit& rhs) {
  return !(lhs == rhs);
}

inline Vector2dLayoutUnit operator-(const Vector2dLayoutUnit& v) {
  return Vector2dLayoutUnit(-v.x(), -v.y());
}

inline Vector2dLayoutUnit operator+(const Vector2dLayoutUnit& lhs,
                                    const Vector2dLayoutUnit& rhs) {
  Vector2dLayoutUnit result = lhs;
  result.Add(rhs);
  return result;
}

inline Vector2dLayoutUnit operator-(const Vector2dLayoutUnit& lhs,
                                    const Vector2dLayoutUnit& rhs) {
  Vector2dLayoutUnit result = lhs;
  result.Add(-rhs);
  return result;
}

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_VECTOR2D_LAYOUT_UNIT_H_
