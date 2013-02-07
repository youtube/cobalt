// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines a simple integer rectangle class.  The containment semantics
// are array-like; that is, the coordinate (x, y) is considered to be
// contained by the rectangle, but the coordinate (x + width, y) is not.
// The class will happily let you create malformed rectangles (that is,
// rectangles with negative width and/or height), but there will be assertions
// in the operations (such as Contains()) to complain in this case.

#ifndef UI_GFX_RECT_H_
#define UI_GFX_RECT_H_

#include <string>

#include "ui/gfx/point.h"
#include "ui/gfx/rect_base.h"
#include "ui/gfx/rect_f.h"
#include "ui/gfx/size.h"
#include "ui/gfx/vector2d.h"

#if defined(OS_WIN)
typedef struct tagRECT RECT;
#elif defined(TOOLKIT_GTK)
typedef struct _GdkRectangle GdkRectangle;
#elif defined(OS_IOS)
#include <CoreGraphics/CoreGraphics.h>
#elif defined(OS_MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace gfx {

class Insets;

class UI_EXPORT Rect
    : public RectBase<Rect, Point, Size, Insets, Vector2d, int> {
 public:
  Rect() : RectBase<Rect, Point, Size, Insets, Vector2d, int>(Point()) {}

  Rect(int width, int height)
      : RectBase<Rect, Point, Size, Insets, Vector2d, int>
            (Size(width, height)) {}

  Rect(int x, int y, int width, int height)
      : RectBase<Rect, Point, Size, Insets, Vector2d, int>
            (Point(x, y), Size(width, height)) {}

#if defined(OS_WIN)
  explicit Rect(const RECT& r);
#elif defined(OS_MACOSX)
  explicit Rect(const CGRect& r);
#elif defined(TOOLKIT_GTK)
  explicit Rect(const GdkRectangle& r);
#endif

  explicit Rect(const gfx::Size& size)
      : RectBase<Rect, Point, Size, Insets, Vector2d, int>(size) {}

  Rect(const gfx::Point& origin, const gfx::Size& size)
      : RectBase<Rect, Point, Size, Insets, Vector2d, int>(origin, size) {}

  ~Rect() {}

#if defined(OS_WIN)
  // Construct an equivalent Win32 RECT object.
  RECT ToRECT() const;
#elif defined(TOOLKIT_GTK)
  GdkRectangle ToGdkRectangle() const;
#elif defined(OS_MACOSX)
  // Construct an equivalent CoreGraphics object.
  CGRect ToCGRect() const;
#endif

  operator RectF() const {
    return RectF(origin().x(), origin().y(), size().width(), size().height());
  }

  std::string ToString() const;
};

inline bool operator==(const Rect& lhs, const Rect& rhs) {
  return lhs.origin() == rhs.origin() && lhs.size() == rhs.size();
}

inline bool operator!=(const Rect& lhs, const Rect& rhs) {
  return !(lhs == rhs);
}

UI_EXPORT Rect operator+(const Rect& lhs, const Vector2d& rhs);
UI_EXPORT Rect operator-(const Rect& lhs, const Vector2d& rhs);

inline Rect operator+(const Vector2d& lhs, const Rect& rhs) {
  return rhs + lhs;
}

UI_EXPORT Rect IntersectRects(const Rect& a, const Rect& b);
UI_EXPORT Rect UnionRects(const Rect& a, const Rect& b);
UI_EXPORT Rect SubtractRects(const Rect& a, const Rect& b);

// Constructs a rectangle with |p1| and |p2| as opposite corners.
//
// This could also be thought of as "the smallest rect that contains both
// points", except that we consider points on the right/bottom edges of the
// rect to be outside the rect.  So technically one or both points will not be
// contained within the rect, because they will appear on one of these edges.
UI_EXPORT Rect BoundingRect(const Point& p1, const Point& p2);

#if !defined(COMPILER_MSVC)
extern template class RectBase<Rect, Point, Size, Insets, Vector2d, int>;
#endif

}  // namespace gfx

#endif  // UI_GFX_RECT_H_
