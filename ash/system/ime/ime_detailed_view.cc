// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/ime/ime_detailed_view.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"

namespace ash {

IMEDetailedView::IMEDetailedView(DetailedViewDelegate* delegate,
                                 ImeControllerImpl* ime_controller)
    : ImeListView(delegate), ime_controller_(ime_controller) {
  DCHECK(ime_controller_);
}

void IMEDetailedView::Update(const std::string& current_ime_id,
                             const std::vector<ImeInfo>& list,
                             const std::vector<ImeMenuItem>& property_list,
                             bool show_keyboard_toggle,
                             SingleImeBehavior single_ime_behavior) {
  ImeListView::Update(current_ime_id, list, property_list, show_keyboard_toggle,
                      single_ime_behavior);
  CreateTitleRow(IDS_ASH_STATUS_TRAY_IME);
}

void IMEDetailedView::ResetImeListView() {
  ImeListView::ResetImeListView();
  settings_button_ = nullptr;
  controlled_setting_icon_ = nullptr;
}

void IMEDetailedView::CreateExtraTitleRowButtons() {
  if (ime_controller_->managed_by_policy()) {
    const bool is_qs_revamp = features::IsQsRevampEnabled();
    controlled_setting_icon_ = TrayPopupUtils::CreateMainImageView(
        /*use_wide_layout=*/is_qs_revamp);
    if (is_qs_revamp) {
      // Match the size of the settings button. This size matches IconButton
      // kSmall, but IconButton doesn't expose that value, so we inline it here.
      controlled_setting_icon_->SetPreferredSize(gfx::Size(32, 32));
    }
    controlled_setting_icon_->SetImage(ui::ImageModel::FromVectorIcon(
        kSystemMenuBusinessIcon, kColorAshIconColorPrimary));
    controlled_setting_icon_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_IME_MANAGED));
    tri_view()->AddView(TriView::Container::END, controlled_setting_icon_);
  }

  tri_view()->SetContainerVisible(TriView::Container::END, true);
  settings_button_ =
      CreateSettingsButton(base::BindRepeating(&IMEDetailedView::ShowSettings,
                                               base::Unretained(this)),
                           IDS_ASH_STATUS_TRAY_IME_SETTINGS);
  tri_view()->AddView(TriView::Container::END, settings_button_);
}

void IMEDetailedView::ShowSettings() {
  base::RecordAction(base::UserMetricsAction("StatusArea_IME_Detailed"));
  CloseBubble();  // Deletes |this|.
  Shell::Get()->system_tray_model()->client()->ShowIMESettings();
}

BEGIN_METADATA(IMEDetailedView, ImeListView)
END_METADATA

}  // namespace ash
