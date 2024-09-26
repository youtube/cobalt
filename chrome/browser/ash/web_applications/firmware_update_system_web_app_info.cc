// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/firmware_update_system_web_app_info.h"

#include <memory>

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/color_util.h"
#include "ash/webui/firmware_update_ui/url_constants.h"
#include "ash/webui/grit/ash_firmware_update_app_resources.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/display/screen.h"

namespace {
// The Firmware Update SWA window will be a fixed 600px * 640px portal per
// the specification.
constexpr int kFirmwareUpdateAppDefaultWidth = 600;
constexpr int kFirmwareUpdateAppDefaultHeight = 640;

// FirmwareUpdateApp's title bar and background needs to be bg-elevation-2 for
// dark mode instead of the default dark mode background color.
SkColor GetDarkModeBackgroundColor() {
  // This code will be deleted after Jelly launch.  Do NOT copy it! This was
  // copied from system_web_app_install_utils.cc
  ui::ColorProviderSource* color_provider_source =
      ash::ColorUtil::GetColorProviderSourceForWindow(
          ash::Shell::GetPrimaryRootWindow());
  DCHECK(color_provider_source);
  const ui::ColorProvider* color_provider =
      color_provider_source->GetColorProvider();
  DCHECK(color_provider);
  return color_provider->GetColor(cros_tokens::kBgColorElevation2Dark);
}

}  // namespace

// TODO(michaelcheco): Update to correct icon.
std::unique_ptr<WebAppInstallInfo>
CreateWebAppInfoForFirmwareUpdateSystemWebApp() {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL(ash::kChromeUIFirmwareUpdateAppURL);
  info->scope = GURL(ash::kChromeUIFirmwareUpdateAppURL);
  info->title = l10n_util::GetStringUTF16(IDS_ASH_FIRMWARE_UPDATE_APP_TITLE);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {{"app_icon_192.png", 192, IDR_ASH_FIRMWARE_UPDATE_APP_APP_ICON_192_PNG}},
      *info);
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
  info->theme_color =
      web_app::GetDefaultBackgroundColor(/*use_dark_mode=*/false);
  if (!chromeos::features::IsJellyEnabled()) {
    // Once Jelly is launched, the theme and background colors for SWA are
    // ignored and this can be deleted.
    info->dark_mode_theme_color = GetDarkModeBackgroundColor();
    info->dark_mode_background_color = info->dark_mode_theme_color;
  }
  info->background_color = info->theme_color;

  return info;
}

gfx::Rect GetDefaultBoundsForFirmwareUpdateApp(Browser*) {
  gfx::Rect bounds =
      display::Screen::GetScreen()->GetDisplayForNewWindows().work_area();
  bounds.ClampToCenteredSize(
      {kFirmwareUpdateAppDefaultWidth, kFirmwareUpdateAppDefaultHeight});
  return bounds;
}

FirmwareUpdateSystemAppDelegate::FirmwareUpdateSystemAppDelegate(
    Profile* profile)
    : ash::SystemWebAppDelegate(ash::SystemWebAppType::FIRMWARE_UPDATE,
                                "FirmwareUpdate",
                                GURL(ash::kChromeUIFirmwareUpdateAppURL),
                                profile) {}

std::unique_ptr<WebAppInstallInfo>
FirmwareUpdateSystemAppDelegate::GetWebAppInfo() const {
  return CreateWebAppInfoForFirmwareUpdateSystemWebApp();
}

bool FirmwareUpdateSystemAppDelegate::ShouldAllowMaximize() const {
  return false;
}

bool FirmwareUpdateSystemAppDelegate::ShouldAllowResize() const {
  return false;
}

bool FirmwareUpdateSystemAppDelegate::ShouldShowInLauncher() const {
  return false;
}

bool FirmwareUpdateSystemAppDelegate::ShouldShowInSearch() const {
  return false;
}

gfx::Rect FirmwareUpdateSystemAppDelegate::GetDefaultBounds(
    Browser* browser) const {
  return GetDefaultBoundsForFirmwareUpdateApp(browser);
}
