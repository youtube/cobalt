// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_FIRMWARE_UPDATE_SYSTEM_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_FIRMWARE_UPDATE_SYSTEM_WEB_APP_INFO_H_

#include <memory>

#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/gfx/geometry/rect.h"

class Browser;
struct WebAppInstallInfo;

class FirmwareUpdateSystemAppDelegate : public ash::SystemWebAppDelegate {
 public:
  explicit FirmwareUpdateSystemAppDelegate(Profile* profile);

  // ash::SystemWebAppDelegate overrides:
  std::unique_ptr<WebAppInstallInfo> GetWebAppInfo() const override;
  bool ShouldAllowMaximize() const override;
  bool ShouldAllowResize() const override;
  bool ShouldShowInLauncher() const override;
  bool ShouldShowInSearch() const override;
  gfx::Rect GetDefaultBounds(Browser*) const override;
};

// Returns a WebAppInstallInfo used to install the app.
std::unique_ptr<WebAppInstallInfo>
CreateWebAppInfoForFirmwareUpdateSystemWebApp();

gfx::Rect GetDefaultBoundsForFirmwareUpdateApp(Browser*);

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_FIRMWARE_UPDATE_SYSTEM_WEB_APP_INFO_H_
