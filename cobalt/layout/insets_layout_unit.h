// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_LAYOUT_INSETS_LAYOUT_UNIT_H_
#define COBALT_LAYOUT_INSETS_LAYOUT_UNIT_H_

#include <string>

#include "cobalt/layout/layout_unit.h"
#include "cobalt/math/insets_base.h"

namespace cobalt {
namespace layout {

// A floating-point version of Insets.
class InsetsLayoutUnit : public math::InsetsBase<InsetsLayoutUnit, LayoutUnit> {
 public:
  InsetsLayoutUnit() {}
  InsetsLayoutUnit(LayoutUnit left, LayoutUnit top, LayoutUnit right,
                   LayoutUnit bottom);
  ~InsetsLayoutUnit();

  // Returns a string representation of the insets.
  std::string ToString() const;
};

}  // namespace layout
}  // namespace cobalt

namespace cobalt {
namespace math {

extern template class InsetsBase<layout::InsetsLayoutUnit, layout::LayoutUnit>;

}  // namespace math
}  // namespace cobalt

#endif  // COBALT_LAYOUT_INSETS_LAYOUT_UNIT_H_
