// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/privacy/privacy_hub_handler.h"

#include "ash/constants/ash_features.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/synchronization/condition_variable.h"
#include "chrome/browser/ash/privacy_hub/privacy_hub_hats_trigger.h"
#include "chrome/browser/ash/privacy_hub/privacy_hub_util.h"
#include "chrome/common/chrome_features.h"

namespace ash::settings {

PrivacyHubHandler::PrivacyHubHandler() = default;

PrivacyHubHandler::~PrivacyHubHandler() {
  TriggerHatsIfPageWasOpened();
  privacy_hub_util::SetFrontend(nullptr);
}

void PrivacyHubHandler::RegisterMessages() {
  if (ash::features::IsCrosPrivacyHubEnabled()) {
    privacy_hub_util::SetFrontend(this);
    web_ui()->RegisterMessageCallback(
        "getInitialMicrophoneHardwareToggleState",
        base::BindRepeating(
            &PrivacyHubHandler::HandleInitialMicrophoneSwitchState,
            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "getInitialCameraSwitchForceDisabledState",
        base::BindRepeating(
            &PrivacyHubHandler::HandleInitialCameraSwitchForceDisabledState,
            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "getCameraLedFallbackState",
        base::BindRepeating(
            &PrivacyHubHandler::HandleInitialCameraLedFallbackState,
            base::Unretained(this)));
  }

  if (base::FeatureList::IsEnabled(
          ::features::kHappinessTrackingPrivacyHubPostLaunch)) {
    web_ui()->RegisterMessageCallback(
        "osPrivacyPageWasOpened",
        base::BindRepeating(&PrivacyHubHandler::HandlePrivacyPageOpened,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "leftOsPrivacyPage",
        base::BindRepeating(&PrivacyHubHandler::HandlePrivacyPageClosed,
                            base::Unretained(this)));
  }
}

void PrivacyHubHandler::NotifyJS(const std::string& event_name,
                                 const base::Value& value) {
  if (IsJavascriptAllowed()) {
    FireWebUIListener(event_name, value);
  } else {
    DVLOG(1) << "JS disabled. Skip \"" << event_name
             << "\" event until enabled.";
  }
}

void PrivacyHubHandler::MicrophoneHardwareToggleChanged(bool muted) {
  DCHECK(ash::features::IsCrosPrivacyHubEnabled());
  NotifyJS("microphone-hardware-toggle-changed", base::Value(muted));
}

void PrivacyHubHandler::SetForceDisableCameraSwitch(bool disabled) {
  DCHECK(ash::features::IsCrosPrivacyHubEnabled());
  NotifyJS("force-disable-camera-switch", base::Value(disabled));
}

void PrivacyHubHandler::SetPrivacyPageOpenedTimeStampForTesting(
    base::TimeTicks time_stamp) {
  privacy_page_opened_timestamp_ = time_stamp;
}

void PrivacyHubHandler::HandlePrivacyPageOpened(const base::Value::List& args) {
  DCHECK(args.empty());
  DCHECK(base::FeatureList::IsEnabled(
      ::features::kHappinessTrackingPrivacyHubPostLaunch));

  // TODO(b/290646585): Replace with a CHECK().
  AllowJavascript();

  privacy_page_opened_timestamp_ = base::TimeTicks::Now();
}

void PrivacyHubHandler::HandlePrivacyPageClosed(const base::Value::List& args) {
  DCHECK(args.empty());
  DCHECK(base::FeatureList::IsEnabled(
      ::features::kHappinessTrackingPrivacyHubPostLaunch));

  // TODO(b/290646585): Replace with a CHECK().
  AllowJavascript();

  TriggerHatsIfPageWasOpened();
}

void PrivacyHubHandler::HandleInitialMicrophoneSwitchState(
    const base::Value::List& args) {
  const auto callback_id = ValidateArgs(args);
  const auto value = base::Value(privacy_hub_util::MicrophoneSwitchState());
  ResolveJavascriptCallback(callback_id, value);
}

void PrivacyHubHandler::HandleInitialCameraSwitchForceDisabledState(
    const base::Value::List& args) {
  const auto callback_id = ValidateArgs(args);
  const auto is_disabled =
      base::Value(privacy_hub_util::ShouldForceDisableCameraSwitch());
  ResolveJavascriptCallback(callback_id, is_disabled);
}

void PrivacyHubHandler::HandleInitialCameraLedFallbackState(
    const base::Value::List& args) {
  const auto callback_id = ValidateArgs(args);
  const auto value = base::Value(privacy_hub_util::UsingCameraLEDFallback());
  ResolveJavascriptCallback(callback_id, value);
}

const base::ValueView PrivacyHubHandler::ValidateArgs(
    const base::Value::List& args) {
  CHECK(ash::features::IsCrosPrivacyHubEnabled());
  // TODO(b/290646585): Replace with a CHECK().
  AllowJavascript();

  DCHECK_GE(1U, args.size()) << ": Did not expect arguments";
  DCHECK_EQ(1U, args.size()) << ": Callback ID is required";

  return args[0];
}

void PrivacyHubHandler::TriggerHatsIfPageWasOpened() {
  if (const base::TimeTicks now = base::TimeTicks::Now();
      (now - privacy_page_opened_timestamp_.value_or(now)) >=
      base::Seconds(5)) {
    privacy_page_opened_timestamp_.reset();
    PrivacyHubHatsTrigger::Get().ShowSurveyAfterDelayElapsed();
  }
}

}  // namespace ash::settings
