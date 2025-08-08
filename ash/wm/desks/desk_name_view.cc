// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_name_view.h"

#include <memory>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

namespace {

constexpr int kDeskNameViewHorizontalPadding = 6;

}  // namespace

DeskNameView::DeskNameView(DeskMiniView* mini_view)
    : DeskTextfield(SystemTextfield::Type::kSmall), mini_view_(mini_view) {
  views::Builder<DeskNameView>(this)
      .SetBorder(views::CreateEmptyBorder(
          gfx::Insets::VH(0, kDeskNameViewHorizontalPadding)))
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER)
      .BuildChildren();

  set_use_default_focus_manager(mini_view_->owner_bar()->type() ==
                                DeskBarViewBase::Type::kDeskButton);
}

DeskNameView::~DeskNameView() = default;

void DeskNameView::OnFocus() {
  DeskTextfield::OnFocus();

  // When this gets focus, scroll to make `mini_view_` visible.
  mini_view_->owner_bar()->ScrollToShowViewIfNecessary(mini_view_);
}

void DeskNameView::OnFocusableViewFocused() {
  if (!HasFocus()) {
    // When the focus ring is the result of tabbing, as opposed to clicking or
    // chromevoxing, the name view will not have focus, so the user should be
    // told how to focus and edit the field.
    Shell::Get()
        ->accessibility_controller()
        ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringUTF8(
            IDS_ASH_DESKS_NAME_HIGHLIGHT_NOTIFICATION));
  }

  DeskTextfield::OnFocusableViewFocused();
  mini_view_->owner_bar()->ScrollToShowViewIfNecessary(mini_view_);
}

BEGIN_METADATA(DeskNameView, DeskTextfield)
END_METADATA

}  // namespace ash
