// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_SYSTEM_NUDGE_VIEW_H_
#define ASH_SYSTEM_TOAST_SYSTEM_NUDGE_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

namespace views {
class ImageView;
class Label;
class LabelButton;
class ImageButton;
}  // namespace views

namespace ash {

struct AnchoredNudgeData;
class SystemShadow;

// The System Nudge view. (go/cros-educationalnudge-spec)
// This view supports different configurations depending on the provided
// nudge data parameters. It will always have a body text, and may have a
// leading image view, a title text, and up to two buttons placed on the bottom.
class ASH_EXPORT SystemNudgeView : public views::FlexLayoutView {
 public:
  METADATA_HEADER(SystemNudgeView);

  SystemNudgeView(AnchoredNudgeData& nudge_data);
  SystemNudgeView(const SystemNudgeView&) = delete;
  SystemNudgeView& operator=(const SystemNudgeView&) = delete;
  ~SystemNudgeView() override;

  // TODO(b/306466133): Use `GetViewByID` when applicable in tests instead of
  // exposing nudge child views.
  views::ImageView* image_view() const { return image_view_; }
  views::Label* body_label() const { return body_label_; }
  views::Label* title_label() const { return title_label_; }
  views::LabelButton* first_button() const { return first_button_; }
  views::LabelButton* second_button() const { return second_button_; }

  // Called when the device zoom scale changes, observed from the widget.
  void UpdateShadowBounds();

 private:
  // Owned by the views hierarchy.
  raw_ptr<views::ImageView> image_view_ = nullptr;
  raw_ptr<views::Label> body_label_ = nullptr;
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::LabelButton> first_button_ = nullptr;
  raw_ptr<views::LabelButton> second_button_ = nullptr;
  raw_ptr<views::ImageButton> close_button_ = nullptr;

  std::unique_ptr<SystemShadow> shadow_;

  // views::View:
  void AddedToWidget() override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // Handles mouse enter/exit events to either show or hide `close_button_`.
  void HandleOnMouseHovered(const bool mouse_entered);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_SYSTEM_NUDGE_VIEW_H_
