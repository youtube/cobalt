// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_NATIVE_PIXMAP_HANDLE_H_
#define UI_GFX_NATIVE_PIXMAP_HANDLE_H_

// A reduced version of `native_pixmap_handle.h` enough to make required files
// in Chromium media to be built.

namespace gfx {

class NativePixmapHandle {
 public:
  static constexpr uint64_t kNoModifier = 0x00ffffffffffffff;
};

}  // namespace gfx

#endif  // UI_GFX_NATIVE_PIXMAP_HANDLE_H_
