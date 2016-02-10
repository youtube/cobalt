// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MATH_INSETS_H_
#define COBALT_MATH_INSETS_H_

#include <string>

#include "cobalt/math/insets_base.h"
#include "cobalt/math/insets_f.h"

namespace cobalt {
namespace math {

// An integer version of Insets.
class Insets : public InsetsBase<Insets, int> {
 public:
  Insets();
  Insets(int left, int top, int right, int bottom);

  ~Insets();

  Insets Scale(float scale) const { return Scale(scale, scale); }

  Insets Scale(float x_scale, float y_scale) const {
    return Insets(static_cast<int>(left() * x_scale),
                  static_cast<int>(top() * y_scale),
                  static_cast<int>(right() * x_scale),
                  static_cast<int>(bottom() * y_scale));
  }

  operator InsetsF() const { return InsetsF(left(), top(), right(), bottom()); }

  // Returns a string representation of the insets.
  std::string ToString() const;
};

extern template class InsetsBase<Insets, int>;

}  // namespace math
}  // namespace cobalt

#endif  // COBALT_MATH_INSETS_H_
