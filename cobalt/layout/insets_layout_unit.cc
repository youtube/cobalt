// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/layout/insets_layout_unit.h"

#include "base/strings/stringprintf.h"

namespace cobalt {
namespace layout {

InsetsLayoutUnit::InsetsLayoutUnit(LayoutUnit left, LayoutUnit top,
                                   LayoutUnit right, LayoutUnit bottom)
    : math::InsetsBase<InsetsLayoutUnit, LayoutUnit>(left, top, right, bottom) {
}

InsetsLayoutUnit::~InsetsLayoutUnit() {}

std::string InsetsLayoutUnit::ToString() const {
  // Print members in the same order of the constructor parameters.
  if (left() == top() && left() == right() && left() == bottom()) {
    return base::StringPrintf("%.7g", left().toFloat());
  } else {
    return base::StringPrintf("%.7g,%.7g,%.7g,%.7g", left().toFloat(),
                              top().toFloat(), right().toFloat(),
                              bottom().toFloat());
  }
}

}  // namespace layout
}  // namespace cobalt

namespace cobalt {
namespace math {

template class InsetsBase<layout::InsetsLayoutUnit, layout::LayoutUnit>;

}  // namespace math
}  // namespace cobalt
