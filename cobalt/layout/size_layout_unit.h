// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_LAYOUT_SIZE_LAYOUT_UNIT_H_
#define COBALT_LAYOUT_SIZE_LAYOUT_UNIT_H_

#include <iostream>
#include <string>

#include "cobalt/layout/layout_unit.h"
#include "cobalt/math/size_base.h"
#include "cobalt/math/size_f.h"

namespace cobalt {
namespace layout {

// A floating-point version of Size.
class SizeLayoutUnit : public math::SizeBase<SizeLayoutUnit, LayoutUnit> {
 public:
  SizeLayoutUnit() {}
  SizeLayoutUnit(LayoutUnit width, LayoutUnit height)
      : math::SizeBase<SizeLayoutUnit, LayoutUnit>(width, height) {}
  ~SizeLayoutUnit() {}

  operator math::SizeF() const {
    return math::SizeF(width().toFloat(), height().toFloat());
  }

  void Scale(float scale) { Scale(scale, scale); }

  void Scale(float x_scale, float y_scale) {
    SetSize(width() * x_scale, height() * y_scale);
  }

  std::string ToString() const;
};

inline bool operator==(const SizeLayoutUnit& lhs, const SizeLayoutUnit& rhs) {
  return lhs.width() == rhs.width() && lhs.height() == rhs.height();
}

inline bool operator!=(const SizeLayoutUnit& lhs, const SizeLayoutUnit& rhs) {
  return !(lhs == rhs);
}

SizeLayoutUnit ScaleSize(const SizeLayoutUnit& p, float x_scale, float y_scale);

inline SizeLayoutUnit ScaleSize(const SizeLayoutUnit& p, float scale) {
  return ScaleSize(p, scale, scale);
}

inline std::ostream& operator<<(std::ostream& stream,
                                const SizeLayoutUnit& size) {
  stream << "{width=" << size.width().toFloat()
         << " height=" << size.height().toFloat() << "}";
  return stream;
}

}  // namespace layout
}  // namespace cobalt

namespace cobalt {
namespace math {

extern template class SizeBase<layout::SizeLayoutUnit, layout::LayoutUnit>;

}  // namespace math
}  // namespace cobalt
#endif  // COBALT_LAYOUT_SIZE_LAYOUT_UNIT_H_
