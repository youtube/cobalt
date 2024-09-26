// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_PROJECTOR_SYSTEM_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_PROJECTOR_SYSTEM_WEB_APP_INFO_H_

#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"

class Profile;

class ProjectorSystemWebAppDelegate : public ash::SystemWebAppDelegate {
 public:
  explicit ProjectorSystemWebAppDelegate(Profile* profile);
  ProjectorSystemWebAppDelegate(const ProjectorSystemWebAppDelegate&) = delete;
  ProjectorSystemWebAppDelegate operator=(
      const ProjectorSystemWebAppDelegate&) = delete;
  ~ProjectorSystemWebAppDelegate() override;

  // ash::SystemWebAppDelegate:
  std::unique_ptr<WebAppInstallInfo> GetWebAppInfo() const override;
  bool ShouldCaptureNavigations() const override;
  gfx::Size GetMinimumWindowSize() const override;
  bool IsAppEnabled() const override;
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_PROJECTOR_SYSTEM_WEB_APP_INFO_H_
