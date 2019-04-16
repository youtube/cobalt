// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/layout/vector2d_layout_unit.h"

#include <cmath>

#include "base/strings/stringprintf.h"

namespace cobalt {
namespace layout {

std::string Vector2dLayoutUnit::ToString() const {
  return base::StringPrintf("[%f %f]", x_.toFloat(), y_.toFloat());
}

bool Vector2dLayoutUnit::IsZero() const {
  return x_ == LayoutUnit() && y_ == LayoutUnit();
}

void Vector2dLayoutUnit::Add(const Vector2dLayoutUnit& other) {
  x_ += other.x_;
  y_ += other.y_;
}

void Vector2dLayoutUnit::Subtract(const Vector2dLayoutUnit& other) {
  x_ -= other.x_;
  y_ -= other.y_;
}

}  // namespace layout
}  // namespace cobalt
