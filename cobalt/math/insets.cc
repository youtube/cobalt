// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/math/insets.h"

#include "base/strings/stringprintf.h"

namespace cobalt {
namespace math {

template class InsetsBase<Insets, int>;

Insets::Insets() : InsetsBase<Insets, int>(0, 0, 0, 0) {}

Insets::Insets(int left, int top, int right, int bottom)
    : InsetsBase<Insets, int>(left, top, right, bottom) {}

Insets::~Insets() {}

std::string Insets::ToString() const {
  // Print members in the same order of the constructor parameters.
  return base::StringPrintf("%d,%d,%d,%d", left(), top(), right(), bottom());
}

}  // namespace math
}  // namespace cobalt
