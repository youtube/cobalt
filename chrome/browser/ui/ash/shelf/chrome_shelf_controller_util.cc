// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"

#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/policy_util.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_list/extension_app_utils.h"
#include "chrome/browser/ash/eche_app/app_id.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_item_factory.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_prefs.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

const extensions::Extension* GetExtensionForAppID(const std::string& app_id,
                                                  Profile* profile) {
  return extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
      app_id, extensions::ExtensionRegistry::EVERYTHING);
}

AppListControllerDelegate::Pinnable GetPinnableForAppID(
    const std::string& app_id,
    Profile* profile) {
  // These file manager apps have a shelf presence, but can only be launched
  // when provided a filename to open. Likewise, the feedback extension, the
  // Eche application need context when launching. Pinning these creates an
  // item that does nothing.
  const char* kNoPinAppIds[] = {
      ash::eche_app::kEcheAppId,
  };
  if (base::Contains(kNoPinAppIds, app_id))
    return AppListControllerDelegate::NO_PIN;

  const absl::optional<std::vector<std::string>> policy_ids =
      apps_util::GetPolicyIdsFromAppId(profile, app_id);

  if (!policy_ids || policy_ids->empty()) {
    return AppListControllerDelegate::PIN_EDITABLE;
  }

  if (ash::DemoSession::Get() &&
      base::ranges::none_of(*policy_ids, [](const auto& policy_id) {
        return ash::DemoSession::Get()->ShouldShowAndroidOrChromeAppInShelf(
            policy_id);
      })) {
    return AppListControllerDelegate::PIN_EDITABLE;
  }

  const base::Value::List& policy_apps =
      profile->GetPrefs()->GetList(prefs::kPolicyPinnedLauncherApps);

  for (const base::Value& policy_dict_entry : policy_apps) {
    if (!policy_dict_entry.is_dict())
      return AppListControllerDelegate::PIN_EDITABLE;

    const std::string* policy_entry = policy_dict_entry.GetDict().FindString(
        ChromeShelfPrefs::kPinnedAppsPrefAppIDKey);
    if (!policy_entry)
      return AppListControllerDelegate::PIN_EDITABLE;

    if (base::Contains(*policy_ids,
                       apps_util::TransformRawPolicyId(*policy_entry))) {
      return AppListControllerDelegate::PIN_FIXED;
    }
  }

  return AppListControllerDelegate::PIN_EDITABLE;
}

bool IsBrowserRepresentedInBrowserList(Browser* browser,
                                       const ash::ShelfModel* model) {
  // Only Ash desktop browser windows for the active user are represented.
  if (!browser || !multi_user_util::IsProfileFromActiveUser(browser->profile()))
    return false;

  if (browser->is_type_app() || browser->is_type_app_popup()) {
    // V1 App popup windows may have their own item.
    ash::ShelfID id(web_app::GetAppIdFromApplicationName(browser->app_name()));
    if (model->ItemByID(id))
      return false;
  }

  return true;
}

void PinAppWithIDToShelf(const std::string& app_id) {
  auto* shelf_controller = ChromeShelfController::instance();
  auto* shelf_model = shelf_controller->shelf_model();
  if (shelf_model->ItemIndexByAppID(app_id) >= 0) {
    shelf_model->PinExistingItemWithID(app_id);
  } else {
    shelf_model->AddAndPinAppWithFactoryConstructedDelegate(app_id);
  }
}

void UnpinAppWithIDFromShelf(const std::string& app_id) {
  auto* shelf_controller = ChromeShelfController::instance();
  shelf_controller->shelf_model()->UnpinAppWithID(app_id);
}

bool IsAppWithIDPinnedToShelf(const std::string& app_id) {
  return ChromeShelfController::instance()->shelf_model()->IsAppPinned(app_id);
}

apps::LaunchSource ShelfLaunchSourceToAppsLaunchSource(
    ash::ShelfLaunchSource source) {
  switch (source) {
    case ash::LAUNCH_FROM_UNKNOWN:
      return apps::LaunchSource::kUnknown;
    case ash::LAUNCH_FROM_INTERNAL:
      return apps::LaunchSource::kFromChromeInternal;
    case ash::LAUNCH_FROM_APP_LIST:
      return apps::LaunchSource::kFromAppListGrid;
    case ash::LAUNCH_FROM_APP_LIST_SEARCH:
      return apps::LaunchSource::kFromAppListQuery;
    case ash::LAUNCH_FROM_APP_LIST_RECOMMENDATION:
      return apps::LaunchSource::kFromAppListRecommendation;
    case ash::LAUNCH_FROM_SHELF:
      return apps::LaunchSource::kFromShelf;
  }
}

bool BrowserAppShelfControllerShouldHandleApp(const std::string& app_id,
                                              Profile* profile) {
  if (!web_app::IsWebAppsCrosapiEnabled()) {
    return false;
  }
  auto* proxy =
      apps::AppServiceProxyFactory::GetInstance()->GetForProfile(profile);
  apps::AppType app_type = proxy->AppRegistryCache().GetAppType(app_id);
  switch (app_type) {
    case apps::AppType::kWeb:
    case apps::AppType::kSystemWeb:
    case apps::AppType::kStandaloneBrowser:
      return true;
    case apps::AppType::kStandaloneBrowserChromeApp: {
      // Should handle Standalone browser hosted apps.
      bool is_platform_app = false;
      proxy->AppRegistryCache().ForOneApp(
          app_id, [&is_platform_app](const apps::AppUpdate& update) {
            is_platform_app = update.IsPlatformApp().value_or(true);
          });
      return !is_platform_app;
    }
    default:
      return false;
  }
}
