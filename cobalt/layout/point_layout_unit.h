// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_LAYOUT_POINT_LAYOUT_UNIT_H_
#define COBALT_LAYOUT_POINT_LAYOUT_UNIT_H_

#include <iosfwd>
#include <string>

#include "cobalt/layout/layout_unit.h"
#include "cobalt/layout/vector2d_layout_unit.h"
#include "cobalt/math/point_base.h"

namespace cobalt {
namespace layout {

// A LayoutUnit version of Point.
class PointLayoutUnit
    : public math::PointBase<PointLayoutUnit, LayoutUnit, Vector2dLayoutUnit> {
 public:
  PointLayoutUnit()
      : math::PointBase<PointLayoutUnit, LayoutUnit, Vector2dLayoutUnit>() {}
  PointLayoutUnit(LayoutUnit x, LayoutUnit y)
      : math::PointBase<PointLayoutUnit, LayoutUnit, Vector2dLayoutUnit>(x, y) {
  }
  ~PointLayoutUnit() {}

  void Scale(float scale) { Scale(scale, scale); }

  void Scale(float x_scale, float y_scale) {
    SetPoint(x() * x_scale, y() * y_scale);
  }

  // Returns a string representation of point.
  std::string ToString() const;
};

inline bool operator==(const PointLayoutUnit& lhs, const PointLayoutUnit& rhs) {
  return lhs.x() == rhs.x() && lhs.y() == rhs.y();
}

inline bool operator!=(const PointLayoutUnit& lhs, const PointLayoutUnit& rhs) {
  return !(lhs == rhs);
}

inline PointLayoutUnit operator+(const PointLayoutUnit& lhs,
                                 const Vector2dLayoutUnit& rhs) {
  PointLayoutUnit result(lhs);
  result += rhs;
  return result;
}

inline PointLayoutUnit operator-(const PointLayoutUnit& lhs,
                                 const Vector2dLayoutUnit& rhs) {
  PointLayoutUnit result(lhs);
  result -= rhs;
  return result;
}

inline Vector2dLayoutUnit operator-(const PointLayoutUnit& lhs,
                                    const PointLayoutUnit& rhs) {
  return Vector2dLayoutUnit(lhs.x() - rhs.x(), lhs.y() - rhs.y());
}

inline PointLayoutUnit PointAtOffsetFromOrigin(
    const Vector2dLayoutUnit& offset_from_origin) {
  return PointLayoutUnit(offset_from_origin.x(), offset_from_origin.y());
}

PointLayoutUnit ScalePoint(const PointLayoutUnit& p, float x_scale,
                           float y_scale);

inline PointLayoutUnit ScalePoint(const PointLayoutUnit& p, float scale) {
  return ScalePoint(p, scale, scale);
}

}  // namespace layout
}  // namespace cobalt

namespace cobalt {
namespace math {

extern template class PointBase<layout::PointLayoutUnit, layout::LayoutUnit,
                                layout::Vector2dLayoutUnit>;

}  // namespace math
}  // namespace cobalt
#endif  // COBALT_LAYOUT_POINT_LAYOUT_UNIT_H_
