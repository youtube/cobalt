// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quiet_mode_feature_pod_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/notifier_metadata.h"
#include "ash/public/cpp/notifier_settings_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"

using message_center::MessageCenter;

namespace ash {

QuietModeFeaturePodController::QuietModeFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {
  MessageCenter::Get()->AddObserver(this);
  if (!features::IsOsSettingsAppBadgingToggleEnabled()) {
    NotifierSettingsController::Get()->AddNotifierSettingsObserver(this);
  }
}

QuietModeFeaturePodController::~QuietModeFeaturePodController() {
  if (!features::IsOsSettingsAppBadgingToggleEnabled()) {
    NotifierSettingsController::Get()->RemoveNotifierSettingsObserver(this);
  }
  MessageCenter::Get()->RemoveObserver(this);
}

// static
bool QuietModeFeaturePodController::CalculateButtonVisibility() {
  auto* session_controller = Shell::Get()->session_controller();
  return session_controller->ShouldShowNotificationTray() &&
         !session_controller->IsScreenLocked();
}

FeaturePodButton* QuietModeFeaturePodController::CreateButton() {
  DCHECK(!button_);
  button_ = new FeaturePodButton(this);
  button_->SetVectorIcon(kUnifiedMenuDoNotDisturbIcon);
  const bool target_visibility = CalculateButtonVisibility();
  button_->SetVisible(target_visibility);
  if (target_visibility) {
    TrackVisibilityUMA();
  }

  button_->SetIconTooltip(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NOTIFICATIONS_TOGGLE_TOOLTIP,
      GetQuietModeStateTooltip()));

  if (features::IsOsSettingsAppBadgingToggleEnabled()) {
    button_->SetLabel(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DO_NOT_DISTURB));
  } else {
    button_->SetLabel(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NOTIFICATIONS_LABEL));
    button_->ShowDetailedViewArrow();
  }

  OnQuietModeChanged(MessageCenter::Get()->IsQuietMode());
  return button_;
}

std::unique_ptr<FeatureTile> QuietModeFeaturePodController::CreateTile(
    bool compact) {
  DCHECK(features::IsQsRevampEnabled());
  auto tile = std::make_unique<FeatureTile>(
      base::BindRepeating(&FeaturePodControllerBase::OnIconPressed,
                          weak_ptr_factory_.GetWeakPtr()),
      /*is_togglable=*/true,
      compact ? FeatureTile::TileType::kCompact
              : FeatureTile::TileType::kPrimary);
  tile_ = tile.get();
  tile_->SetID(VIEW_ID_DND_FEATURE_TILE);

  const bool target_visibility = CalculateButtonVisibility();
  tile_->SetVisible(target_visibility);
  if (target_visibility) {
    TrackVisibilityUMA();
  }

  // TODO(b/263416361): Update vector icon to its newer version.
  tile_->SetVectorIcon(kUnifiedMenuDoNotDisturbIcon);
  tile_->SetLabel(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DO_NOT_DISTURB));
  if (!compact) {
    tile_->SetSubLabelVisibility(false);
  }
  tile_->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NOTIFICATIONS_TOGGLE_TOOLTIP,
      GetQuietModeStateTooltip()));

  OnQuietModeChanged(MessageCenter::Get()->IsQuietMode());

  return tile;
}

QsFeatureCatalogName QuietModeFeaturePodController::GetCatalogName() {
  return QsFeatureCatalogName::kQuietMode;
}

void QuietModeFeaturePodController::OnIconPressed() {
  MessageCenter* message_center = MessageCenter::Get();
  bool is_quiet_mode = message_center->IsQuietMode();
  TrackToggleUMA(/*target_toggle_state=*/!is_quiet_mode);
  message_center->SetQuietMode(!is_quiet_mode);

  if (message_center->IsQuietMode()) {
    base::RecordAction(base::UserMetricsAction("StatusArea_QuietMode_Enabled"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("StatusArea_QuietMode_Disabled"));
  }
}

void QuietModeFeaturePodController::OnLabelPressed() {
  if (features::IsOsSettingsAppBadgingToggleEnabled()) {
    // Now that app badging has been moved to OS Settings, this detailed view is
    // not required.
    FeaturePodControllerBase::OnLabelPressed();
    return;
  }
  TrackDiveInUMA();
  tray_controller_->ShowNotifierSettingsView();
}

void QuietModeFeaturePodController::OnQuietModeChanged(bool in_quiet_mode) {
  if (features::IsQsRevampEnabled()) {
    tile_->SetToggled(in_quiet_mode);
    tile_->SetTooltipText(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NOTIFICATIONS_TOGGLE_TOOLTIP,
        GetQuietModeStateTooltip()));
    return;
  }

  button_->SetToggled(in_quiet_mode);
  button_->SetIconTooltip(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NOTIFICATIONS_TOGGLE_TOOLTIP,
      GetQuietModeStateTooltip()));

  if (in_quiet_mode) {
    const int sublabel_string_id =
        features::IsOsSettingsAppBadgingToggleEnabled()
            ? IDS_ASH_STATUS_TRAY_NOTIFICATIONS_DO_NOT_DISTURB_ON_SUBLABEL
            : IDS_ASH_STATUS_TRAY_NOTIFICATIONS_DO_NOT_DISTURB_SUBLABEL;
    const int tooltip_string_id =
        features::IsOsSettingsAppBadgingToggleEnabled()
            ? IDS_ASH_STATUS_TRAY_NOTIFICATIONS_SETTINGS_DO_NOT_DISTURB_TOGGLE_TOOLTIP
            : IDS_ASH_STATUS_TRAY_NOTIFICATIONS_SETTINGS_DO_NOT_DISTURB_TOOLTIP;
    button_->SetSubLabel(l10n_util::GetStringUTF16(sublabel_string_id));
    button_->SetLabelTooltip(l10n_util::GetStringUTF16(tooltip_string_id));
    return;
  }

  if (button_->GetVisible() &&
      !features::IsOsSettingsAppBadgingToggleEnabled()) {
    NotifierSettingsController::Get()->GetNotifiers();
    return;
  }

  button_->SetSubLabel(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_NOTIFICATIONS_DO_NOT_DISTURB_OFF_SUBLABEL));
  button_->SetLabelTooltip(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_NOTIFICATIONS_DO_NOT_DISTURB_OFF_STATE));
}

void QuietModeFeaturePodController::OnNotifiersUpdated(
    const std::vector<NotifierMetadata>& notifiers) {
  if (MessageCenter::Get()->IsQuietMode()) {
    return;
  }

  int disabled_count = 0;
  for (const NotifierMetadata& notifier : notifiers) {
    if (!notifier.enabled) {
      ++disabled_count;
    }
  }
  RecordDisabledNotifierCount(disabled_count);

  if (disabled_count > 0) {
    button_->SetSubLabel(l10n_util::GetPluralStringFUTF16(
        IDS_ASH_STATUS_TRAY_NOTIFICATIONS_OFF_FOR_APPS_SUBLABEL,
        disabled_count));
    button_->SetLabelTooltip(l10n_util::GetPluralStringFUTF16(
        IDS_ASH_STATUS_TRAY_NOTIFICATIONS_SETTINGS_OFF_FOR_APPS_TOOLTIP,
        disabled_count));
  } else {
    button_->SetSubLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NOTIFICATIONS_ON_SUBLABEL));
    button_->SetLabelTooltip(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NOTIFICATIONS_SETTINGS_ON_TOOLTIP));
  }
}

std::u16string QuietModeFeaturePodController::GetQuietModeStateTooltip() {
  return l10n_util::GetStringUTF16(
      MessageCenter::Get()->IsQuietMode()
          ? IDS_ASH_STATUS_TRAY_NOTIFICATIONS_DO_NOT_DISTURB_ON_STATE
          : IDS_ASH_STATUS_TRAY_NOTIFICATIONS_DO_NOT_DISTURB_OFF_STATE);
}

void QuietModeFeaturePodController::RecordDisabledNotifierCount(
    int disabled_count) {
  if (!last_disabled_count_.has_value()) {
    last_disabled_count_ = disabled_count;
    UMA_HISTOGRAM_COUNTS_100("ChromeOS.SystemTray.BlockedNotifiersOnOpen",
                             disabled_count);
    return;
  }

  if (*last_disabled_count_ == disabled_count) {
    return;
  }

  last_disabled_count_ = disabled_count;
  UMA_HISTOGRAM_COUNTS_100("ChromeOS.SystemTray.BlockedNotifiersAfterUpdate",
                           disabled_count);
}

}  // namespace ash
