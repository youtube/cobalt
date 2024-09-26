// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/web_applications/commands/manifest_update_finalize_command.h"
#include "chrome/browser/web_applications/manifest_update_utils.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

class ManifestUpdateFinalizeCommandTest : public WebAppTest {
 public:
  ManifestUpdateFinalizeCommandTest() = default;
  ManifestUpdateFinalizeCommandTest(const ManifestUpdateFinalizeCommandTest&) =
      delete;
  ManifestUpdateFinalizeCommandTest& operator=(
      const ManifestUpdateFinalizeCommandTest&) = delete;
  ~ManifestUpdateFinalizeCommandTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

 protected:
  std::unique_ptr<ManifestUpdateFinalizeCommand> CreateCommand(
      const GURL& url,
      const AppId& app_id,
      WebAppInstallInfo install_info,
      ManifestUpdateFinalizeCommand::ManifestWriteCallback callback) {
    auto keep_alive = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::APP_MANIFEST_UPDATE, KeepAliveRestartOption::DISABLED);
    auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
        profile(), ProfileKeepAliveOrigin::kWebAppUpdate);
    return std::make_unique<ManifestUpdateFinalizeCommand>(
        url, app_id, std::move(install_info), std::move(callback),
        std::move(keep_alive), std::move(profile_keep_alive));
  }

  ManifestUpdateResult RunCommandAndGetResult(const GURL& url,
                                              const AppId& app_id,
                                              WebAppInstallInfo install_info) {
    ManifestUpdateResult output_result;
    base::RunLoop loop;
    provider().command_manager().ScheduleCommand(CreateCommand(
        url, app_id, std::move(install_info),
        base::BindLambdaForTesting([&](const GURL& url,
                                       const AppId& output_app_id,
                                       ManifestUpdateResult result) {
          EXPECT_EQ(output_app_id, app_id);
          output_result = result;
          loop.Quit();
        })));
    loop.Run();
    return output_result;
  }

  AppId InstallWebApp() {
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = app_url();
    web_app_info->scope = app_url().GetWithoutFilename();
    web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
    web_app_info->title = u"Foo Bar";
    return test::InstallWebApp(profile(), std::move(web_app_info));
  }

  WebAppInstallInfo GetNewInstallInfoWithTitle(std::u16string new_title) {
    WebAppInstallInfo info;
    info.start_url = app_url();
    info.scope = app_url().GetWithoutFilename();
    info.user_display_mode = mojom::UserDisplayMode::kStandalone;
    info.title = new_title;
    return info;
  }

  WebAppProvider& provider() { return *WebAppProvider::GetForTest(profile()); }
  WebAppRegistrar& registrar() { return provider().registrar_unsafe(); }
  GURL app_url() { return app_url_; }

 private:
  const GURL app_url_{"http://www.foo.bar/web_apps/basic.html"};
};

TEST_F(ManifestUpdateFinalizeCommandTest, NameUpdate) {
  AppId app_id = InstallWebApp();
  ManifestUpdateResult expected_result = RunCommandAndGetResult(
      app_url(), app_id, GetNewInstallInfoWithTitle(u"New Name"));
  EXPECT_EQ(expected_result, ManifestUpdateResult::kAppUpdated);
  EXPECT_EQ(registrar().GetAppById(app_id)->untranslated_name(), "New Name");
}

TEST_F(ManifestUpdateFinalizeCommandTest, UpdateFailsOnUnsuccessfulCode) {
  ManifestUpdateResult expected_result =
      RunCommandAndGetResult(app_url(), "RandomAppId", WebAppInstallInfo());
  EXPECT_EQ(expected_result, ManifestUpdateResult::kAppUpdateFailed);
}

}  // namespace web_app
