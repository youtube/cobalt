// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/recovery/recovery_install_global_error.h"

#include "base/functional/bind.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/recovery_component_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"

RecoveryInstallGlobalError::RecoveryInstallGlobalError(Profile* profile)
        : elevation_needed_(false),
          profile_(profile),
          has_shown_bubble_view_(false) {
  GlobalErrorServiceFactory::GetForProfile(profile_)->AddUnownedGlobalError(
      this);

  PrefService* pref = g_browser_process->local_state();
  if (pref->FindPreference(prefs::kRecoveryComponentNeedsElevation)) {
    elevation_needed_ =
        pref->GetBoolean(prefs::kRecoveryComponentNeedsElevation);
  }
  if (elevation_needed_)
    GlobalErrorServiceFactory::GetForProfile(profile_)->NotifyErrorsChanged();

  pref_registrar_.Init(pref);
  pref_registrar_.Add(
      prefs::kRecoveryComponentNeedsElevation,
      base::BindRepeating(
          &RecoveryInstallGlobalError::OnElevationRequirementChanged,
          base::Unretained(this)));
}

RecoveryInstallGlobalError::~RecoveryInstallGlobalError() {}

void RecoveryInstallGlobalError::Shutdown() {
  GlobalErrorServiceFactory::GetForProfile(profile_)->RemoveUnownedGlobalError(
      this);
}

GlobalError::Severity RecoveryInstallGlobalError::GetSeverity() {
  return GlobalError::SEVERITY_HIGH;
}

bool RecoveryInstallGlobalError::HasMenuItem() {
  return HasElevationNotification();
}

int RecoveryInstallGlobalError::MenuItemCommandID() {
  return IDC_ELEVATED_RECOVERY_DIALOG;
}

std::u16string RecoveryInstallGlobalError::MenuItemLabel() {
  return l10n_util::GetStringUTF16(IDS_RELAUNCH_TO_UPDATE);
}

ui::ImageModel RecoveryInstallGlobalError::MenuItemIcon() {
  return ui::ImageModel::FromVectorIcon(kBrowserToolsUpdateIcon,
                                        ui::kColorAlertHighSeverity);
}

void RecoveryInstallGlobalError::ExecuteMenuItem(Browser* browser) {
  ShowBubbleView(browser);
}

bool RecoveryInstallGlobalError::HasBubbleView() {
  return HasElevationNotification();
}

bool RecoveryInstallGlobalError::HasShownBubbleView() {
  return has_shown_bubble_view_;
}

void RecoveryInstallGlobalError::ShowBubbleView(Browser* browser) {
  GlobalErrorWithStandardBubble::ShowBubbleView(browser);
  has_shown_bubble_view_ = true;
}

bool RecoveryInstallGlobalError::ShouldCloseOnDeactivate() const {
  return false;
}

std::u16string RecoveryInstallGlobalError::GetBubbleViewTitle() {
  return l10n_util::GetStringUTF16(IDS_RECOVERY_BUBBLE_TITLE);
}

std::vector<std::u16string>
RecoveryInstallGlobalError::GetBubbleViewMessages() {
  return std::vector<std::u16string>(
      1, l10n_util::GetStringUTF16(IDS_RECOVERY_BUBBLE_TEXT));
}

std::u16string RecoveryInstallGlobalError::GetBubbleViewAcceptButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_RUN_RECOVERY);
}

bool RecoveryInstallGlobalError::ShouldShowCloseButton() const {
  return true;
}

bool RecoveryInstallGlobalError::ShouldAddElevationIconToAcceptButton() {
  return true;
}

std::u16string RecoveryInstallGlobalError::GetBubbleViewCancelButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_DECLINE_RECOVERY);
}

void RecoveryInstallGlobalError::OnBubbleViewDidClose(Browser* browser) {
}

void RecoveryInstallGlobalError::BubbleViewAcceptButtonPressed(
    Browser* browser) {
  component_updater::AcceptedElevatedRecoveryInstall(pref_registrar_.prefs());
}

void RecoveryInstallGlobalError::BubbleViewCancelButtonPressed(
    Browser* browser) {
  component_updater::DeclinedElevatedRecoveryInstall(pref_registrar_.prefs());
}

bool RecoveryInstallGlobalError::HasElevationNotification() const {
  // Do not show this bubble if we already have an upgrade notice.
  return elevation_needed_ && !UpgradeDetector::GetInstance()->notify_upgrade();
}

void RecoveryInstallGlobalError::OnElevationRequirementChanged() {
  PrefService* pref = pref_registrar_.prefs();
  DCHECK(pref->FindPreference(prefs::kRecoveryComponentNeedsElevation));
  elevation_needed_ = pref->GetBoolean(prefs::kRecoveryComponentNeedsElevation);

  // Got a new elevation request, resets |has_shown_bubble_view_| so the
  // bubble has a higher priority to show.
  if (elevation_needed_)
    has_shown_bubble_view_ = false;

  GlobalErrorServiceFactory::GetForProfile(profile_)->NotifyErrorsChanged();
}
