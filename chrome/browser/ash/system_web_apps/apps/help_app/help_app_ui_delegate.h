// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_HELP_APP_HELP_APP_UI_DELEGATE_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_HELP_APP_HELP_APP_UI_DELEGATE_H_

#include <memory>

#include "ash/webui/help_app_ui/help_app_ui_delegate.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class WebUI;
}  // namespace content

namespace apps {
class DeviceInfoManager;
}  // namespace apps

namespace ash {

/**
 * Implementation of the HelpAppUiDelegate interface. Provides the help app
 * code in chromeos/ with functions that only exist in chrome/.
 */
class ChromeHelpAppUIDelegate : public HelpAppUIDelegate {
 public:
  explicit ChromeHelpAppUIDelegate(content::WebUI* web_ui);

  ChromeHelpAppUIDelegate(const ChromeHelpAppUIDelegate&) = delete;
  ChromeHelpAppUIDelegate& operator=(const ChromeHelpAppUIDelegate&) = delete;
  ~ChromeHelpAppUIDelegate() override;

  // HelpAppUIDelegate:
  absl::optional<std::string> OpenFeedbackDialog() override;
  void ShowParentalControls() override;
  void TriggerWelcomeTipCallToAction(
      help_app::mojom::ActionTypeId action_type_id) override;
  PrefService* GetLocalState() override;
  void LaunchMicrosoft365Setup() override;
  void MaybeShowDiscoverNotification() override;
  void MaybeShowReleaseNotesNotification() override;
  void GetDeviceInfo(ash::help_app::mojom::PageHandler::GetDeviceInfoCallback
                         callback) override;
  absl::optional<std::string> OpenUrlInBrowserAndTriggerInstallDialog(
      const GURL& url) override;

 private:
  raw_ptr<content::WebUI, ExperimentalAsh> web_ui_;  // Owns |this|.
  std::unique_ptr<apps::DeviceInfoManager> device_info_manager_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_HELP_APP_HELP_APP_UI_DELEGATE_H_
