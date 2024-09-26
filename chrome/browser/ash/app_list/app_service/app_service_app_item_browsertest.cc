// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/shell.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/app_service/app_service_app_item.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/web_app_launch_process.h"
#include "chrome/browser/web_applications/test/with_crosapi_param.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/events/event_constants.h"

using web_app::test::CrosapiParam;
using web_app::test::WithCrosapiParam;

namespace {

void UpdateAppRegistryCache(Profile* profile,
                            const std::string& app_id,
                            bool block,
                            bool pause) {
  apps::AppPtr app =
      std::make_unique<apps::App>(apps::AppType::kChromeApp, app_id);
  app->readiness =
      block ? apps::Readiness::kDisabledByPolicy : apps::Readiness::kReady;
  app->paused = pause;

  std::vector<apps::AppPtr> apps;
  apps.push_back(std::move(app));
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .OnApps(std::move(apps), apps::AppType::kChromeApp,
              false /* should_notify_initialized */);
}

void UpdateAppNameInRegistryCache(Profile* profile,
                                  const std::string& app_id,
                                  const std::string& app_name) {
  apps::AppPtr app =
      std::make_unique<apps::App>(apps::AppType::kChromeApp, app_id);
  app->name = app_name;

  std::vector<apps::AppPtr> apps;
  apps.push_back(std::move(app));
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .OnApps(std::move(apps), apps::AppType::kChromeApp,
              false /* should_notify_initialized */);
}

ash::AppListItem* GetAppListItem(const std::string& id) {
  return ash::AppListModelProvider::Get()->model()->FindItem(id);
}

}  // namespace

class AppServiceAppItemBrowserTest : public extensions::PlatformAppBrowserTest {
 public:
  AppServiceAppItemBrowserTest() = default;
  ~AppServiceAppItemBrowserTest() override = default;

  // extensions::PlatformAppBrowserTest:
  void SetUpOnMainThread() override {
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();

    AppListClientImpl* client = AppListClientImpl::GetInstance();
    ASSERT_TRUE(client);

    // Associate |client| with the current profile.
    client->UpdateProfile();
  }

  std::unique_ptr<AppServiceAppItem> CreateUserInstalledChromeApp() {
    auto app = std::make_unique<apps::App>(apps::AppType::kChromeApp,
                                           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    app->install_reason = apps::InstallReason::kUser;
    apps::AppUpdate app_update(/*state=*/nullptr, /*delta=*/app.get(),
                               EmptyAccountId());
    return std::make_unique<AppServiceAppItem>(
        profile(), /*model_updater=*/nullptr,
        /*sync_item=*/nullptr, app_update);
  }
};

// Test the app status when the paused app is blocked, un-paused, and un-blocked
IN_PROC_BROWSER_TEST_F(AppServiceAppItemBrowserTest,
                       VerifyAppStatusForPausedApp) {
  const extensions::Extension* extension_app =
      LoadAndLaunchPlatformApp("launch", "Launched");
  ASSERT_TRUE(extension_app);

  ash::AppListItem* item = GetAppListItem(extension_app->id());
  ASSERT_TRUE(item);

  // Set the app as paused.
  UpdateAppRegistryCache(profile(), extension_app->id(), false /* block */,
                         true /* pause */);
  EXPECT_EQ(ash::AppStatus::kPaused, item->app_status());

  // Set the app as blocked, and paused.
  UpdateAppRegistryCache(profile(), extension_app->id(), true /* block */,
                         true /* pause */);
  EXPECT_EQ(ash::AppStatus::kBlocked, item->app_status());

  // Set the app as blocked, but not paused.
  UpdateAppRegistryCache(profile(), extension_app->id(), true /* block */,
                         false /* pause */);
  EXPECT_EQ(ash::AppStatus::kBlocked, item->app_status());

  // Set the app as neither blocked, nor paused.
  UpdateAppRegistryCache(profile(), extension_app->id(), false /* block */,
                         false /* pause */);
  EXPECT_EQ(ash::AppStatus::kReady, item->app_status());
}

// Test the app status when the blocked app is paused, un-blocked, and un-paused
IN_PROC_BROWSER_TEST_F(AppServiceAppItemBrowserTest,
                       VerifyAppStatusForBlockedApp) {
  const extensions::Extension* extension_app =
      LoadAndLaunchPlatformApp("launch", "Launched");
  ASSERT_TRUE(extension_app);

  ash::AppListItem* item = GetAppListItem(extension_app->id());
  ASSERT_TRUE(item);

  // Set the app as blocked.
  UpdateAppRegistryCache(profile(), extension_app->id(), true /* block */,
                         false /* pause */);
  EXPECT_EQ(ash::AppStatus::kBlocked, item->app_status());

  // Set the app as blocked, and paused.
  UpdateAppRegistryCache(profile(), extension_app->id(), true /* block */,
                         true /* pause */);
  EXPECT_EQ(ash::AppStatus::kBlocked, item->app_status());

  // Set the app as not blocked, but paused.
  UpdateAppRegistryCache(profile(), extension_app->id(), false /* block */,
                         true /* pause */);
  EXPECT_EQ(ash::AppStatus::kPaused, item->app_status());

  // Set the app as neither blocked, nor paused.
  UpdateAppRegistryCache(profile(), extension_app->id(), false /* block */,
                         false /* pause */);
  EXPECT_EQ(ash::AppStatus::kReady, item->app_status());
}

// Test the app status when the app is both blocked and paused.
IN_PROC_BROWSER_TEST_F(AppServiceAppItemBrowserTest,
                       VerifyAppStatusForBlockedAndPausedApp) {
  const extensions::Extension* extension_app =
      LoadAndLaunchPlatformApp("launch", "Launched");
  ASSERT_TRUE(extension_app);

  ash::AppListItem* item = GetAppListItem(extension_app->id());
  ASSERT_TRUE(item);

  // Set the app as blocked, and paused.
  UpdateAppRegistryCache(profile(), extension_app->id(), true /* block */,
                         true /* pause */);
  EXPECT_EQ(ash::AppStatus::kBlocked, item->app_status());

  // Set the app as neither blocked, nor paused.
  UpdateAppRegistryCache(profile(), extension_app->id(), false /* block */,
                         false /* pause */);
  EXPECT_EQ(ash::AppStatus::kReady, item->app_status());
}

// Test app name changes get propagated to launcher UI.
IN_PROC_BROWSER_TEST_F(AppServiceAppItemBrowserTest, UpdateAppNameInLauncher) {
  const extensions::Extension* extension_app =
      LoadAndLaunchPlatformApp("launch", "Launched");
  ASSERT_TRUE(extension_app);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST, {});
  ash::AppListTestApi app_list_test_api;
  app_list_test_api.WaitForBubbleWindow(/*wait_for_opening_animation=*/false);

  UpdateAppNameInRegistryCache(profile(), extension_app->id(), "Updated Name");

  EXPECT_EQ(u"Updated Name",
            app_list_test_api.GetAppListItemViewName(extension_app->id()));
}

IN_PROC_BROWSER_TEST_F(AppServiceAppItemBrowserTest,
                       ActivateAppRecordsNewInstallHistogram) {
  base::HistogramTester histograms;
  {
    ASSERT_FALSE(ash::TabletMode::Get()->InTabletMode());

    // Simulate a user-installed chrome app item.
    std::unique_ptr<AppServiceAppItem> app_item =
        CreateUserInstalledChromeApp();
    ASSERT_TRUE(app_item->is_new_install());

    // Activate (launch) the app, which marks it as not a new install and
    // records metrics.
    app_item->PerformActivate(ui::EF_NONE);
    EXPECT_FALSE(app_item->is_new_install());
    histograms.ExpectTotalCount(
        "Apps.TimeBetweenAppInstallAndLaunch.ClamshellMode", 1);
  }
  {
    ash::TabletMode::Get()->SetEnabledForTest(true);

    // Simulate a user-installed chrome app item.
    std::unique_ptr<AppServiceAppItem> app_item =
        CreateUserInstalledChromeApp();
    ASSERT_TRUE(app_item->is_new_install());

    // Activate (launch) the app, which marks it as not a new install and
    // records metrics.
    app_item->PerformActivate(ui::EF_NONE);
    EXPECT_FALSE(app_item->is_new_install());
    histograms.ExpectTotalCount(
        "Apps.TimeBetweenAppInstallAndLaunch.TabletMode", 1);
  }
}

class AppServiceSystemWebAppItemBrowserTest
    : public AppServiceAppItemBrowserTest,
      public WithCrosapiParam {};

IN_PROC_BROWSER_TEST_P(AppServiceSystemWebAppItemBrowserTest, Activate) {
  Profile* const profile = browser()->profile();
  ash::SystemWebAppManager::GetForTest(profile)->InstallSystemAppsForTesting();
  const web_app::AppId app_id = web_app::kHelpAppId;

  auto help_app = std::make_unique<apps::App>(apps::AppType::kWeb, app_id);
  apps::AppUpdate app_update(/*state=*/nullptr, /*delta=*/help_app.get(),
                             EmptyAccountId());
  AppServiceAppItem app_item(profile, /*model_updater=*/nullptr,
                             /*sync_item=*/nullptr, app_update);
  app_item.SetChromePosition(app_item.CalculateDefaultPositionForTest());

  ui_test_utils::BrowserChangeObserver browser_opened(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  app_item.PerformActivate(ui::EF_NONE);
  browser_opened.Wait();

  // Verify that a launch no longer occurs.
  web_app::WebAppLaunchProcess::SetOpenApplicationCallbackForTesting(
      base::BindLambdaForTesting(
          [](apps::AppLaunchParams&& params) -> content::WebContents* {
            NOTREACHED();
            return nullptr;
          }));

  app_item.PerformActivate(ui::EF_NONE);
}

INSTANTIATE_TEST_SUITE_P(All,
                         AppServiceSystemWebAppItemBrowserTest,
                         ::testing::Values(CrosapiParam::kDisabled,
                                           CrosapiParam::kEnabled),
                         WithCrosapiParam::ParamToString);
