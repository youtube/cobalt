// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_SOURCE_VIEW_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_SOURCE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class IconButton;
class IconSwitch;

// A view that is part of the CaptureBar view, from which the user can toggle
// between the three available capture sources (fullscreen, region, and window).
// Only a single capture source can be active at any time.
class ASH_EXPORT CaptureModeSourceView : public views::View {
 public:
  METADATA_HEADER(CaptureModeSourceView);

  CaptureModeSourceView();
  CaptureModeSourceView(const CaptureModeSourceView&) = delete;
  CaptureModeSourceView& operator=(const CaptureModeSourceView&) = delete;
  ~CaptureModeSourceView() override;

  IconButton* fullscreen_toggle_button() const {
    return fullscreen_toggle_button_;
  }
  IconButton* region_toggle_button() const { return region_toggle_button_; }
  IconButton* window_toggle_button() const { return window_toggle_button_; }

  // Called when the capture source changes.
  void OnCaptureSourceChanged(CaptureModeSource new_source);

  // Called when the capture type changes to update tooltip texts.
  void OnCaptureTypeChanged(CaptureModeType type);

 private:
  void OnFullscreenToggle();
  void OnRegionToggle();
  void OnWindowToggle();

  // Owned by the view hierarchy. Contains fullscreen, region, and window toggle
  // buttons.
  IconSwitch* capture_source_switch_;

  // Owned by the `capture_source_switch_`.
  IconButton* fullscreen_toggle_button_;
  IconButton* region_toggle_button_;
  IconButton* window_toggle_button_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_SOURCE_VIEW_H_
