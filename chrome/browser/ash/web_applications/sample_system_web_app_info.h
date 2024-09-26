// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_SAMPLE_SYSTEM_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_SAMPLE_SYSTEM_WEB_APP_INFO_H_

#include "chrome/browser/ash/system_web_apps/types/system_web_app_background_task_info.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"

struct WebAppInstallInfo;

#if defined(OFFICIAL_BUILD)
#error Demo Mode should only be included in unofficial builds.
#endif

class SampleSystemAppDelegate : public ash::SystemWebAppDelegate {
 public:
  explicit SampleSystemAppDelegate(Profile* profile);

  // ash::SystemWebAppDelegate overrides:
  std::unique_ptr<WebAppInstallInfo> GetWebAppInfo() const override;
  bool ShouldCaptureNavigations() const override;
  Browser* GetWindowForLaunch(Profile* profile, const GURL& url) const override;
  bool ShouldShowNewWindowMenuOption() const override;
  absl::optional<ash::SystemWebAppBackgroundTaskInfo> GetTimerInfo()
      const override;
};

// Return a WebAppInstallInfo used to install the app.
std::unique_ptr<WebAppInstallInfo> CreateWebAppInfoForSampleSystemWebApp();

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_SAMPLE_SYSTEM_WEB_APP_INFO_H_
