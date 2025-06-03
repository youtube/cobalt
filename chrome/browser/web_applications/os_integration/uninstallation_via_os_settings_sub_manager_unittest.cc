// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/uninstallation_via_os_settings_sub_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/install_result_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

class UninstallationViaOsSettingsSubManagerTest
    : public WebAppTest,
      public ::testing::WithParamInterface<OsIntegrationSubManagersState> {
 public:
  const GURL kWebAppUrl = GURL("https://example.com/path/index.html");

  UninstallationViaOsSettingsSubManagerTest() = default;
  ~UninstallationViaOsSettingsSubManagerTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_ =
          OsIntegrationTestOverrideImpl::OverrideForTesting(base::GetHomeDir());
    }
    if (GetParam() == OsIntegrationSubManagersState::kSaveStateToDB) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kOsIntegrationSubManagers, {{"stage", "write_config"}});
    } else if (GetParam() ==
               OsIntegrationSubManagersState::kSaveStateAndExecute) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kOsIntegrationSubManagers,
          {{"stage", "execute_and_write_config"}});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{features::kOsIntegrationSubManagers});
    }

    provider_ = FakeWebAppProvider::Get(profile());

    auto file_handler_manager =
        std::make_unique<WebAppFileHandlerManager>(profile());
    auto protocol_handler_manager =
        std::make_unique<WebAppProtocolHandlerManager>(profile());
    auto shortcut_manager = std::make_unique<WebAppShortcutManager>(
        profile(), file_handler_manager.get(), protocol_handler_manager.get());
    auto os_integration_manager = std::make_unique<OsIntegrationManager>(
        profile(), std::move(shortcut_manager), std::move(file_handler_manager),
        std::move(protocol_handler_manager), /*url_handler_manager=*/nullptr);

    provider_->SetOsIntegrationManager(std::move(os_integration_manager));
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    // Blocking required due to file operations in the shortcut override
    // destructor.
    test::UninstallAllWebApps(profile());
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_.reset();
    }
    WebAppTest::TearDown();
  }

  webapps::AppId InstallWebApp(webapps::WebappInstallSource install_source) {
    std::unique_ptr<WebAppInstallInfo> info =
        std::make_unique<WebAppInstallInfo>();
    info->start_url = kWebAppUrl;
    info->title = u"Test App";
    info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
    auto source = install_source;
    base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
        result;
    // InstallFromInfoWithParams is used instead of InstallFromInfo, because
    // InstallFromInfo doesn't register OS integration.
    provider().scheduler().InstallFromInfoWithParams(
        std::move(info), /*overwrite_existing_manifest_fields=*/true, source,
        result.GetCallback(), WebAppInstallParams());
    bool success = result.Wait();
    EXPECT_TRUE(success);
    if (!success) {
      return webapps::AppId();
    }
    EXPECT_EQ(result.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessNewInstall);
    return result.Get<webapps::AppId>();
  }

  OsIntegrationTestOverrideImpl& test_override() const {
    return *test_override_->test_override;
  }

 protected:
  WebAppProvider& provider() { return *provider_; }

 private:
  raw_ptr<FakeWebAppProvider, DanglingUntriaged> provider_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<OsIntegrationTestOverrideImpl::BlockingRegistration>
      test_override_;
};

bool IsOsUninstallationSupported() {
#if BUILDFLAG(IS_WIN)
  return true;
#else
  return false;
#endif
}

TEST_P(UninstallationViaOsSettingsSubManagerTest, TestUserUninstallable) {
  const webapps::AppId& app_id =
      InstallWebApp(webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  if (AreOsIntegrationSubManagersEnabled()) {
    EXPECT_EQ(
        IsOsUninstallationSupported(),
        os_integration_state.uninstall_registration().registered_with_os());
  } else {
    ASSERT_FALSE(os_integration_state.has_uninstall_registration());
  }
#if BUILDFLAG(IS_WIN)
  base::expected<bool, std::string> result =
      test_override().IsUninstallRegisteredWithOs(app_id, "Test App",
                                                  profile());
  ASSERT_TRUE(result.has_value())
      << "Error parsing os integration: " << result.error();
  EXPECT_TRUE(result.value());
#endif
}

TEST_P(UninstallationViaOsSettingsSubManagerTest, TestNotUserUninstallable) {
  const webapps::AppId& app_id =
      InstallWebApp(webapps::WebappInstallSource::EXTERNAL_POLICY);

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  if (AreOsIntegrationSubManagersEnabled()) {
    EXPECT_FALSE(
        os_integration_state.uninstall_registration().registered_with_os());
  } else {
    ASSERT_FALSE(os_integration_state.has_uninstall_registration());
  }
  if (IsOsUninstallationSupported()) {
    ASSERT_FALSE(
        os_integration_state.uninstall_registration().registered_with_os());
  }
#if BUILDFLAG(IS_WIN)
  base::expected<bool, std::string> result =
      test_override().IsUninstallRegisteredWithOs(app_id, "Test App",
                                                  profile());
  ASSERT_TRUE(result.has_value())
      << "Error parsing os integration: " << result.error();
  EXPECT_FALSE(result.value());
#endif
}

TEST_P(UninstallationViaOsSettingsSubManagerTest, UninstallApp) {
  const webapps::AppId& app_id =
      InstallWebApp(webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
  test::UninstallAllWebApps(profile());
  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_FALSE(state.has_value());
}

// Testing crbug.com/1434577, that OS states can be cleaned up even after
// the app has been uninstalled.
TEST_P(UninstallationViaOsSettingsSubManagerTest,
       OsStatesCleanupAfterAppUninstallation) {
  const webapps::AppId& app_id =
      InstallWebApp(webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  if (AreOsIntegrationSubManagersEnabled()) {
    EXPECT_EQ(
        IsOsUninstallationSupported(),
        os_integration_state.uninstall_registration().registered_with_os());
  } else {
    ASSERT_FALSE(os_integration_state.has_uninstall_registration());
  }

  if (AreSubManagersExecuteEnabled()) {
#if BUILDFLAG(IS_WIN)
    base::expected<bool, std::string> install_result =
        test_override().IsUninstallRegisteredWithOs(app_id, "Test App",
                                                    profile());
    ASSERT_TRUE(install_result.has_value())
        << "Error parsing os integration: " << install_result.error();
    EXPECT_TRUE(install_result.value());

    // Verify that OS integration is bypassed but the app is
    // uninstalled, leading to left over OS states.
    absl::optional<OsIntegrationManager::ScopedSuppressForTesting>
        scoped_supress = absl::nullopt;
    scoped_supress.emplace();
    test::UninstallAllWebApps(profile());
    base::expected<bool, std::string> output_result =
        test_override().IsUninstallRegisteredWithOs(app_id, "Test App",
                                                    profile());
    ASSERT_TRUE(output_result.has_value())
        << "Error parsing os integration: " << output_result.error();
    EXPECT_TRUE(output_result.value());

    // Call OS integration again with the force_unregister_os_integration flag
    // set to true.
    SynchronizeOsOptions options;
    options.force_unregister_os_integration = true;
    test::SynchronizeOsIntegration(profile(), app_id, options);

    // OS Uninstallation entries should no longer exist in the registry.
    base::expected<bool, std::string> final_result =
        test_override().IsUninstallRegisteredWithOs(app_id, "Test App",
                                                    profile());
    ASSERT_TRUE(final_result.has_value())
        << "Error parsing os integration: " << final_result.error();
    EXPECT_FALSE(final_result.value());
    scoped_supress.reset();
#endif
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    UninstallationViaOsSettingsSubManagerTest,
    ::testing::Values(OsIntegrationSubManagersState::kDisabled,
                      OsIntegrationSubManagersState::kSaveStateToDB,
                      OsIntegrationSubManagersState::kSaveStateAndExecute),
    test::GetOsIntegrationSubManagersTestName);

}  // namespace

}  // namespace web_app
