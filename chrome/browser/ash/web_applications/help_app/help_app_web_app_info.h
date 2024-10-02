// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_HELP_APP_HELP_APP_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_HELP_APP_HELP_APP_WEB_APP_INFO_H_

#include <memory>
#include <vector>

#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

struct WebAppInstallInfo;
class Browser;

namespace ash {

class HelpAppSystemAppDelegate : public SystemWebAppDelegate {
 public:
  explicit HelpAppSystemAppDelegate(Profile* profile);

  // SystemWebAppDelegate overrides:
  std::unique_ptr<WebAppInstallInfo> GetWebAppInfo() const override;
  gfx::Rect GetDefaultBounds(Browser*) const override;
  gfx::Size GetMinimumWindowSize() const override;
  std::vector<int> GetAdditionalSearchTerms() const override;
  absl::optional<SystemWebAppBackgroundTaskInfo> GetTimerInfo() const override;
  bool ShouldCaptureNavigations() const override;
};

// Return a WebAppInstallInfo used to install the app.
std::unique_ptr<WebAppInstallInfo> CreateWebAppInfoForHelpWebApp();

// Returns the default bounds.
gfx::Rect GetDefaultBoundsForHelpApp(Browser*);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_HELP_APP_HELP_APP_WEB_APP_INFO_H_
