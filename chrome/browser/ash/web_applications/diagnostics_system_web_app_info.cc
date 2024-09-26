// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/diagnostics_system_web_app_info.h"

#include <memory>

#include "ash/webui/diagnostics_ui/url_constants.h"
#include "ash/webui/grit/ash_diagnostics_app_resources.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/ui/webui/ash/diagnostics_dialog.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "url/gurl.h"

std::unique_ptr<WebAppInstallInfo>
CreateWebAppInfoForDiagnosticsSystemWebApp() {
  std::unique_ptr<WebAppInstallInfo> info =
      std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL(ash::kChromeUIDiagnosticsAppUrl);
  info->scope = GURL(ash::kChromeUIDiagnosticsAppUrl);

  // TODO(jimmyxgong): Update the title with finalized i18n copy.
  info->title = u"Diagnostics";
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {{"app_icon_192.png", 192, IDR_ASH_DIAGNOSTICS_APP_APP_ICON_192_PNG}},
      *info);
  info->theme_color =
      web_app::GetDefaultBackgroundColor(/*use_dark_mode=*/false);
  info->dark_mode_theme_color =
      web_app::GetDefaultBackgroundColor(/*use_dark_mode=*/true);
  info->background_color = info->theme_color;
  info->dark_mode_background_color = info->dark_mode_theme_color;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;

  return info;
}

DiagnosticsSystemAppDelegate::DiagnosticsSystemAppDelegate(Profile* profile)
    : ash::SystemWebAppDelegate(ash::SystemWebAppType::DIAGNOSTICS,
                                "Diagnostics",
                                GURL("chrome://diagnostics"),
                                profile) {}

std::unique_ptr<WebAppInstallInfo> DiagnosticsSystemAppDelegate::GetWebAppInfo()
    const {
  return CreateWebAppInfoForDiagnosticsSystemWebApp();
}

bool DiagnosticsSystemAppDelegate::ShouldShowInLauncher() const {
  return false;
}

gfx::Size DiagnosticsSystemAppDelegate::GetMinimumWindowSize() const {
  return {600, 390};
}

bool DiagnosticsSystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

Browser* DiagnosticsSystemAppDelegate::LaunchAndNavigateSystemWebApp(
    Profile* profile,
    web_app::WebAppProvider* provider,
    const GURL& url,
    const apps::AppLaunchParams& params) const {
  // Opening Diagnostics as an SWA does not automatically close any Diagnostics
  // Dialog instances so attempt to close it here.
  ash::DiagnosticsDialog::MaybeCloseExistingDialog();

  return SystemWebAppDelegate::LaunchAndNavigateSystemWebApp(profile, provider,
                                                             url, params);
}
