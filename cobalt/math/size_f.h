// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MATH_SIZE_F_H_
#define COBALT_MATH_SIZE_F_H_

#include <iostream>
#include <string>

#include "cobalt/math/size_base.h"

namespace cobalt {
namespace math {

// A floating-point version of Size.
class SizeF : public SizeBase<SizeF, float> {
 public:
  SizeF() : SizeBase<SizeF, float>(0, 0) {}
  SizeF(float width, float height) : SizeBase<SizeF, float>(width, height) {}
  ~SizeF() {}

  float GetArea() const { return width() * height(); }

  void Scale(float scale) { Scale(scale, scale); }

  void Scale(float x_scale, float y_scale) {
    SetSize(width() * x_scale, height() * y_scale);
  }

  std::string ToString() const;
};

inline bool operator==(const SizeF& lhs, const SizeF& rhs) {
  return lhs.width() == rhs.width() && lhs.height() == rhs.height();
}

inline bool operator!=(const SizeF& lhs, const SizeF& rhs) {
  return !(lhs == rhs);
}

SizeF ScaleSize(const SizeF& p, float x_scale, float y_scale);

inline SizeF ScaleSize(const SizeF& p, float scale) {
  return ScaleSize(p, scale, scale);
}

inline std::ostream& operator<<(std::ostream& stream, const SizeF& size) {
  stream << "{width=" << size.width() << " height=" << size.height() << "}";
  return stream;
}

extern template class SizeBase<SizeF, float>;

}  // namespace math
}  // namespace cobalt

#endif  // COBALT_MATH_SIZE_F_H_
