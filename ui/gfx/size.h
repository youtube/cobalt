// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SIZE_H_
#define UI_GFX_SIZE_H_
#pragma once

#include "build/build_config.h"

#include <iosfwd>

#if defined(OS_WIN)
typedef struct tagSIZE SIZE;
#elif defined(OS_MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace gfx {

// A size has width and height values.
class Size {
 public:
  Size() : width_(0), height_(0) {}
  Size(int width, int height);
#if defined(OS_MACOSX)
  explicit Size(const CGSize& s);
#endif

  ~Size() {}

#if defined(OS_MACOSX)
  Size& operator=(const CGSize& s);
#endif

  int width() const { return width_; }
  int height() const { return height_; }

  int GetArea() const { return width_ * height_; }

  void SetSize(int width, int height) {
    set_width(width);
    set_height(height);
  }

  void Enlarge(int width, int height) {
    set_width(width_ + width);
    set_height(height_ + height);
  }

  void set_width(int width);
  void set_height(int height);

  bool operator==(const Size& s) const {
    return width_ == s.width_ && height_ == s.height_;
  }

  bool operator!=(const Size& s) const {
    return !(*this == s);
  }

  bool IsEmpty() const {
    // Size doesn't allow negative dimensions, so testing for 0 is enough.
    return (width_ == 0) || (height_ == 0);
  }

#if defined(OS_WIN)
  SIZE ToSIZE() const;
#elif defined(OS_MACOSX)
  CGSize ToCGSize() const;
#endif

 private:
  int width_;
  int height_;
};

std::ostream& operator<<(std::ostream& out, const gfx::Size& s);

}  // namespace gfx

#endif  // UI_GFX_SIZE_H_
