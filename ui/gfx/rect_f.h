// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_RECT_F_H_
#define UI_GFX_RECT_F_H_

#include <string>

#include "ui/gfx/point_f.h"
#include "ui/gfx/rect_base.h"
#include "ui/gfx/size_f.h"
#include "ui/gfx/vector2d_f.h"

namespace gfx {

class InsetsF;

// A floating version of gfx::Rect.
class UI_EXPORT RectF
    : public RectBase<RectF, PointF, SizeF, InsetsF, Vector2dF, float> {
 public:
  RectF();
  RectF(float width, float height);
  RectF(float x, float y, float width, float height);
  explicit RectF(const gfx::SizeF& size);
  RectF(const gfx::PointF& origin, const gfx::SizeF& size);

  ~RectF();

  /// Scales the rectangle by |scale|.
  void Scale(float scale) {
    Scale(scale, scale);
  }

  void Scale(float x_scale, float y_scale) {
    SizeF newSize = size().Scale(x_scale, y_scale);
    newSize.ClampToNonNegative();
    set_origin(origin().Scale(x_scale, y_scale));
    set_size(newSize);
  }

  std::string ToString() const;
};

inline bool operator==(const RectF& lhs, const RectF& rhs) {
  return lhs.origin() == rhs.origin() && lhs.size() == rhs.size();
}

inline bool operator!=(const RectF& lhs, const RectF& rhs) {
  return !(lhs == rhs);
}

UI_EXPORT RectF IntersectRects(const RectF& a, const RectF& b);
UI_EXPORT RectF UnionRects(const RectF& a, const RectF& b);
UI_EXPORT RectF SubtractRects(const RectF& a, const RectF& b);
UI_EXPORT RectF ScaleRect(const RectF& r, float x_scale, float y_scale);

inline RectF ScaleRect(const RectF& r, float scale) {
  return ScaleRect(r, scale, scale);
}

#if !defined(COMPILER_MSVC)
extern template class RectBase<RectF, PointF, SizeF, InsetsF, Vector2dF, float>;
#endif

}  // namespace gfx

#endif  // UI_GFX_RECT_F_H_
