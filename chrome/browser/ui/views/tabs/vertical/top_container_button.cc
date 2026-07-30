// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/top_container_button.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/actions/action_view_interface.h"
#include "ui/views/widget/widget.h"

namespace {
class TopContainerButtonActionViewInterface
    : public views::LabelButtonActionViewInterface {
 public:
  explicit TopContainerButtonActionViewInterface(
      TopContainerButton* action_view)
      : views::LabelButtonActionViewInterface(action_view),
        action_view_(action_view) {}

  void ActionItemChangedImpl(actions::ActionItem* action_item) override {
    ButtonActionViewInterface::ActionItemChangedImpl(action_item);
    if (action_item->GetImage().IsVectorIcon()) {
      action_view_->UpdateIcon(action_item->GetImage());
    }
  }

  void OnViewChangedImpl(actions::ActionItem* action_item) override {
    ButtonActionViewInterface::OnViewChangedImpl(action_item);
    if (action_item->GetImage().IsVectorIcon()) {
      action_view_->UpdateIcon(action_item->GetImage());
    }
  }

 private:
  raw_ptr<TopContainerButton> action_view_ = nullptr;
};
}  // namespace

TopContainerButton::TopContainerButton() {
  views::FocusRing::Get(this)->SetColorId(kColorNewTabButtonFocusRing);
  ConfigureInkDrop(this);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

TopContainerButton::~TopContainerButton() = default;

void TopContainerButton::UpdateIcon(const ui::ImageModel& icon_image) {
  CHECK(icon_image.IsVectorIcon());

  if (delay_icon_updates_) {
    pending_icon_image_ = icon_image;
    return;
  }

  const ui::ImageModel image_model = ui::ImageModel::FromVectorIcon(
      *icon_image.GetVectorIcon().vector_icon(), GetForegroundColor(),
      GetLayoutConstant(LayoutConstant::kVerticalTabStripButtonIconSize));

  SetImageModel(views::Button::STATE_NORMAL, image_model);
  SetImageModel(views::Button::STATE_HOVERED, image_model);
  SetImageModel(views::Button::STATE_PRESSED, image_model);
  SetImageModel(views::Button::STATE_DISABLED, image_model);
}

void TopContainerButton::SetDelayIconUpdates(bool delay) {
  delay_icon_updates_ = delay;
}

void TopContainerButton::ApplyPendingIcon() {
  if (pending_icon_image_) {
    bool old_delay = delay_icon_updates_;
    delay_icon_updates_ = false;
    UpdateIcon(*pending_icon_image_);
    delay_icon_updates_ = old_delay;
    pending_icon_image_.reset();
  }
}

void TopContainerButton::AddedToWidget() {
  paint_as_active_subscription_ =
      GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &View::NotifyViewControllerCallback, base::Unretained(this)));
}

void TopContainerButton::RemovedFromWidget() {
  paint_as_active_subscription_ = {};
}

ui::ColorId TopContainerButton::GetForegroundColor() const {
  return GetWidget() && GetWidget()->ShouldPaintAsActive()
             ? kColorTabForegroundInactiveFrameActive
             : kColorTabForegroundInactiveFrameInactive;
}

gfx::Size TopContainerButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int size =
      GetLayoutConstant(LayoutConstant::kVerticalTabStripCollapseButtonSize);
  return gfx::Size(size, size);
}

std::unique_ptr<views::ActionViewInterface>
TopContainerButton::GetActionViewInterface() {
  return std::make_unique<TopContainerButtonActionViewInterface>(this);
}

BEGIN_METADATA(TopContainerButton)
END_METADATA
