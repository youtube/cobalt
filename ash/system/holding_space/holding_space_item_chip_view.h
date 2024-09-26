// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_CHIP_VIEW_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_CHIP_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/holding_space/holding_space_animation_registry.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/metadata/view_factory.h"

namespace views {
class ImageButton;
class Label;
}  // namespace views

namespace ash {

class HoldingSpaceItem;
class HoldingSpaceViewDelegate;
class ProgressIndicator;
class RoundedImageView;

// A button with an image derived from a file's thumbnail and file's name as the
// label.
class ASH_EXPORT HoldingSpaceItemChipView : public HoldingSpaceItemView {
 public:
  METADATA_HEADER(HoldingSpaceItemChipView);

  HoldingSpaceItemChipView(HoldingSpaceViewDelegate* delegate,
                           const HoldingSpaceItem* item);
  HoldingSpaceItemChipView(const HoldingSpaceItemChipView&) = delete;
  HoldingSpaceItemChipView& operator=(const HoldingSpaceItemChipView&) = delete;
  ~HoldingSpaceItemChipView() override;

 private:
  // HoldingSpaceItemView:
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  std::u16string GetTooltipText(const gfx::Point& point) const override;
  void OnHoldingSpaceItemUpdated(const HoldingSpaceItem* item,
                                 uint32_t updated_fields) override;
  void OnPrimaryActionVisibilityChanged(bool visible) override;
  void OnSelectionUiChanged() override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnThemeChanged() override;

  // Invoked during `label`'s paint sequence to paint its optional mask. Note
  // that `label` is only masked when the `primary_action_container()` is
  // visible to avoid overlapping.
  void OnPaintLabelMask(views::Label* label, gfx::Canvas* canvas);

  // Invoked when the secondary action is pressed. This will be one of either
  // `secondary_action_pause_` or `secondary_action_resume_`.
  void OnSecondaryActionPressed();

  void UpdateImage();
  void UpdateImageAndProgressIndicatorVisibility();
  void UpdateImageTransform();
  void UpdateLabels();
  void UpdateSecondaryAction();

  // Owned by view hierarchy.
  RoundedImageView* image_ = nullptr;
  views::Label* primary_label_ = nullptr;
  views::Label* secondary_label_ = nullptr;
  views::View* secondary_action_container_ = nullptr;
  views::ImageButton* secondary_action_pause_ = nullptr;
  views::ImageButton* secondary_action_resume_ = nullptr;
  ProgressIndicator* progress_indicator_ = nullptr;

  base::CallbackListSubscription image_skia_changed_subscription_;
  base::CallbackListSubscription progress_ring_animation_changed_subscription_;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   HoldingSpaceItemChipView,
                   HoldingSpaceItemView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::HoldingSpaceItemChipView)

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_CHIP_VIEW_H_
