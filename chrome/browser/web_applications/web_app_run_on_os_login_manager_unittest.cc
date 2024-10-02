// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_run_on_os_login_manager.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "url/gurl.h"

namespace web_app {

namespace {

using PolicyRunOnOsLoginValue = std::string;

using TestParam = std::
    tuple<PolicyRunOnOsLoginValue, RunOnOsLoginMode, blink::mojom::DisplayMode>;

constexpr char kTestApp[] = "https://test.test/";

const char kWebAppSettings[] = R"([
  {
    "manifest_id": "https://test.test/",
    "run_on_os_login": "#"
  }
])";

class WebAppRunOnOsLoginManagerTest
    : public WebAppTest,
      public testing::WithParamInterface<TestParam> {
 public:
  void SetUp() override {
    BuildAndInitFeatureList();
    WebAppTest::SetUp();

    provider_ = FakeWebAppProvider::Get(profile());

    // Set up policy before managers are started.
    SetWebAppSettingsPref();

    static_cast<FakeWebAppUiManager*>(&provider_->GetUiManager())
        ->SetOnLaunchWebAppCallback(base::BindLambdaForTesting(
            [this](apps::AppLaunchParams params,
                   LaunchWebAppWindowSetting launch_setting) {
              launched_apps_.push_back(std::move(params));
            }));

    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    tester_ = std::make_unique<NotificationDisplayServiceTester>(
        /*profile=*/nullptr);
    tester_->SetNotificationAddedClosure(
        base::BindLambdaForTesting([this]() { notification_count_++; }));
    notification_count_ = 0u;

    // This test requires that a) the WebAppSettings are correctly read by the
    // WebAppPolicyManager and b) the PWA is installed before RunOnOsLogin
    // happens. WebAppSettings prefs can simply set before the WebAppProvider is
    // started, but the PWA needs to be installed after the sync bridge is ready
    // (on_sync_bridge_ready_) and before the RunOnOsLogin happens (after
    // on_external_managers_synchronized_). Instead, we delay the startup of the
    // WebAppRunOnOsLoginManager until all subsystems are ready and then
    // manually trigger the RunOnOsLogin, so that we can install a PWA before
    // that happens.
    provider_->GetWebAppRunOnOsLoginManager().SetSkipStartupForTesting(true);
    test::AwaitStartWebAppProviderAndSubsystems(profile());
    InstallWebApp();
    provider_->GetWebAppRunOnOsLoginManager().RunAppsOnOsLoginForTesting();
  }

  void TearDown() override {
    provider_->Shutdown();
    WebAppTest::TearDown();
  }

 protected:
  void BuildAndInitFeatureList() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    enabled_features.push_back(
        features::kDesktopPWAsEnforceWebAppSettingsPolicy);
    enabled_features.push_back(features::kDesktopPWAsRunOnOsLogin);
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void SetWebAppSettingsPref() {
    PolicyRunOnOsLoginValue policy = GetPolicyRunOnOsLoginValue();
    if (policy.empty()) {
      return;
    }

    std::string pref = std::string(kWebAppSettings);
    base::ReplaceChars(pref, "#", policy, &pref);

    auto result = base::JSONReader::ReadAndReturnValueWithError(
        pref, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    ASSERT_TRUE(result->is_list());
    profile()->GetPrefs()->Set(prefs::kWebAppSettings, std::move(*result));
  }

  void InstallWebApp() {
    std::unique_ptr<WebApp> web_app = test::CreateWebApp(
        GURL(kTestApp), WebAppManagement::Type::kWebAppStore);

    RunOnOsLoginMode run_mode = GetUserRunOnOsLoginMode();
    web_app->SetRunOnOsLoginMode(run_mode);

    blink::mojom::DisplayMode display_mode = GetDisplayMode();
    web_app->SetDisplayMode(display_mode);
    if (display_mode == DisplayMode::kBrowser) {
      web_app->SetUserDisplayMode(mojom::UserDisplayMode::kBrowser);
    }

    WebAppSyncBridge& sync_bridge = provider_->sync_bridge_unsafe();
    ScopedRegistryUpdate update(&sync_bridge);
    update->CreateApp(std::move(web_app));
  }

  PolicyRunOnOsLoginValue GetPolicyRunOnOsLoginValue() {
    return std::get<0>(GetParam());
  }

  RunOnOsLoginMode GetUserRunOnOsLoginMode() { return std::get<1>(GetParam()); }

  blink::mojom::DisplayMode GetDisplayMode() { return std::get<2>(GetParam()); }

  void AwaitAllCommandsComplete() {
    provider_->command_manager().AwaitAllCommandsCompleteForTesting();
  }

  const std::vector<apps::AppLaunchParams>& launched_apps() {
    return launched_apps_;
  }

  unsigned int notification_count_;
  std::unique_ptr<NotificationDisplayServiceTester> tester_;
  std::vector<apps::AppLaunchParams> launched_apps_;
  raw_ptr<FakeWebAppProvider> provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(WebAppRunOnOsLoginManagerTest, WebAppRunOnOsLogin) {
  bool launch_by_policy = GetPolicyRunOnOsLoginValue() == "run_windowed";
  bool launch_by_user_mode =
      GetPolicyRunOnOsLoginValue() != "blocked" &&
      GetUserRunOnOsLoginMode() != RunOnOsLoginMode::kNotRun;
  bool launched = launch_by_policy || launch_by_user_mode;

  auto launch_container = GetDisplayMode() == DisplayMode::kBrowser
                              ? apps::LaunchContainer::kLaunchContainerTab
                              : apps::LaunchContainer::kLaunchContainerWindow;

  AwaitAllCommandsComplete();

  const std::vector<apps::AppLaunchParams>& launched_apps_ = launched_apps();

  ASSERT_EQ(launched, !launched_apps_.empty());

  if (launched) {
    auto actual_container = launched_apps_[0].container;
    ASSERT_EQ(launch_container, actual_container);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ASSERT_EQ(notification_count_, 1u);
#endif
  }
}

INSTANTIATE_TEST_SUITE_P(
    WebAppRunOnOsLoginManagerTest,
    WebAppRunOnOsLoginManagerTest,
    testing::Combine(testing::Values("", "allowed", "blocked", "run_windowed"),
                     testing::Values(RunOnOsLoginMode::kNotRun,
                                     RunOnOsLoginMode::kWindowed,
                                     RunOnOsLoginMode::kMinimized),
                     testing::Values(blink::mojom::DisplayMode::kStandalone,
                                     blink::mojom::DisplayMode::kBrowser)),
    [](const ::testing::TestParamInfo<TestParam>& info) {
      auto policy = std::get<0>(info.param);
      auto user_mode = std::get<1>(info.param);
      auto display_mode = std::get<2>(info.param);

      std::string policy_name =
          policy.empty() ? "no_policy" : "policy_" + policy;
      std::string user_mode_name = "user_mode_";
      switch (user_mode) {
        case RunOnOsLoginMode::kNotRun:
          user_mode_name += "notRun";
          break;
        case RunOnOsLoginMode::kWindowed:
          user_mode_name += "windowed";
          break;
        case RunOnOsLoginMode::kMinimized:
          user_mode_name += "minimized";
          break;
      }
      std::string display_mode_name =
          display_mode == blink::mojom::DisplayMode::kStandalone ? "standalone"
                                                                 : "browser";

      return "Test_" + policy_name + "_" + user_mode_name + "_" +
             display_mode_name;
    });

}  // namespace

}  // namespace web_app
