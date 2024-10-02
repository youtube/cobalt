// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLLBAR_THEME_OVERLAY_MOBILE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLLBAR_THEME_OVERLAY_MOBILE_H_

#include "base/notreached.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme_overlay.h"
#include "third_party/blink/renderer/platform/graphics/color.h"

namespace blink {

class CORE_EXPORT ScrollbarThemeOverlayMobile : public ScrollbarThemeOverlay {
 public:
  static ScrollbarThemeOverlayMobile& GetInstance();

  void PaintThumb(GraphicsContext&,
                  const Scrollbar&,
                  const gfx::Rect&) override;
  bool AllowsHitTest() const override { return false; }
  bool IsSolidColor() const override { return true; }
  bool UsesNinePatchThumbResource() const override { return false; }

 protected:
  ScrollbarThemeOverlayMobile(int thumb_thickness_default,
                              int scrollbar_margin_default,
                              int thumb_thickness_thin,
                              int scrollbar_margin_thin,
                              Color);

  ScrollbarPart HitTest(const Scrollbar&, const gfx::Point&) override {
    NOTREACHED();
    return kNoPart;
  }

 private:
  Color color_;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLLBAR_THEME_OVERLAY_MOBILE_H_
