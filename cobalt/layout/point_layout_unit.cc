// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/layout/point_layout_unit.h"

#include "base/strings/stringprintf.h"

namespace cobalt {
namespace layout {

std::string PointLayoutUnit::ToString() const {
  return base::StringPrintf("%f,%f", x().toFloat(), y().toFloat());
}

PointLayoutUnit ScalePoint(const PointLayoutUnit& p, float x_scale,
                           float y_scale) {
  PointLayoutUnit scaled_p(p);
  scaled_p.Scale(x_scale, y_scale);
  return scaled_p;
}

}  // namespace layout
}  // namespace cobalt

namespace cobalt {
namespace math {

template class PointBase<layout::PointLayoutUnit, layout::LayoutUnit,
                         layout::Vector2dLayoutUnit>;

}  // namespace math
}  // namespace cobalt
