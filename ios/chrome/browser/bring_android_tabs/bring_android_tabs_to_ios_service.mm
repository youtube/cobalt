// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bring_android_tabs/bring_android_tabs_to_ios_service.h"

#import <string>

#import "base/containers/contains.h"
#import "base/files/file.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/segmentation_platform/public/result.h"
#import "components/sync/base/sync_prefs.h"
#import "components/sync/driver/sync_service.h"
#import "components/sync/driver/sync_user_settings.h"
#import "components/sync_device_info/device_info.h"
#import "components/sync_sessions/session_sync_service.h"
#import "ios/chrome/browser/bring_android_tabs/features.h"
#import "ios/chrome/browser/bring_android_tabs/metrics.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/flags/system_flags.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/synced_sessions/distant_session.h"
#import "ios/chrome/browser/synced_sessions/distant_tab.h"
#import "ios/chrome/browser/synced_sessions/synced_sessions.h"
#import "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using ::bring_android_tabs::kPromptAttemptStatusHistogramName;
using ::bring_android_tabs::PromptAttemptStatus;

// The length of time from now in which the tabs will be brought over.
const base::TimeDelta kTimeRangeOfTabsImported = base::Days(14);

// Logs `status` on UMA.
void RecordPromptAttemptStatus(PromptAttemptStatus status) {
  base::UmaHistogramEnumeration(kPromptAttemptStatusHistogramName, status);
}

// Returns true if the user is eligible for the Bring Android Tabs prompt. Logs
// attempt status metric on UMA if the user is NOT eligible.
bool UserEligibleForAndroidSwitcherPrompt() {
  bool first_run = FirstRun::IsChromeFirstRun() ||
                   experimental_flags::AlwaysDisplayFirstRun();
  if (!first_run) {
    // Check the time of first run.
    absl::optional<base::File::Info> info = FirstRun::GetSentinelInfo();
    if (info.has_value()) {
      base::Time first_run_time = info.value().creation_time;
      bool first_run_over_7_days_ago =
          base::Time::Now() - first_run_time > base::Days(7);
      if (first_run_over_7_days_ago) {
        return false;
      }
    }
  }
  return ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE;
}

// Returns true if the user is segmented as an Android switcher, either by
// the segmentation platform or by using the forced device switcher flag. If the
// user is NOT enrolled in sync or not identified as an Android switcher, this
// method logs so on histogram.
bool UserIsAndroidSwitcher(
    segmentation_platform::DeviceSwitcherResultDispatcher* dispatcher) {
  bool device_switcher_forced =
      experimental_flags::GetSegmentForForcedDeviceSwitcherExperience() ==
      segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel;
  if (device_switcher_forced) {
    return true;
  }

  segmentation_platform::ClassificationResult result =
      dispatcher->GetCachedClassificationResult();
  if (result.status != segmentation_platform::PredictionStatus::kSucceeded) {
    RecordPromptAttemptStatus(PromptAttemptStatus::kSegmentationIncomplete);
    return false;
  }

  if (base::Contains(
          result.ordered_labels,
          segmentation_platform::DeviceSwitcherModel::kIosPhoneChromeLabel)) {
    RecordPromptAttemptStatus(PromptAttemptStatus::kNotAndroidSwitcher);
    return false;
  }

  if (result.ordered_labels[0] ==
      segmentation_platform::DeviceSwitcherModel::kNotSyncedLabel) {
    RecordPromptAttemptStatus(PromptAttemptStatus::kSyncDisabled);
  }

  if (result.ordered_labels[0] !=
      segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel) {
    RecordPromptAttemptStatus(PromptAttemptStatus::kNotAndroidSwitcher);
    return false;
  }
  return true;
}

}  // namespace

BringAndroidTabsToIOSService::BringAndroidTabsToIOSService(
    segmentation_platform::DeviceSwitcherResultDispatcher* dispatcher,
    syncer::SyncService* sync_service,
    sync_sessions::SessionSyncService* session_sync_service,
    PrefService* browser_state_prefs)
    : device_switcher_result_dispatcher_(dispatcher),
      sync_service_(sync_service),
      session_sync_service_(session_sync_service),
      browser_state_prefs_(browser_state_prefs) {
  DCHECK(device_switcher_result_dispatcher_);
  DCHECK(sync_service_);
  DCHECK(session_sync_service_);
  DCHECK(browser_state_prefs_);
}

BringAndroidTabsToIOSService::~BringAndroidTabsToIOSService() {}

void BringAndroidTabsToIOSService::LoadTabs() {
  load_tabs_invoked_ = true;
  // Early return for users who should never see the prompt in the current
  // session. In case the user is previously eligible for the prompt but not
  // anymore, clear the tabs.
  if (!UserEligibleForAndroidSwitcherPrompt() ||
      PromptShownAndShouldNotShowAgain()) {
    synced_sessions_.reset();
    position_of_tabs_in_synced_sessions_.clear();
    return;
  }
  // Tabs already loaded. In this case, the user is guaranteed to be an Android
  // switcher, so we can skip the check to `UserIsAndroidSwitcher` and return
  // early.
  if (!position_of_tabs_in_synced_sessions_.empty()) {
    return;
  }
  // If the feature is enabled and the user is an android switcher BUT the tabs
  // are empty, it's possible that the tabs aren't loaded because the last
  // invocation to `LoadTabs()` happens before the user enrolls in sync. Try
  // again.
  // The feature check is called right before
  // `UserIsAndroidSwitcher()` to avoid collecting unnecessary/noisy data.
  if (GetBringYourOwnTabsPromptType() !=
          BringYourOwnTabsPromptType::kDisabled &&
      UserIsAndroidSwitcher(device_switcher_result_dispatcher_)) {
    LoadSyncedSessionsAndComputeTabPositions();
  }
}

size_t BringAndroidTabsToIOSService::GetNumberOfAndroidTabs() const {
  DCHECK(load_tabs_invoked_);
  return position_of_tabs_in_synced_sessions_.size();
}

synced_sessions::DistantTab* BringAndroidTabsToIOSService::GetTabAtIndex(
    size_t index) const {
  DCHECK_LT(index, position_of_tabs_in_synced_sessions_.size());
  std::tuple<size_t, size_t> indices =
      position_of_tabs_in_synced_sessions_[index];
  size_t session_idx = std::get<0>(indices);
  size_t tab_idx = std::get<1>(indices);
  return synced_sessions_->GetSession(session_idx)->tabs[tab_idx].get();
}

void BringAndroidTabsToIOSService::OnBringAndroidTabsPromptDisplayed() {
  browser_state_prefs_->SetBoolean(prefs::kIosBringAndroidTabsPromptDisplayed,
                                   true);
  prompt_shown_current_session_ = true;
}

void BringAndroidTabsToIOSService::OnUserInteractWithBringAndroidTabsPrompt() {
  prompt_interacted_ = true;
}

bool BringAndroidTabsToIOSService::PromptShownAndShouldNotShowAgain() const {
  bool shown_before = browser_state_prefs_->GetBoolean(
      prefs::kIosBringAndroidTabsPromptDisplayed);
  bool should_not_show_again =
      prompt_interacted_ || (shown_before && !prompt_shown_current_session_);
  if (should_not_show_again) {
    RecordPromptAttemptStatus(PromptAttemptStatus::kPromptShownAndDismissed);
  }
  return should_not_show_again;
}

void BringAndroidTabsToIOSService::LoadSyncedSessionsAndComputeTabPositions() {
  bool tab_sync_disabled =
      !sync_service_ ||
      !sync_service_->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kTabs);
  if (tab_sync_disabled) {
    RecordPromptAttemptStatus(PromptAttemptStatus::kTabSyncDisabled);
    return;
  }

  synced_sessions_ =
      std::make_unique<synced_sessions::SyncedSessions>(session_sync_service_);
  size_t session_count = synced_sessions_->GetSessionCount();
  for (size_t session_idx = 0; session_idx < session_count; session_idx++) {
    // Only tabs from an Android phone device within the last
    // `kTimeRangeOfTabsImported` are considered Android tabs.
    const synced_sessions::DistantSession* session =
        synced_sessions_->GetSession(session_idx);
    if (session->form_factor != syncer::DeviceInfo::FormFactor::kPhone ||
        session->modified_time < base::Time::Now() - kTimeRangeOfTabsImported) {
      continue;
    }
    size_t tab_size = session->tabs.size();
    for (size_t tab_idx = 0; tab_idx < tab_size; tab_idx++) {
      std::tuple<size_t, size_t> indices = {session_idx, tab_idx};
      position_of_tabs_in_synced_sessions_.push_back(indices);
    }
  }
  PromptAttemptStatus status = position_of_tabs_in_synced_sessions_.empty()
                                   ? PromptAttemptStatus::kNoActiveTabs
                                   : PromptAttemptStatus::kSuccess;
  RecordPromptAttemptStatus(status);
}
