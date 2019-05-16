// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/layout/rect_layout_unit.h"

#include <algorithm>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "cobalt/layout/insets_layout_unit.h"
#include "cobalt/layout/layout_unit.h"
#include "cobalt/layout/point_layout_unit.h"
#include "cobalt/layout/size_layout_unit.h"
#include "cobalt/layout/vector2d_layout_unit.h"
#include "cobalt/math/rect_base_impl.h"
#include "cobalt/math/safe_integer_conversions.h"

namespace cobalt {
namespace layout {

typedef class math::RectBase<RectLayoutUnit, PointLayoutUnit, SizeLayoutUnit,
                             InsetsLayoutUnit, Vector2dLayoutUnit,
                             LayoutUnit> RectBaseT;

bool RectLayoutUnit::IsExpressibleAsRect() const {
  return math::IsExpressibleAsInt(x().toFloat()) &&
         math::IsExpressibleAsInt(y().toFloat()) &&
         math::IsExpressibleAsInt(width().toFloat()) &&
         math::IsExpressibleAsInt(height().toFloat()) &&
         math::IsExpressibleAsInt(right().toFloat()) &&
         math::IsExpressibleAsInt(bottom().toFloat());
}

std::string RectLayoutUnit::ToString() const {
  return base::StringPrintf("%s %s", origin().ToString().c_str(),
                            size().ToString().c_str());
}

RectLayoutUnit IntersectRects(const RectLayoutUnit& a,
                              const RectLayoutUnit& b) {
  RectLayoutUnit result = a;
  result.Intersect(b);
  return result;
}

RectLayoutUnit UnionRects(const RectLayoutUnit& a, const RectLayoutUnit& b) {
  RectLayoutUnit result = a;
  result.Union(b);
  return result;
}

RectLayoutUnit SubtractRects(const RectLayoutUnit& a, const RectLayoutUnit& b) {
  RectLayoutUnit result = a;
  result.Subtract(b);
  return result;
}

RectLayoutUnit BoundingRect(const PointLayoutUnit& p1,
                            const PointLayoutUnit& p2) {
  LayoutUnit rx(std::min(p1.x(), p2.x()));
  LayoutUnit ry(std::min(p1.y(), p2.y()));
  LayoutUnit rr(std::max(p1.x(), p2.x()));
  LayoutUnit rb(std::max(p1.y(), p2.y()));
  return RectLayoutUnit(rx, ry, rr - rx, rb - ry);
}

}  // namespace layout
}  // namespace cobalt

namespace cobalt {
namespace math {

template class math::RectBase<layout::RectLayoutUnit, layout::PointLayoutUnit,
                              layout::SizeLayoutUnit, layout::InsetsLayoutUnit,
                              layout::Vector2dLayoutUnit, layout::LayoutUnit>;

}  // namespace math
}  // namespace cobalt
