// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/quick_settings_media_view_container.h"

#include "ash/system/media/quick_settings_media_view_controller.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {
constexpr int kContainerHeight = 150;
}  // namespace

QuickSettingsMediaViewContainer::QuickSettingsMediaViewContainer(
    UnifiedSystemTrayController* controller)
    : controller_(controller) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

void QuickSettingsMediaViewContainer::SetShowMediaView(bool show_media_view) {
  show_media_view_ = show_media_view;
  MaybeShowMediaView();
}

void QuickSettingsMediaViewContainer::MaybeShowMediaView() {
  SetVisible(show_media_view_);
  if (show_media_view_) {
    // When the quick settings view wants to show the media view, update the
    // media item order to put the actively playing ones in the front.
    controller_->media_view_controller()->UpdateMediaItemOrder();
  }
}

int QuickSettingsMediaViewContainer::GetExpandedHeight() const {
  return show_media_view_ ? kContainerHeight : 0;
}

gfx::Size QuickSettingsMediaViewContainer::CalculatePreferredSize() const {
  return gfx::Size(kTrayMenuWidth, GetExpandedHeight());
}

}  // namespace ash