// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SKIA_FONT_DELEGATE_H_
#define UI_GFX_SKIA_FONT_DELEGATE_H_

#include <memory>
#include <string>

#include "ui/gfx/font_render_params.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// Allows a Linux platform-specific overriding of font preferences.
class GFX_EXPORT SkiaFontDelegate {
 public:
  virtual ~SkiaFontDelegate() {}

  // Sets the dynamically loaded singleton that provides font preferences.
  // This pointer is not owned, and if this method is called a second time,
  // the first instance is not deleted.
  static void SetInstance(SkiaFontDelegate* instance);

  // Returns a SkiaFontDelegate instance for the toolkit used in
  // the user's desktop environment.
  //
  // Can return NULL, in case no toolkit has been set. (For example, if we're
  // running with the "--ash" flag.)
  static const SkiaFontDelegate* instance();

  // Returns the default font rendering settings.
  virtual FontRenderParams GetDefaultFontRenderParams() const = 0;

  // Returns details about the default UI font. |style_out| holds a bitfield of
  // gfx::Font::Style values.
  virtual void GetDefaultFontDescription(
      std::string* family_out,
      int* size_pixels_out,
      int* style_out,
      Font::Weight* weight_out,
      FontRenderParams* params_out) const = 0;
};

}  // namespace gfx

#endif  // UI_GFX_SKIA_FONT_DELEGATE_H_
