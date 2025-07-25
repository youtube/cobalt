// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_HELP_APP_UI_HELP_APP_UI_DELEGATE_H_
#define ASH_WEBUI_HELP_APP_UI_HELP_APP_UI_DELEGATE_H_

#include <string>

#include "ash/webui/help_app_ui/help_app_ui.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;
class PrefService;

namespace ash {

// A delegate which exposes browser functionality from //chrome to the help app
// ui page handler.
class HelpAppUIDelegate {
 public:
  virtual ~HelpAppUIDelegate() = default;

  // Opens the native chrome feedback dialog scoped to chrome://help-app.
  // Returns an optional error message if unable to open the dialog or nothing
  // if the dialog was determined to have opened successfully.
  virtual absl::optional<std::string> OpenFeedbackDialog() = 0;

  // Opens OS Settings at the parental controls section.
  virtual void ShowParentalControls() = 0;

  // Triggers the call-to-action associated with the given action type id.
  virtual void TriggerWelcomeTipCallToAction(
      help_app::mojom::ActionTypeId action_type_id) = 0;

  // Gets locally stored users preferences and state.
  virtual PrefService* GetLocalState() = 0;

  // Launches the MS365 setup flow (or shows the final screen of the flow if it
  // was already completed).
  virtual void LaunchMicrosoft365Setup() = 0;

  // Asks the help app notification controller to show the discover notification
  // if the required heuristics are present and if a notification for the help
  // app has not yet been shown in the current milestone.
  virtual void MaybeShowDiscoverNotification() = 0;

  // Asks the help app notification controller to show the release notes
  // notification if a notification for the help app has not yet been shown in
  // the current milestone.
  virtual void MaybeShowReleaseNotesNotification() = 0;

  // Gets device info obtained asynchronously via the DeviceInfoManager.
  virtual void GetDeviceInfo(
      ash::help_app::mojom::PageHandler::GetDeviceInfoCallback callback) = 0;

  // Opens a valid https:// URL in a new browser tab without getting intercepted
  // by URL capturing logic. If the "HelpAppAutoTriggerInstallDialog" feature
  // flag is enabled, this will automatically trigger the install dialog.
  // Failure to provide a valid https:// URL will cause the Help app renderer
  // process to crash.
  virtual absl::optional<std::string> OpenUrlInBrowserAndTriggerInstallDialog(
      const GURL& url) = 0;
};

}  // namespace ash

#endif  // ASH_WEBUI_HELP_APP_UI_HELP_APP_UI_DELEGATE_H_
