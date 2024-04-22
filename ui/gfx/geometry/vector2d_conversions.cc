// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/vector2d_conversions.h"

#include "base/numerics/safe_conversions.h"

namespace gfx {

Vector2d ToFlooredVector2d(const Vector2dF& vector2d) {
  return Vector2d(base::ClampFloor(vector2d.x()),
                  base::ClampFloor(vector2d.y()));
}

Vector2d ToCeiledVector2d(const Vector2dF& vector2d) {
  return Vector2d(base::ClampCeil(vector2d.x()), base::ClampCeil(vector2d.y()));
}

Vector2d ToRoundedVector2d(const Vector2dF& vector2d) {
  return Vector2d(base::ClampRound(vector2d.x()),
                  base::ClampRound(vector2d.y()));
}

}  // namespace gfx

