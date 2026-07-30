// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/preferred_apps_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/app_service/web_app_publisher_helper.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

webapps::AppId FindAppIdByPath(WebAppProvider& provider,
                               std::string_view path) {
  for (const WebApp& app : provider.registrar_unsafe().GetApps()) {
    if (app.start_url().path() == path) {
      return app.app_id();
    }
  }
  ADD_FAILURE() << "App not found for path: " << path;
  return "";
}

// Waiter class that blocks until the `apps::PreferredAppsListHandle` has
// finished initializing and is ready to be queried.
class PreferredAppsListReadyWaiter
    : public apps::PreferredAppsListHandle::Observer {
 public:
  explicit PreferredAppsListReadyWaiter(apps::PreferredAppsListHandle& handle)
      : handle_(handle) {
    if (!handle_->IsInitialized()) {
      observation_.Observe(&*handle_);
    }
  }

  PreferredAppsListReadyWaiter(const PreferredAppsListReadyWaiter&) = delete;
  PreferredAppsListReadyWaiter& operator=(const PreferredAppsListReadyWaiter&) =
      delete;
  ~PreferredAppsListReadyWaiter() override = default;

  void Wait() {
    if (handle_->IsInitialized()) {
      return;
    }
    run_loop_.Run();
  }

  void OnPreferredAppsListInitialized() override { run_loop_.Quit(); }

  void OnPreferredAppChanged(const std::string& app_id,
                             bool is_preferred_app) override {}
  void OnPreferredAppsListWillBeDestroyed(
      apps::PreferredAppsListHandle* handle) override {
    observation_.Reset();
  }

 private:
  const raw_ref<apps::PreferredAppsListHandle> handle_;
  base::RunLoop run_loop_;
  base::ScopedObservation<apps::PreferredAppsListHandle,
                          apps::PreferredAppsListHandle::Observer>
      observation_{this};
};

// Verifies the automated startup migration routine for PWA navigation capturing
// user preferences across two successive browser sessions:
// 1. `PRE_*`: Establishes the initial baseline state under `default-off`
//    (`reimpl_default_off`). Installs test applications and marks a specific
//    app (App B) as preferred by the user.
// 2. Main test: Simulates an upgrade/restart where navigation capturing is
//    enabled (`GetParam()`, which is `default-on` or `on-via-client-mode`). The
//    browser automatically executes startup state migration, keeps App B
//    preferred, migrates valid applications to preferred, and stores a backup
//    of App B.
class WebAppNavigationCapturingMigrationSuccessTest
    : public WebAppBrowserTestBase,
      public testing::WithParamInterface<std::string> {
 public:
  WebAppNavigationCapturingMigrationSuccessTest() {
    std::string_view test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();
    if (test_name.starts_with("DISABLED_")) {
      test_name.remove_prefix(9);
    }

    std::string link_capturing_state = "reimpl_default_off";
    if (!test_name.starts_with("PRE_")) {
      link_capturing_state = GetParam();
    }

    feature_list_.InitAndEnableFeatureWithParameters(
        features::kPwaNavigationCapturing,
        {{"link_capturing_state", link_capturing_state}});
  }

  WebAppNavigationCapturingMigrationSuccessTest(
      const WebAppNavigationCapturingMigrationSuccessTest&) = delete;
  WebAppNavigationCapturingMigrationSuccessTest& operator=(
      const WebAppNavigationCapturingMigrationSuccessTest&) = delete;
  ~WebAppNavigationCapturingMigrationSuccessTest() override = default;

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebAppNavigationCapturingMigrationSuccessTest,
                       DISABLED_PRE_MigrationOnStartup) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install App A (standard web app, no client mode).
  const GURL url_a =
      embedded_test_server()->GetURL("/web_apps/site_d/basic.html");
  webapps::AppId app_a = InstallWebAppFromManifest(browser(), url_a);
  apps::AppReadinessWaiter(profile(), app_a).Await();

  // Install App B (standard web app, no client mode).
  const GURL url_b =
      embedded_test_server()->GetURL("/web_apps/standalone/basic.html");
  webapps::AppId app_b = InstallWebAppFromManifest(browser(), url_b);
  apps::AppReadinessWaiter(profile(), app_b).Await();

  // Mark App B as user preferred before migration.
  apps_util::SetSupportedLinksPreferenceAndWait(profile(), app_b);

  // Install App C (has client mode: focus-existing).
  const GURL url_c = embedded_test_server()->GetURL(
      "/web_apps/get_manifest.html?"
      "launch_handler_client_mode_focus_existing.json");
  webapps::AppId app_c = InstallWebAppFromPage(browser(), url_c);
  apps::AppReadinessWaiter(profile(), app_c).Await();

  // Only app B should be set to capture links.
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  PreferredAppsListReadyWaiter(proxy->PreferredAppsList()).Wait();
  EXPECT_FALSE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_a));
  EXPECT_TRUE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_b));
  EXPECT_FALSE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_c));

  PrefService* prefs = profile()->GetPrefs();
  EXPECT_EQ(static_cast<MigrationState>(prefs->GetInteger(
                prefs::kLastNavigationCapturingMigrationState)),
            MigrationState::kDefaultOff);
}

IN_PROC_BROWSER_TEST_P(WebAppNavigationCapturingMigrationSuccessTest,
                       DISABLED_MigrationOnStartup) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  PreferredAppsListReadyWaiter(proxy->PreferredAppsList()).Wait();

  webapps::AppId app_a =
      FindAppIdByPath(provider(), "/web_apps/site_d/basic.html");
  webapps::AppId app_b =
      FindAppIdByPath(provider(), "/web_apps/standalone/basic.html");
  webapps::AppId app_c = FindAppIdByPath(provider(), "/web_apps/basic.html");

  PrefService* prefs = profile()->GetPrefs();

  // App A was migrated to preferred only if the flag state is `default-on`,
  // regardless of client_mode.
  if (GetParam() == "reimpl_default_on") {
    EXPECT_EQ(static_cast<MigrationState>(prefs->GetInteger(
                  prefs::kLastNavigationCapturingMigrationState)),
              MigrationState::kDefaultOn);
    EXPECT_TRUE(
        proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_a));
  } else if (GetParam() == "reimpl_on_via_client_mode") {
    EXPECT_EQ(static_cast<MigrationState>(prefs->GetInteger(
                  prefs::kLastNavigationCapturingMigrationState)),
              MigrationState::kOnViaClientMode);
    // App A is NOT preferred (standard web app, no client mode).
    EXPECT_FALSE(
        proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_a));
  }

  // App B remains preferred (user setting).
  EXPECT_TRUE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_b));

  // App C is now preferred (migrated because it has client mode, or because
  // it's a standard app under DefaultOn).
  EXPECT_TRUE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_c));

  // Backup contains App B.
  const base::ListValue& backup =
      prefs->GetList(prefs::kWebAppsPreviouslyAppSupportedLinks);
  EXPECT_EQ(backup.size(), 1u);
  EXPECT_EQ(backup[0].GetString(), app_b);
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppNavigationCapturingMigrationSuccessTest,
                         testing::Values("reimpl_default_on",
                                         "reimpl_on_via_client_mode"),
                         [](const testing::TestParamInfo<std::string>& info) {
                           return info.param == "reimpl_default_on"
                                      ? "DefaultOn"
                                      : "OnViaClientMode";
                         });

// Verifies the automated startup rollback routine for PWA navigation capturing
// user preferences across three successive browser sessions:
// 1. `PRE_PRE_*`: Establishes the initial baseline state under `default-off`
//    (`reimpl_default_off`). Installs test applications and sets a specific
//    app (App B) as preferred by the user.
// 2. `PRE_*`: Simulates an upgrade/restart where navigation capturing is
//    enabled (`GetParam()`, which is `default-on` or `on-via-client-mode`). The
//    browser automatically runs startup migration, makes valid applications
//    preferred, and stores a backup of the user's original preferred app
//    (App B).
// 3. Main test: Simulates a rollback where navigation capturing is
//    turned off again (`reimpl_default_off`). The browser detects the rollback,
//    restores the original preferred app (App B) from the backup preference,
//    and un-prefers any applications that were automatically migrated.
class WebAppNavigationCapturingMigrationRollbackTest
    : public WebAppBrowserTestBase,
      public testing::WithParamInterface<std::string> {
 public:
  WebAppNavigationCapturingMigrationRollbackTest() {
    std::string_view test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();
    if (test_name.starts_with("DISABLED_")) {
      test_name.remove_prefix(9);
    }

    std::string link_capturing_state = "reimpl_default_off";
    if (test_name.starts_with("PRE_") && !test_name.starts_with("PRE_PRE_")) {
      link_capturing_state = GetParam();
    }

    feature_list_.InitAndEnableFeatureWithParameters(
        features::kPwaNavigationCapturing,
        {{"link_capturing_state", link_capturing_state}});
  }

  WebAppNavigationCapturingMigrationRollbackTest(
      const WebAppNavigationCapturingMigrationRollbackTest&) = delete;
  WebAppNavigationCapturingMigrationRollbackTest& operator=(
      const WebAppNavigationCapturingMigrationRollbackTest&) = delete;
  ~WebAppNavigationCapturingMigrationRollbackTest() override = default;

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebAppNavigationCapturingMigrationRollbackTest,
                       DISABLED_PRE_PRE_RollbackOnStartup) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install App A.
  const GURL url_a =
      embedded_test_server()->GetURL("/web_apps/site_d/basic.html");
  webapps::AppId app_a = InstallWebAppFromManifest(browser(), url_a);
  apps::AppReadinessWaiter(profile(), app_a).Await();

  // Install App B.
  const GURL url_b =
      embedded_test_server()->GetURL("/web_apps/standalone/basic.html");
  webapps::AppId app_b = InstallWebAppFromManifest(browser(), url_b);
  apps::AppReadinessWaiter(profile(), app_b).Await();

  // Install App C.
  const GURL url_c = embedded_test_server()->GetURL(
      "/web_apps/get_manifest.html?"
      "launch_handler_client_mode_focus_existing.json");
  webapps::AppId app_c = InstallWebAppFromPage(browser(), url_c);
  apps::AppReadinessWaiter(profile(), app_c).Await();

  // Mark App B as user preferred before migration.
  apps_util::SetSupportedLinksPreferenceAndWait(profile(), app_b);

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  PreferredAppsListReadyWaiter(proxy->PreferredAppsList()).Wait();
  EXPECT_FALSE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_a));
  EXPECT_TRUE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_b));
  EXPECT_FALSE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_c));

  PrefService* prefs = profile()->GetPrefs();
  EXPECT_EQ(static_cast<MigrationState>(prefs->GetInteger(
                prefs::kLastNavigationCapturingMigrationState)),
            MigrationState::kDefaultOff);
}

IN_PROC_BROWSER_TEST_P(WebAppNavigationCapturingMigrationRollbackTest,
                       DISABLED_PRE_RollbackOnStartup) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  PreferredAppsListReadyWaiter(proxy->PreferredAppsList()).Wait();

  webapps::AppId app_a =
      FindAppIdByPath(provider(), "/web_apps/site_d/basic.html");
  webapps::AppId app_b =
      FindAppIdByPath(provider(), "/web_apps/standalone/basic.html");
  webapps::AppId app_c = FindAppIdByPath(provider(), "/web_apps/basic.html");

  PrefService* prefs = profile()->GetPrefs();
  if (GetParam() == "reimpl_default_on") {
    EXPECT_EQ(static_cast<MigrationState>(prefs->GetInteger(
                  prefs::kLastNavigationCapturingMigrationState)),
              MigrationState::kDefaultOn);
    EXPECT_TRUE(
        proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_a));
  } else if (GetParam() == "reimpl_on_via_client_mode") {
    EXPECT_EQ(static_cast<MigrationState>(prefs->GetInteger(
                  prefs::kLastNavigationCapturingMigrationState)),
              MigrationState::kOnViaClientMode);
    EXPECT_FALSE(
        proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_a));
  }

  EXPECT_TRUE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_b));
  EXPECT_TRUE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_c));

  // Backup contains App B.
  const base::ListValue& backup =
      prefs->GetList(prefs::kWebAppsPreviouslyAppSupportedLinks);
  EXPECT_EQ(backup.size(), 1u);
  EXPECT_EQ(backup[0].GetString(), app_b);
}

IN_PROC_BROWSER_TEST_P(WebAppNavigationCapturingMigrationRollbackTest,
                       DISABLED_RollbackOnStartup) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  PreferredAppsListReadyWaiter(proxy->PreferredAppsList()).Wait();

  webapps::AppId app_a =
      FindAppIdByPath(provider(), "/web_apps/site_d/basic.html");
  webapps::AppId app_b =
      FindAppIdByPath(provider(), "/web_apps/standalone/basic.html");
  webapps::AppId app_c = FindAppIdByPath(provider(), "/web_apps/basic.html");

  // Verify that the rollback ran automatically on startup!
  PrefService* prefs = profile()->GetPrefs();
  EXPECT_EQ(static_cast<MigrationState>(prefs->GetInteger(
                prefs::kLastNavigationCapturingMigrationState)),
            MigrationState::kDefaultOff);

  // App A and App C are no longer preferred (rolled back).
  EXPECT_FALSE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_a));
  EXPECT_FALSE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_c));

  // App B remains preferred (restored).
  EXPECT_TRUE(
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_b));

  // Backup cleared.
  EXPECT_FALSE(prefs->HasPrefPath(prefs::kWebAppsPreviouslyAppSupportedLinks));
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppNavigationCapturingMigrationRollbackTest,
                         testing::Values("reimpl_default_on",
                                         "reimpl_on_via_client_mode"),
                         [](const testing::TestParamInfo<std::string>& info) {
                           return info.param == "reimpl_default_on"
                                      ? "DefaultOn"
                                      : "OnViaClientMode";
                         });
}  // namespace web_app
