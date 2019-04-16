// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/math/insets_f.h"

#include "base/strings/stringprintf.h"

namespace cobalt {
namespace math {

template class InsetsBase<InsetsF, float>;

InsetsF::InsetsF() : InsetsBase<InsetsF, float>(0, 0, 0, 0) {}

InsetsF::InsetsF(float left, float top, float right, float bottom)
    : InsetsBase<InsetsF, float>(left, top, right, bottom) {}

InsetsF::~InsetsF() {}

std::string InsetsF::ToString() const {
  // Print members in the same order of the constructor parameters.
  if (left() == top() && left() == right() && left() == bottom()) {
    return base::StringPrintf("%.7g", left());
  } else {
    return base::StringPrintf("%.7g,%.7g,%.7g,%.7g", left(), top(), right(),
                              bottom());
  }
}

}  // namespace math
}  // namespace cobalt
