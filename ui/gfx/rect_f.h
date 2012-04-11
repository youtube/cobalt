// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_RECT_F_H_
#define UI_GFX_RECT_F_H_
#pragma once

#include <string>

#include "ui/gfx/point_f.h"
#include "ui/gfx/size_f.h"

#if !defined(ENABLE_DIP)
#error "This class should be used only when DIP feature is enabled"
#endif

namespace gfx {

class InsetsF;

// A floating versino of gfx::Rect. This is copied, instead of using
// template, to avoid conflict with m19 branch.
// TODO(oshima): Merge this to ui/gfx/rect.h using template.
class UI_EXPORT RectF {
 public:
  RectF();
  RectF(float width, float height);
  RectF(float x, float y, float width, float height);
  explicit RectF(const gfx::SizeF& size);
  RectF(const gfx::PointF& origin, const gfx::SizeF& size);

  ~RectF();

  float x() const { return origin_.x(); }
  void set_x(float x) { origin_.set_x(x); }

  float y() const { return origin_.y(); }
  void set_y(float y) { origin_.set_y(y); }

  float width() const { return size_.width(); }
  void set_width(float width) { size_.set_width(width); }

  float height() const { return size_.height(); }
  void set_height(float height) { size_.set_height(height); }

  const gfx::PointF& origin() const { return origin_; }
  void set_origin(const gfx::PointF& origin) { origin_ = origin; }

  const gfx::SizeF& size() const { return size_; }
  void set_size(const gfx::SizeF& size) { size_ = size; }

  float right() const { return x() + width(); }
  float bottom() const { return y() + height(); }

  void SetRect(float x, float y, float width, float height);

  void Inset(float horizontal, float vertical) {
    Inset(horizontal, vertical, horizontal, vertical);
  }
  // Shrink the rectangle by the given insets.
  void Inset(const gfx::InsetsF& insets);

  // Shrink the rectangle by the specified amount on each side.
  void Inset(float left, float top, float right, float bottom);

  // Move the rectangle by a horizontal and vertical distance.
  void Offset(float horizontal, float vertical);
  void Offset(const gfx::PointF& point) {
    Offset(point.x(), point.y());
  }

  // Returns true if the area of the rectangle is zero.
  bool IsEmpty() const { return size_.IsEmpty(); }

  bool operator==(const RectF& other) const;

  bool operator!=(const RectF& other) const {
    return !(*this == other);
  }

  // A rect is less than another rect if its origin is less than
  // the other rect's origin. If the origins are equal, then the
  // shortest rect is less than the other. If the origin and the
  // height are equal, then the narrowest rect is less than.
  // This comparison is required to use Rects in sets, or sorted
  // vectors.
  bool operator<(const RectF& other) const;

  // Returns true if the point identified by point_x and point_y falls inside
  // this rectangle.  The point (x, y) is inside the rectangle, but the
  // point (x + width, y + height) is not.
  bool Contains(float point_x, float point_y) const;

  // Returns true if the specified point is contained by this rectangle.
  bool Contains(const gfx::PointF& point) const {
    return Contains(point.x(), point.y());
  }

  // Returns true if this rectangle contains the specified rectangle.
  bool Contains(const RectF& rect) const;

  // Returns true if this rectangle intersects the specified rectangle.
  bool Intersects(const RectF& rect) const;

  // Computes the intersection of this rectangle with the given rectangle.
  RectF Intersect(const RectF& rect) const;

  // Computes the union of this rectangle with the given rectangle.  The union
  // is the smallest rectangle containing both rectangles.
  RectF Union(const RectF& rect) const;

  // Computes the rectangle resulting from subtracting |rect| from |this|.  If
  // |rect| does not intersect completely in either the x- or y-direction, then
  // |*this| is returned.  If |rect| contains |this|, then an empty Rect is
  // returned.
  RectF Subtract(const RectF& rect) const;

  // Returns true if this rectangle equals that of the supplied rectangle.
  bool Equals(const RectF& rect) const {
    return *this == rect;
  }

  // Fits as much of the receiving rectangle into the supplied rectangle as
  // possible, returning the result. For example, if the receiver had
  // a x-location of 2 and a width of 4, and the supplied rectangle had
  // an x-location of 0 with a width of 5, the returned rectangle would have
  // an x-location of 1 with a width of 4.
  RectF AdjustToFit(const RectF& rect) const;

  // Returns the center of this rectangle.
  PointF CenterPoint() const;

  // Return a rectangle that has the same center point but with a size capped
  // at given |size|.
  RectF Center(const gfx::SizeF& size) const;

  // Splits |this| in two halves, |left_half| and |right_half|.
  void SplitVertically(gfx::RectF* left_half, gfx::RectF* right_half) const;

  // Returns true if this rectangle shares an entire edge (i.e., same width or
  // same height) with the given rectangle, and the rectangles do not overlap.
  bool SharesEdgeWith(const gfx::RectF& rect) const;

  std::string ToString() const;

 private:
  gfx::PointF origin_;
  gfx::SizeF size_;
};

}  // namespace gfx

#endif  // UI_GFX_RECT_H_
