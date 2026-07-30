// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_TOP_CONTAINER_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_TOP_CONTAINER_BUTTON_H_

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/label_button.h"

// Manages custom styling for buttons used in the top container of the vertical
// tab strip and projects panel.
class TopContainerButton : public views::LabelButton {
  METADATA_HEADER(TopContainerButton, views::LabelButton)
 public:
  TopContainerButton();
  ~TopContainerButton() override;

  void UpdateIcon(const ui::ImageModel& icon_image);

  void SetDelayIconUpdates(bool delay);
  void ApplyPendingIcon();

  // views::LabelButton:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  std::unique_ptr<views::ActionViewInterface> GetActionViewInterface() override;

 private:
  // views::View:
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  ui::ColorId GetForegroundColor() const;

  bool delay_icon_updates_ = false;
  std::optional<ui::ImageModel> pending_icon_image_;

  base::CallbackListSubscription paint_as_active_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_TOP_CONTAINER_BUTTON_H_
