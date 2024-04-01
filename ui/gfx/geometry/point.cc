// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/point.h"

#include "base/strings/stringprintf.h"

namespace gfx {

template class PointBase<Point, int, Vector2d>;

std::string Point::ToString() const {
  return base::StringPrintf("%d,%d", x(), y());
}

}  // namespace gfx
