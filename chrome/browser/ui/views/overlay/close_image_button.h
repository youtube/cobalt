// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_CLOSE_IMAGE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_CLOSE_IMAGE_BUTTON_H_

#include "chrome/browser/ui/views/overlay/overlay_window_image_button.h"
#include "chrome/browser/ui/views/overlay/video_overlay_window_views.h"
#include "ui/base/metadata/metadata_header_macros.h"

// An image button representing a close button.
class CloseImageButton : public OverlayWindowImageButton {
 public:
  METADATA_HEADER(CloseImageButton);

  explicit CloseImageButton(PressedCallback callback);
  CloseImageButton(const CloseImageButton&) = delete;
  CloseImageButton& operator=(const CloseImageButton&) = delete;
  ~CloseImageButton() override = default;

  // Sets the position of itself with an offset from the given window size.
  void SetPosition(const gfx::Size& size,
                   VideoOverlayWindowViews::WindowQuadrant quadrant);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_CLOSE_IMAGE_BUTTON_H_
