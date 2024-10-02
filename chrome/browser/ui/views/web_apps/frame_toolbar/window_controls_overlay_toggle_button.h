// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WINDOW_CONTROLS_OVERLAY_TOGGLE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WINDOW_CONTROLS_OVERLAY_TOGGLE_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

class BrowserView;

// Button in the WebAppToolbarButtonContainer that allows users to toggle
// window controls overlay on and off.
class WindowControlsOverlayToggleButton : public ToolbarButton {
 public:
  explicit WindowControlsOverlayToggleButton(BrowserView* browser_view);
  WindowControlsOverlayToggleButton(const WindowControlsOverlayToggleButton&) =
      delete;
  WindowControlsOverlayToggleButton& operator=(
      const WindowControlsOverlayToggleButton&) = delete;
  ~WindowControlsOverlayToggleButton() override;

  void ButtonPressed(const ui::Event& event);
  void SetColor(SkColor color);

  // ToolbarButton:
  void UpdateIcon() override;

 private:
  // The containing browser view.
  raw_ptr<BrowserView> browser_view_;
  SkColor icon_color_ = gfx::kPlaceholderColor;
  base::WeakPtrFactory<WindowControlsOverlayToggleButton> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WINDOW_CONTROLS_OVERLAY_TOGGLE_BUTTON_H_
