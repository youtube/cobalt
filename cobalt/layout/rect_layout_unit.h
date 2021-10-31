// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_LAYOUT_RECT_LAYOUT_UNIT_H_
#define COBALT_LAYOUT_RECT_LAYOUT_UNIT_H_

#include <iosfwd>
#include <string>

#include "cobalt/layout/insets_layout_unit.h"
#include "cobalt/layout/layout_unit.h"
#include "cobalt/layout/point_layout_unit.h"
#include "cobalt/layout/size_layout_unit.h"
#include "cobalt/layout/vector2d_layout_unit.h"
#include "cobalt/math/rect_base.h"

namespace cobalt {
namespace layout {

class InsetsLayoutUnit;

// A LayoutUnit version of Rect.
class RectLayoutUnit
    : public math::RectBase<RectLayoutUnit, PointLayoutUnit, SizeLayoutUnit,
                            InsetsLayoutUnit, Vector2dLayoutUnit, LayoutUnit> {
 public:
  RectLayoutUnit()
      : math::RectBase<RectLayoutUnit, PointLayoutUnit, SizeLayoutUnit,
                       InsetsLayoutUnit, Vector2dLayoutUnit, LayoutUnit>(
            SizeLayoutUnit()) {}

  RectLayoutUnit(LayoutUnit width, LayoutUnit height)
      : math::RectBase<RectLayoutUnit, PointLayoutUnit, SizeLayoutUnit,
                       InsetsLayoutUnit, Vector2dLayoutUnit, LayoutUnit>(
            SizeLayoutUnit(width, height)) {}

  RectLayoutUnit(LayoutUnit x, LayoutUnit y, LayoutUnit width,
                 LayoutUnit height)
      : math::RectBase<RectLayoutUnit, PointLayoutUnit, SizeLayoutUnit,
                       InsetsLayoutUnit, Vector2dLayoutUnit, LayoutUnit>(
            PointLayoutUnit(x, y), SizeLayoutUnit(width, height)) {}

  explicit RectLayoutUnit(const SizeLayoutUnit& size)
      : math::RectBase<RectLayoutUnit, PointLayoutUnit, SizeLayoutUnit,
                       InsetsLayoutUnit, Vector2dLayoutUnit, LayoutUnit>(size) {
  }

  RectLayoutUnit(const PointLayoutUnit& origin, const SizeLayoutUnit& size)
      : math::RectBase<RectLayoutUnit, PointLayoutUnit, SizeLayoutUnit,
                       InsetsLayoutUnit, Vector2dLayoutUnit, LayoutUnit>(
            origin, size) {}

  ~RectLayoutUnit() {}

  // Scales the rectangle by |scale|.
  void Scale(float scale) { Scale(scale, scale); }

  void Scale(float x_scale, float y_scale) {
    set_origin(ScalePoint(origin(), x_scale, y_scale));
    set_size(ScaleSize(size(), x_scale, y_scale));
  }

  // This method reports if the RectLayoutUnit can be safely converted to an
  // integer
  // Rect. When it is false, some dimension of the RectLayoutUnit is outside the
  // bounds
  // of what an integer can represent, and converting it to a Rect will require
  // clamping.
  bool IsExpressibleAsRect() const;

  std::string ToString() const;
};

inline bool operator==(const RectLayoutUnit& lhs, const RectLayoutUnit& rhs) {
  return lhs.origin() == rhs.origin() && lhs.size() == rhs.size();
}

inline bool operator!=(const RectLayoutUnit& lhs, const RectLayoutUnit& rhs) {
  return !(lhs == rhs);
}

inline RectLayoutUnit operator+(const RectLayoutUnit& lhs,
                                const Vector2dLayoutUnit& rhs) {
  return RectLayoutUnit(lhs.x() + rhs.x(), lhs.y() + rhs.y(), lhs.width(),
                        lhs.height());
}

inline RectLayoutUnit operator-(const RectLayoutUnit& lhs,
                                const Vector2dLayoutUnit& rhs) {
  return RectLayoutUnit(lhs.x() - rhs.x(), lhs.y() - rhs.y(), lhs.width(),
                        lhs.height());
}

inline RectLayoutUnit operator+(const Vector2dLayoutUnit& lhs,
                                const RectLayoutUnit& rhs) {
  return rhs + lhs;
}

RectLayoutUnit IntersectRects(const RectLayoutUnit& a, const RectLayoutUnit& b);
RectLayoutUnit UnionRects(const RectLayoutUnit& a, const RectLayoutUnit& b);
RectLayoutUnit SubtractRects(const RectLayoutUnit& a, const RectLayoutUnit& b);

inline RectLayoutUnit ScaleRect(const RectLayoutUnit& r, float x_scale,
                                float y_scale) {
  return RectLayoutUnit(r.x() * x_scale, r.y() * y_scale, r.width() * x_scale,
                        r.height() * y_scale);
}

inline RectLayoutUnit ScaleRect(const RectLayoutUnit& r, float scale) {
  return ScaleRect(r, scale, scale);
}

// Constructs a rectangle with |p1| and |p2| as opposite corners.
//
// This could also be thought of as "the smallest rect that contains both
// points", except that we consider points on the right/bottom edges of the
// rect to be outside the rect.  So technically one or both points will not be
// contained within the rect, because they will appear on one of these edges.
RectLayoutUnit BoundingRect(const PointLayoutUnit& p1,
                            const PointLayoutUnit& p2);

}  // namespace layout
}  // namespace cobalt

namespace cobalt {
namespace math {

extern template class RectBase<layout::RectLayoutUnit, layout::PointLayoutUnit,
                               layout::SizeLayoutUnit, layout::InsetsLayoutUnit,
                               layout::Vector2dLayoutUnit, layout::LayoutUnit>;

}  // namespace math
}  // namespace cobalt
#endif  // COBALT_LAYOUT_RECT_LAYOUT_UNIT_H_
