// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/crostini_apps.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/app_menu_constants.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_package_service.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "storage/browser/file_system/file_system_context.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/strings/grit/ui_strings.h"

// TODO(crbug.com/826982): the equivalent of
// CrostiniAppModelBuilder::MaybeCreateRootFolder. Does some sort of "root
// folder" abstraction belong here (on the publisher side of the App Service)
// or should we hard-code that in one particular subscriber (the App List UI)?

namespace {

bool ShouldShowDisplayDensityMenuItem(const std::string& app_id,
                                      apps::MenuType menu_type,
                                      int64_t display_id) {
  // The default terminal app is crosh in a Chrome window and it doesn't run in
  // the Crostini container so it doesn't support display density the same way.
  if (menu_type != apps::MenuType::kShelf) {
    return false;
  }

  display::Display d;
  if (!display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id, &d)) {
    return true;
  }

  return d.device_scale_factor() != 1.0;
}

}  // namespace

namespace apps {

CrostiniApps::CrostiniApps(AppServiceProxy* proxy) : GuestOSApps(proxy) {}

CrostiniApps::~CrostiniApps() = default;

bool CrostiniApps::CouldBeAllowed() const {
  return crostini::CrostiniFeatures::Get()->CouldBeAllowed(profile());
}

apps::AppType CrostiniApps::AppType() const {
  return AppType::kCrostini;
}

guest_os::VmType CrostiniApps::VmType() const {
  return guest_os::VmType::TERMINA;
}

void CrostiniApps::LoadIcon(const std::string& app_id,
                            const IconKey& icon_key,
                            IconType icon_type,
                            int32_t size_hint_in_dip,
                            bool allow_placeholder_icon,
                            apps::LoadIconCallback callback) {
  registry()->LoadIcon(app_id, icon_key, icon_type, size_hint_in_dip,
                       allow_placeholder_icon, IDR_LOGO_CROSTINI_DEFAULT,
                       std::move(callback));
}

void CrostiniApps::Launch(const std::string& app_id,
                          int32_t event_flags,
                          LaunchSource launch_source,
                          WindowInfoPtr window_info) {
  crostini::LaunchCrostiniApp(
      profile(), app_id,
      window_info ? window_info->display_id : display::kInvalidDisplayId);
}

void CrostiniApps::LaunchAppWithIntent(const std::string& app_id,
                                       int32_t event_flags,
                                       IntentPtr intent,
                                       LaunchSource launch_source,
                                       WindowInfoPtr window_info,
                                       LaunchCallback callback) {
  // Retrieve URLs from the files in the intent.
  std::vector<crostini::LaunchArg> args;
  if (intent && intent->files.size() > 0) {
    args.reserve(intent->files.size());
    storage::FileSystemContext* file_system_context =
        file_manager::util::GetFileManagerFileSystemContext(profile());
    for (auto& file : intent->files) {
      args.emplace_back(
          file_system_context->CrackURLInFirstPartyContext(file->url));
    }
  }
  crostini::LaunchCrostiniAppWithIntent(
      profile(), app_id,
      window_info ? window_info->display_id : display::kInvalidDisplayId,
      std::move(intent), args,
      base::BindOnce(
          [](LaunchCallback callback, bool success,
             const std::string& failure_reason) {
            if (!success) {
              LOG(ERROR) << "Crostini launch error: " << failure_reason;
            }
            std::move(callback).Run(ConvertBoolToLaunchResult(success));
          },
          std::move(callback)));
}

void CrostiniApps::LaunchAppWithParams(AppLaunchParams&& params,
                                       LaunchCallback callback) {
  auto event_flags = apps::GetEventFlags(params.disposition,
                                         /*prefer_container=*/false);
  if (params.intent) {
    LaunchAppWithIntent(params.app_id, event_flags, std::move(params.intent),
                        params.launch_source,
                        std::make_unique<WindowInfo>(params.display_id),
                        std::move(callback));
  } else {
    Launch(params.app_id, event_flags, params.launch_source,
           std::make_unique<WindowInfo>(params.display_id));
    // TODO(crbug.com/1244506): Add launch return value.
    std::move(callback).Run(LaunchResult());
  }
}

void CrostiniApps::Uninstall(const std::string& app_id,
                             UninstallSource uninstall_source,
                             bool clear_site_data,
                             bool report_abuse) {
  crostini::CrostiniPackageService::GetForProfile(profile())
      ->QueueUninstallApplication(app_id);
}

void CrostiniApps::GetMenuModel(const std::string& app_id,
                                MenuType menu_type,
                                int64_t display_id,
                                base::OnceCallback<void(MenuItems)> callback) {
  MenuItems menu_items;

  if (menu_type == MenuType::kShelf) {
    AddCommandItem(ash::APP_CONTEXT_MENU_NEW_WINDOW, IDS_APP_LIST_NEW_WINDOW,
                   menu_items);
  }

  if (crostini::IsUninstallable(profile(), app_id)) {
    AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM, menu_items);
  }

  if (ShouldAddOpenItem(app_id, menu_type, profile())) {
    AddCommandItem(ash::LAUNCH_NEW, IDS_APP_CONTEXT_MENU_ACTIVATE_ARC,
                   menu_items);
  }

  if (ShouldAddCloseItem(app_id, menu_type, profile())) {
    AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE, menu_items);
  }

  // Offer users the ability to toggle per-application UI scaling.
  // Some apps have high-density display support and do not require scaling
  // to match the system display density, but others are density-unaware and
  // look better when scaled to match the display density.
  if (ShouldShowDisplayDensityMenuItem(app_id, menu_type, display_id)) {
    absl::optional<guest_os::GuestOsRegistryService::Registration>
        registration = registry()->GetRegistration(app_id);
    if (registration) {
      if (registration->IsScaled()) {
        AddCommandItem(ash::CROSTINI_USE_HIGH_DENSITY,
                       IDS_CROSTINI_USE_HIGH_DENSITY, menu_items);
      } else {
        AddCommandItem(ash::CROSTINI_USE_LOW_DENSITY,
                       IDS_CROSTINI_USE_LOW_DENSITY, menu_items);
      }
    }
  }

  std::move(callback).Run(std::move(menu_items));
}

void CrostiniApps::CreateAppOverrides(
    const guest_os::GuestOsRegistryService::Registration& registration,
    App* app) {
  // TODO(crbug.com/955937): Enable once Crostini apps are managed inside App
  // Management.
  app->show_in_management = false;

  app->allow_uninstall =
      crostini::IsUninstallable(profile(), registration.app_id());

  // TODO(crbug.com/1253250): Add other fields for the App struct.
}

}  // namespace apps
