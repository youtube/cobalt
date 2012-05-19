// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SIZE_H_
#define UI_GFX_SIZE_H_
#pragma once

#include <string>

#include "build/build_config.h"
#include "ui/base/ui_export.h"
#include "ui/gfx/size_base.h"

#if defined(OS_WIN)
typedef struct tagSIZE SIZE;
#elif defined(OS_MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace gfx {

// A size has width and height values.
class UI_EXPORT Size : public SizeBase<Size, int> {
 public:
  Size();
  Size(int width, int height);
#if defined(OS_MACOSX)
  explicit Size(const CGSize& s);
#endif

  ~Size();

#if defined(OS_MACOSX)
  Size& operator=(const CGSize& s);
#endif

#if defined(OS_WIN)
  SIZE ToSIZE() const;
#elif defined(OS_MACOSX)
  CGSize ToCGSize() const;
#endif

  std::string ToString() const;
};

#if !defined(COMPILER_MSVC)
extern template class SizeBase<Size, int>;
#endif

}  // namespace gfx

#endif  // UI_GFX_SIZE_H_
