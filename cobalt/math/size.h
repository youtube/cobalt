// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MATH_SIZE_H_
#define COBALT_MATH_SIZE_H_

#include <iosfwd>
#include <string>

#include "cobalt/math/size_base.h"
#include "cobalt/math/size_f.h"

namespace cobalt {
namespace math {

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

  std::string ToString() const;
};

inline bool operator==(const Size& lhs, const Size& rhs) {
  return lhs.width() == rhs.width() && lhs.height() == rhs.height();
}

inline bool operator!=(const Size& lhs, const Size& rhs) {
  return !(lhs == rhs);
}

extern template class SizeBase<Size, int>;

}  // namespace math
}  // namespace cobalt

#endif  // COBALT_MATH_SIZE_H_
