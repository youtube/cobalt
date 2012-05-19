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
#pragma once

#include <string>

#include "ui/gfx/point.h"
#include "ui/gfx/rect_base.h"
#include "ui/gfx/size.h"

#if defined(OS_WIN)
typedef struct tagRECT RECT;
#elif defined(TOOLKIT_GTK)
typedef struct _GdkRectangle GdkRectangle;
#endif

namespace gfx {

class Insets;

class UI_EXPORT Rect : public RectBase<Rect, Point, Size, Insets, int> {
 public:
  Rect();
  Rect(int width, int height);
  Rect(int x, int y, int width, int height);
#if defined(OS_WIN)
  explicit Rect(const RECT& r);
#elif defined(OS_MACOSX)
  explicit Rect(const CGRect& r);
#elif defined(TOOLKIT_GTK)
  explicit Rect(const GdkRectangle& r);
#endif
  explicit Rect(const gfx::Size& size);
  Rect(const gfx::Point& origin, const gfx::Size& size);

  ~Rect();

#if defined(OS_WIN)
  Rect& operator=(const RECT& r);
#elif defined(OS_MACOSX)
  Rect& operator=(const CGRect& r);
#elif defined(TOOLKIT_GTK)
  Rect& operator=(const GdkRectangle& r);
#endif

#if defined(OS_WIN)
  // Construct an equivalent Win32 RECT object.
  RECT ToRECT() const;
#elif defined(TOOLKIT_GTK)
  GdkRectangle ToGdkRectangle() const;
#elif defined(OS_MACOSX)
  // Construct an equivalent CoreGraphics object.
  CGRect ToCGRect() const;
#endif

  std::string ToString() const;
};

#if !defined(COMPILER_MSVC)
extern template class RectBase<Rect, Point, Size, Insets, int>;
#endif

}  // namespace gfx

#endif  // UI_GFX_RECT_H_
