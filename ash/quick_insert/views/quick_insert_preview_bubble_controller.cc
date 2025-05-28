// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_preview_bubble_controller.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/quick_insert/views/quick_insert_preview_bubble.h"
#include "base/check.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Duration to wait before showing the preview bubble when it is requested.
constexpr base::TimeDelta kShowBubbleDelay = base::Milliseconds(600);

}  // namespace

QuickInsertPreviewBubbleController::QuickInsertPreviewBubbleController() =
    default;

QuickInsertPreviewBubbleController::~QuickInsertPreviewBubbleController() {
  CloseBubble();
}

void QuickInsertPreviewBubbleController::ShowBubbleAfterDelay(
    HoldingSpaceImage* async_preview_image,
    const base::FilePath& path,
    views::View* anchor_view) {
  CreateBubbleWidget(async_preview_image, anchor_view);
  show_bubble_timer_.Start(
      FROM_HERE, kShowBubbleDelay,
      base::BindOnce(&QuickInsertPreviewBubbleController::ShowBubble,
                     weak_ptr_factory_.GetWeakPtr()));
}

void QuickInsertPreviewBubbleController::CloseBubble() {
  if (bubble_view_ == nullptr) {
    return;
  }
  bubble_view_->Close();
  OnWidgetDestroying(bubble_view_->GetWidget());
  for (auto& observer : observers_) {
    observer.OnPreviewBubbleVisibilityChanged(false);
  }
}

bool QuickInsertPreviewBubbleController::IsBubbleVisible() const {
  return bubble_view_ != nullptr;
}

void QuickInsertPreviewBubbleController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void QuickInsertPreviewBubbleController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void QuickInsertPreviewBubbleController::SetBubbleMainText(
    const std::u16string& text) {
  if (bubble_view_ == nullptr) {
    return;
  }

  if (text.empty()) {
    bubble_view_->ClearText();
  } else {
    bubble_view_->SetText(text);
  }
}

void QuickInsertPreviewBubbleController::OnWidgetDestroying(
    views::Widget* widget) {
  widget_observation_.Reset();
  bubble_view_ = nullptr;

  async_preview_image_ = nullptr;
}

void QuickInsertPreviewBubbleController::ShowBubbleImmediatelyForTesting(
    HoldingSpaceImage* async_preview_image,
    views::View* anchor_view) {
  CreateBubbleWidget(async_preview_image, anchor_view);
  bubble_view_->GetWidget()->Show();
}

QuickInsertPreviewBubbleView*
QuickInsertPreviewBubbleController::bubble_view_for_testing() const {
  return bubble_view_;
}

void QuickInsertPreviewBubbleController::UpdateBubbleImage() {
  if (bubble_view_ != nullptr) {
    bubble_view_->SetPreviewImage(
        ui::ImageModel::FromImageSkia(async_preview_image_->GetImageSkia(
            QuickInsertPreviewBubbleView::kPreviewImageSize)));
  }
}

void QuickInsertPreviewBubbleController::CreateBubbleWidget(
    HoldingSpaceImage* async_preview_image,
    views::View* anchor_view) {
  if (bubble_view_ != nullptr) {
    return;
  }

  CHECK(anchor_view);
  bubble_view_ = new QuickInsertPreviewBubbleView(anchor_view);
  async_preview_image_ = async_preview_image;
  bubble_view_->SetPreviewImage(
      ui::ImageModel::FromImageSkia(async_preview_image_->GetImageSkia()));
  // base::Unretained is safe here since `image_subscription_` is a member.
  // During destruction, `image_subscription_` will be destroyed before the
  // other members, so the callback is guaranteed to be safe.
  image_subscription_ =
      async_preview_image_->AddImageSkiaChangedCallback(base::BindRepeating(
          &QuickInsertPreviewBubbleController::UpdateBubbleImage,
          base::Unretained(this)));

  widget_observation_.Observe(bubble_view_->GetWidget());
}

void QuickInsertPreviewBubbleController::ShowBubble() {
  if (bubble_view_ == nullptr) {
    return;
  }

  bubble_view_->GetWidget()->Show();
  for (auto& observer : observers_) {
    observer.OnPreviewBubbleVisibilityChanged(true);
  }
}

}  // namespace ash
