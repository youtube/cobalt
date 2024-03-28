// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SCOPED_UI_GRAPHICS_PUSH_CONTEXT_IOS_H_
#define UI_GFX_SCOPED_UI_GRAPHICS_PUSH_CONTEXT_IOS_H_

#import <QuartzCore/QuartzCore.h>

#include "base/macros.h"

namespace gfx {

class ScopedUIGraphicsPushContext {
 public:
  explicit ScopedUIGraphicsPushContext(CGContextRef context);

  ScopedUIGraphicsPushContext(const ScopedUIGraphicsPushContext&) = delete;
  ScopedUIGraphicsPushContext& operator=(const ScopedUIGraphicsPushContext&) =
      delete;

  ~ScopedUIGraphicsPushContext();

 private:
  CGContextRef context_;
};

}  // namespace gfx

#endif  // UI_GFX_SCOPED_UI_GRAPHICS_PUSH_CONTEXT_IOS_H_
