// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_METRICS_UTIL_H_
#define COMPONENTS_SEND_TAB_TO_SELF_METRICS_UTIL_H_

#include <stddef.h>

#include <optional>

#include "base/time/time.h"
#include "components/send_tab_to_self/entry_point_display_reason.h"
#include "components/sync_device_info/device_info.h"

namespace send_tab_to_self {

enum class SendTabToSelfResult;

// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.chrome.browser.share.send_tab_to_self)
// LINT.IfChange(SendTabToSelfShareEntryPoint)
enum class ShareEntryPoint {
  // The context menu on a WebContents.
  kContentMenu = 0,
  // The context menu on a link.
  kLinkMenu = 1,
  // The icon in the toolbar, next to the Omnibox.
  kToolbarIcon = 2,
  // The context menu on the Omnibox.
  kOmniboxMenu = 3,
  // The Share menu in the 3dot menu.
  kShareMenu = 4,
  // The OS-level Share Sheet.
  kShareSheet = 5,
  // The context menu on a tab (in the tab strip or tab switcher).
  kTabMenu = 6,
  // A physical gesture.
  kGesture = 7,
  kMaxValue = kGesture,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sharing/enums.xml:SendTabToSelfShareEntryPoint)

// Records the entry point from which the Send Tab to Self feature was invoked.
void RecordEntryPointInvoked(ShareEntryPoint entry_point);

// Records the entry point from which the Send Tab to Self feature successfully
// sent a tab.
void RecordEntryPointSent(ShareEntryPoint entry_point);

// Records the result of attempting to send a tab.
void RecordSendResult(SendTabToSelfResult result);

// Records when a received STTS notification is shown.
void RecordNotificationShown();

// Records when a received STTS notification is dismissed.
void RecordNotificationDismissed();

// Records when a received STTS notification is opened.
void RecordNotificationOpened();

// Records when a received STTS notification is shown and times out.
void RecordNotificationTimedOut();

// Records when a received STTS notification is dismissed for an unknown reason.
void RecordNotificationDismissReasonUnknown();

// Records when a received STTS notification is throttled from being sent.
void RecordNotificationThrottled();

// Status of the auto-open attempt for a received STTS tab.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.chrome.browser.share.send_tab_to_self)
// LINT.IfChange(AutoOpenOutcome)
enum class AutoOpenOutcome {
  kSuccess = 0,
  kPending = 1,
  kOpenedPending = 2,
  kMaxValue = kOpenedPending,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sharing/enums.xml:SendTabToSelfAutoOpenOutcome)

// Records the outcome of an auto-open attempt.
void RecordAutoOpenOutcome(AutoOpenOutcome outcome);

// Outcome of matching a received form field to a field on the page.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SendTabToSelfFormFieldMatchOutcome)
enum class FormFieldMatchOutcome {
  kMatchedByIdNameAndType = 0,
  kMatchedBySignature = 1,
  kMatchedByExactTypeSet = 2,
  kNoMatch = 3,
  kMaxValue = kNoMatch,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sharing/enums.xml:SendTabToSelfFormFieldMatchOutcome)

// Records the outcome of matching a received form field.
void RecordFormFieldMatchOutcome(FormFieldMatchOutcome outcome, int count = 1);

// Status of scroll position generation when sending a tab.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.chrome.browser.share.send_tab_to_self)
// LINT.IfChange(ScrollPositionGenerationOutcome)
enum class ScrollPositionGenerationOutcome {
  kSuccess = 0,
  kBrowserTimeout = 1,
  kMainFrameChanged = 2,
  kMainFrameUnavailable = 3,
  kEmptySelector = 4,
  kLinkGenerationError = 5,
  kInvalidSelector = 6,
  kRendererTimeout = 7,
  kMaxValue = kRendererTimeout,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sharing/enums.xml:SendTabToSelfScrollPositionGenerationOutcome)

// Records the time taken to generate the scroll position when sending a tab.
void RecordScrollPositionGenerationTime(base::TimeDelta time);

// Records the outcome of scroll position generation when sending a tab.
void RecordScrollPositionGenerationOutcome(
    ScrollPositionGenerationOutcome outcome);

// Records the length of the generated scroll position selector.
void RecordScrollPositionSelectorLength(size_t length);

// Records whether an opened STTS notification contained a scroll position.
void RecordHasScrollPositionOnOpened(bool has_scroll_position);

// Records the size of the PageContext proto when sending a tab, before
// truncation.
void RecordPageContextSize(size_t size);

// Records the volume of scroll interaction after an STTS tab is opened.
// `with_restoration` is true if scroll restoration was attempted.
void RecordScrollVolume(float volume, bool with_restoration);

// Records the time from when a tab was shared (on the sending device) to when
// it was first received by the target device's bridge. Note: this involves
// clocks on two different devices so the value may be skewed.
void RecordTimeSentToReceived(base::TimeDelta delay);

// Records the time from when a tab was shared (on the sending device) to when
// it was opened by the user on the target device. Note: this involves clocks
// on two different devices so the value may be skewed.
void RecordTimeSentToOpened(base::TimeDelta delay);

// Form factor combinations for sending/receiving devices.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SendTabToSelfFormFactorCombination)
enum class SendTabToSelfFormFactorCombination {
  kDesktopToDesktop = 0,
  kDesktopToPhone = 1,
  kDesktopToTablet = 2,
  kDesktopToUnknown = 3,
  kPhoneToDesktop = 4,
  kPhoneToPhone = 5,
  kPhoneToTablet = 6,
  kPhoneToUnknown = 7,
  kTabletToDesktop = 8,
  kTabletToPhone = 9,
  kTabletToTablet = 10,
  kTabletToUnknown = 11,
  kUnknownToDesktop = 12,
  kUnknownToPhone = 13,
  kUnknownToTablet = 14,
  kUnknownToUnknown = 15,
  kMaxValue = kUnknownToUnknown,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sharing/enums.xml:SendTabToSelfFormFactorCombination)

void RecordDeviceFormFactorCombination(
    syncer::DeviceInfo::FormFactor sender_form_factor,
    syncer::DeviceInfo::FormFactor target_form_factor);

// Keep in sync with SendTabToSelfDeviceCount in enums.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SendTabToSelfDeviceCount)
enum class SendTabToSelfDeviceCount {
  kNoTargetDevicesBecauseSignedOut = 0,
  kZeroDevices = 1,
  kOneDevice = 2,
  kTwoDevices = 3,
  kThreeDevices = 4,
  kFourDevices = 5,
  kFiveDevices = 6,
  kMoreThanFiveDevices = 7,
  kMaxValue = kMoreThanFiveDevices,
};
// LINT.ThenChange(/tools/metrics/histograms/enums.xml:SendTabToSelfDeviceCount)

void RecordTargetDeviceCount(ShareEntryPoint entry_point,
                             EntryPointDisplayReason display_reason,
                             size_t device_count);

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_METRICS_UTIL_H_
