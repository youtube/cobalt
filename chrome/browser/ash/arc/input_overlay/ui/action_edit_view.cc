// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_edit_view.h"

#include <memory>

#include "ash/bubble/bubble_utils.h"
#include "ash/style/rounded_container.h"
#include "ash/style/typography.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_labels.h"
#include "chrome/browser/ash/arc/input_overlay/ui/name_tag.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/background.h"
#include "ui/views/layout/table_layout_view.h"

namespace arc::input_overlay {

namespace {
constexpr float corner_radius = 16.0f;
}  // namespace

ActionEditView::ActionEditView(DisplayOverlayController* controller,
                               Action* action,
                               ash::RoundedContainer::Behavior container_type)
    : views::Button(base::BindRepeating(&ActionEditView::OnClicked,
                                        base::Unretained(this))),
      controller_(controller),
      action_(action) {
  // TODO(b/279117180): Replace with proper accessible name.
  SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_GAME_CONTROLS_ALPHA));
  SetUseDefaultFillLayout(true);
  SetNotifyEnterExitOnChild(true);
  auto* container = AddChildView(std::make_unique<views::TableLayoutView>());
  container->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(14, 16)));
  container->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBase,
      /*top_radius=*/container_type ==
              ash::RoundedContainer::Behavior::kBottomRounded
          ? 0.0f
          : corner_radius,
      /*bottom_radius=*/corner_radius,
      /*for_border_thickness=*/0));

  container
      ->AddColumn(/*h_align=*/views::LayoutAlignment::kStart,
                  /*v_align=*/views::LayoutAlignment::kStart,
                  /*horizontal_resize=*/1.0f,
                  /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                  /*fixed_width=*/0, /*min_width=*/0)
      .AddColumn(/*h_align=*/views::LayoutAlignment::kEnd,
                 /*v_align=*/views::LayoutAlignment::kCenter,
                 /*horizontal_resize=*/1.0f,
                 /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddRows(1, /*vertical_resize=*/views::TableLayout::kFixedSize);

  // TODO(b/274690042): Replace placeholder text with localized strings.
  name_tag_ = container->AddChildView(NameTag::CreateNameTag(u"Unassigned"));
  labels_view_ = container->AddChildView(EditLabels::CreateEditLabels(
      controller_, action_, name_tag_, /*should_update_title=*/true));
}

ActionEditView::~ActionEditView() = default;

void ActionEditView::RemoveNewState() {
  labels_view_->RemoveNewState();
}

void ActionEditView::OnActionNameUpdated() {}

void ActionEditView::OnActionInputBindingUpdated() {
  labels_view_->OnActionInputBindingUpdated();
}

void ActionEditView::OnClicked() {
  ClickCallback();
}

}  // namespace arc::input_overlay
