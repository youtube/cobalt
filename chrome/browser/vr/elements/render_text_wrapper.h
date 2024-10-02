// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_RENDER_TEXT_WRAPPER_H_
#define CHROME_BROWSER_VR_ELEMENTS_RENDER_TEXT_WRAPPER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "ui/gfx/render_text.h"

namespace vr {

// A minimal, mockable wrapper around gfx::RenderText, to facilitate testing of
// RenderText users.
class VR_UI_EXPORT RenderTextWrapper {
 public:
  explicit RenderTextWrapper(gfx::RenderText* render_text);

  RenderTextWrapper(const RenderTextWrapper&) = delete;
  RenderTextWrapper& operator=(const RenderTextWrapper&) = delete;

  virtual ~RenderTextWrapper();

  virtual void SetColor(SkColor value);
  virtual void ApplyColor(SkColor value, const gfx::Range& range);

  virtual void SetStyle(gfx::TextStyle style, bool value);
  virtual void ApplyStyle(gfx::TextStyle style,
                          bool value,
                          const gfx::Range& range);

  virtual void SetWeight(gfx::Font::Weight weight);
  virtual void ApplyWeight(gfx::Font::Weight weight, const gfx::Range& range);

  virtual void SetDirectionalityMode(gfx::DirectionalityMode mode);

 private:
  raw_ptr<gfx::RenderText> render_text_ = nullptr;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_RENDER_TEXT_WRAPPER_H_
