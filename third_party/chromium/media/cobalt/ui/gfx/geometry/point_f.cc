// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(STARBOARD)
#include "third_party/chromium/media/cobalt/ui/gfx/geometry/point_f.h"
#else
#include "ui/gfx/geometry/point_f.h"
#endif  // defined(STARBOARD)

#include "base/strings/stringprintf.h"

namespace gfx {

template class PointBase<PointF, float, Vector2dF>;

std::string PointF::ToString() const {
  return base::StringPrintf("%f,%f", x(), y());
}

PointF ScalePoint(const PointF& p, float x_scale, float y_scale) {
  PointF scaled_p(p);
  scaled_p.Scale(x_scale, y_scale);
  return scaled_p;
}

}  // namespace gfx
