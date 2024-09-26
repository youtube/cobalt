// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/cros_next_default_desk_button.h"

#include <algorithm>

#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"

namespace ash {

namespace {

constexpr int kDefaultButtonCornerRadius = 14;

constexpr int kDefaultButtonHorizontalPadding = 16;

constexpr int kDefaultDeskButtonMinWidth = 56;

constexpr int kDefaultDeskButtonHeight = 28;

}  // namespace

CrOSNextDefaultDeskButton::CrOSNextDefaultDeskButton(DeskBarViewBase* bar_view)
    : CrOSNextDeskButtonBase(
          DesksController::Get()->desks()[0]->name(),
          /*set_text=*/true,
          bar_view,
          base::BindRepeating(&CrOSNextDefaultDeskButton::OnButtonPressed,
                              base::Unretained(this))) {
  GetViewAccessibility().OverrideName(
      l10n_util::GetStringFUTF16(IDS_ASH_DESKS_DESK_ACCESSIBLE_NAME,
                                 DesksController::Get()->desks()[0]->name()));

  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBase, kDefaultButtonCornerRadius));
}

void CrOSNextDefaultDeskButton::UpdateLabelText() {
  SetText(gfx::ElideText(
      DesksController::Get()->desks()[0]->name(), gfx::FontList(),
      bounds().width() - 2 * kDefaultButtonHorizontalPadding, gfx::ELIDE_TAIL));
}

gfx::Size CrOSNextDefaultDeskButton::CalculatePreferredSize() const {
  auto* root_window =
      bar_view_->GetWidget()->GetNativeWindow()->GetRootWindow();
  const int preview_width = DeskMiniView::GetPreviewWidth(
      root_window->bounds().size(), DeskPreviewView::GetHeight(root_window));
  int label_width = 0, label_height = 0;
  gfx::Canvas::SizeStringInt(DesksController::Get()->desks()[0]->name(),
                             gfx::FontList(), &label_width, &label_height, 0,
                             gfx::Canvas::NO_ELLIPSIS);

  // `preview_width` is supposed to be larger than
  // `kZeroStateDefaultDeskButtonMinWidth`, but it might be not the truth for
  // tests with extreme abnormal size of display.
  const int min_width = std::min(preview_width, kDefaultDeskButtonMinWidth);
  const int max_width = std::max(preview_width, kDefaultDeskButtonMinWidth);
  const int width = std::clamp(
      label_width + 2 * kDefaultButtonHorizontalPadding, min_width, max_width);
  return gfx::Size(width, kDefaultDeskButtonHeight);
}

void CrOSNextDefaultDeskButton::OnButtonPressed() {
  bar_view_->UpdateNewMiniViews(/*initializing_bar_view=*/false,
                                /*expanding_bar_view=*/true);
  bar_view_->NudgeDeskName(/*desk_index=*/0);
}

BEGIN_METADATA(CrOSNextDefaultDeskButton, CrOSNextDeskButtonBase)
END_METADATA

}  // namespace ash
