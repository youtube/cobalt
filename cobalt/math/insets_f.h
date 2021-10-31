// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MATH_INSETS_F_H_
#define COBALT_MATH_INSETS_F_H_

#include <string>

#include "cobalt/math/insets_base.h"

namespace cobalt {
namespace math {

// A floating-point version of Insets.
class InsetsF : public InsetsBase<InsetsF, float> {
 public:
  InsetsF();
  InsetsF(float left, float top, float right, float bottom);
  ~InsetsF();

  InsetsF Scale(float scale) const { return Scale(scale, scale); }

  InsetsF Scale(float x_scale, float y_scale) const {
    return InsetsF(left() * x_scale, top() * y_scale, right() * x_scale,
                   bottom() * y_scale);
  }

  // Returns a string representation of the insets.
  std::string ToString() const;
};

extern template class InsetsBase<InsetsF, float>;

}  // namespace math
}  // namespace cobalt

#endif  // COBALT_MATH_INSETS_F_H_
