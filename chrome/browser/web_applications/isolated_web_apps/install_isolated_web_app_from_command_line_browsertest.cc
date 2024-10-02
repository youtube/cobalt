// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_from_command_line.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace web_app {
namespace {
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsTrue;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::VariantWith;

class InstallIsolatedWebAppFromCommandLineBrowserTest
    : public WebAppControllerBrowserTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode}, {});
    WebAppControllerBrowserTest::SetUp();
  }

  WebAppRegistrar& GetWebAppRegistrar() {
    auto* provider = WebAppProvider::GetForTest(browser()->profile());
    CHECK(provider != nullptr);
    return provider->registrar_unsafe();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class InstallIsolatedWebAppFromCommandLineFromUrlBrowserTest
    : public InstallIsolatedWebAppFromCommandLineBrowserTest {
 protected:
  void SetUp() override {
    embedded_test_server()->AddDefaultHandlers(
        GetChromeTestDataDir().AppendASCII("web_apps/simple_isolated_app"));
    ASSERT_TRUE(embedded_test_server()->Start());

    InstallIsolatedWebAppFromCommandLineBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    CHECK(command_line != nullptr);

    command_line->AppendSwitchASCII("install-isolated-web-app-from-url",
                                    GetAppUrl().spec());
    InstallIsolatedWebAppFromCommandLineBrowserTest::SetUpCommandLine(
        command_line);
  }

  GURL GetAppUrl() const { return embedded_test_server()->base_url(); }
};

IN_PROC_BROWSER_TEST_F(InstallIsolatedWebAppFromCommandLineFromUrlBrowserTest,
                       AppFromCommandLineIsInstalled) {
  WebAppTestInstallObserver observer(browser()->profile());
  AppId id = observer.BeginListeningAndWait();

  ASSERT_THAT(GetWebAppRegistrar().IsInstalled(id), IsTrue());

  EXPECT_THAT(
      GetWebAppRegistrar().GetAppById(id),
      Pointee(Property(
          &WebApp::isolation_data,
          Optional(Field(&WebApp::IsolationData::location,
                         VariantWith<DevModeProxy>(
                             Field(&DevModeProxy::proxy_url,
                                   Eq(url::Origin::Create(GetAppUrl())))))))));
}

class InstallIsolatedWebAppFromCommandLineFromFileBrowserTest
    : public InstallIsolatedWebAppFromCommandLineBrowserTest {
 protected:
  void SetUp() override {
    CHECK(scoped_temp_dir_.CreateUniqueTempDir());
    signed_web_bundle_path_ = scoped_temp_dir_.GetPath().Append(
        base::FilePath::FromASCII("test-bundle.swbn"));
    TestSignedWebBundle bundle = BuildDefaultTestSignedWebBundle();
    bundle_id_ = std::make_unique<web_package::SignedWebBundleId>(bundle.id);
    CHECK(base::WriteFile(signed_web_bundle_path_, bundle.data));

    InstallIsolatedWebAppFromCommandLineBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    CHECK(command_line != nullptr);
    command_line->AppendSwitchPath("install-isolated-web-app-from-file",
                                   signed_web_bundle_path_);
    InstallIsolatedWebAppFromCommandLineBrowserTest::SetUpCommandLine(
        command_line);
  }

  base::ScopedTempDir scoped_temp_dir_;
  base::FilePath signed_web_bundle_path_;
  std::unique_ptr<web_package::SignedWebBundleId> bundle_id_;
};

IN_PROC_BROWSER_TEST_F(InstallIsolatedWebAppFromCommandLineFromFileBrowserTest,
                       AppFromCommandLineIsInstalled) {
  WebAppTestInstallObserver observer(browser()->profile());
  AppId id = observer.BeginListeningAndWait();

  ASSERT_TRUE(bundle_id_);
  ASSERT_EQ(
      id,
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(*bundle_id_).app_id());
  ASSERT_THAT(GetWebAppRegistrar().IsInstalled(id), IsTrue());

  EXPECT_THAT(
      GetWebAppRegistrar().GetAppById(id),
      Pointee(Property(&WebApp::isolation_data,
                       Optional(Field(&WebApp::IsolationData::location,
                                      VariantWith<DevModeBundle>(Field(
                                          &DevModeBundle::path,
                                          Eq(signed_web_bundle_path_))))))));
}

}  // namespace
}  // namespace web_app
