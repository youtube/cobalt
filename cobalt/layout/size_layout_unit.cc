// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/layout/size_layout_unit.h"

#include "base/stringprintf.h"

namespace cobalt {
namespace layout {

template class math::SizeBase<SizeLayoutUnit, LayoutUnit>;

std::string SizeLayoutUnit::ToString() const {
  return base::StringPrintf("%fx%f", width().toFloat(), height().toFloat());
}

SizeLayoutUnit ScaleSize(const SizeLayoutUnit& s, float x_scale,
                         float y_scale) {
  SizeLayoutUnit scaled_s(s);
  scaled_s.Scale(x_scale, y_scale);
  return scaled_s;
}

}  // namespace layout
}  // namespace cobalt
