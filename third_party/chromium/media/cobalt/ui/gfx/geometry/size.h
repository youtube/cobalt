// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_SIZE_H_
#define UI_GFX_GEOMETRY_SIZE_H_

#include <iosfwd>
#include <string>

#include "base/numerics/checked_math.h"
#include "ui/gfx/geometry/size_base.h"
#include "ui/gfx/geometry/size_f.h"

namespace gfx {

// A size has width and height values.
class Size : public SizeBase<Size, int> {
 public:
  Size() : SizeBase<Size, int>(0, 0) {}
  Size(int width, int height) : SizeBase<Size, int>(width, height) {}
  ~Size() {}

  operator SizeF() const {
    return SizeF(static_cast<float>(width()), static_cast<float>(height()));
  }

  int GetArea() const { return width() * height(); }

  base::CheckedNumeric<int> GetCheckedArea() const {
    base::CheckedNumeric<int> checked_area = width();
    checked_area *= height();
    return checked_area;
  }

  std::string ToString() const;
};

inline bool operator==(const Size& lhs, const Size& rhs) {
  return lhs.width() == rhs.width() && lhs.height() == rhs.height();
}

inline bool operator!=(const Size& lhs, const Size& rhs) {
  return !(lhs == rhs);
}

// extern template class SizeBase<Size, int>;

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_SIZE_H_
