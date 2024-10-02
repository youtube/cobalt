// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_managed_app_manager.h"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/externally_managed_app_manager_impl.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/fake_externally_managed_app_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/install_result_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

class ExternallyManagedAppManagerTest
    : public WebAppTest,
      public testing::WithParamInterface<test::ExternalPrefMigrationTestCases> {
 public:
  ExternallyManagedAppManagerTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    switch (GetParam()) {
      case test::ExternalPrefMigrationTestCases::kDisableMigrationReadPref:
        disabled_features.push_back(features::kMigrateExternalPrefsToWebAppDB);
        disabled_features.push_back(
            features::kUseWebAppDBInsteadOfExternalPrefs);
        break;
      case test::ExternalPrefMigrationTestCases::kDisableMigrationReadDB:
        disabled_features.push_back(features::kMigrateExternalPrefsToWebAppDB);
        enabled_features.push_back(
            features::kUseWebAppDBInsteadOfExternalPrefs);
        break;
      case test::ExternalPrefMigrationTestCases::kEnableMigrationReadPref:
        enabled_features.push_back(features::kMigrateExternalPrefsToWebAppDB);
        disabled_features.push_back(
            features::kUseWebAppDBInsteadOfExternalPrefs);
        break;
      case test::ExternalPrefMigrationTestCases::kEnableMigrationReadDB:
        enabled_features.push_back(features::kMigrateExternalPrefsToWebAppDB);
        enabled_features.push_back(
            features::kUseWebAppDBInsteadOfExternalPrefs);
        break;
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 protected:
  void SetUp() override {
    WebAppTest::SetUp();
    provider_ = web_app::FakeWebAppProvider::Get(profile());
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

    externally_installed_app_prefs_ =
        std::make_unique<ExternallyInstalledWebAppPrefs>(profile()->GetPrefs());

    externally_managed_app_manager().SetHandleInstallRequestCallback(
        base::BindLambdaForTesting(
            [this](const ExternalInstallOptions& install_options)
                -> ExternallyManagedAppManager::InstallResult {
              const GURL& install_url = install_options.install_url;
              if (!app_registrar().GetAppById(GenerateAppId(
                      /*manifest_id=*/absl::nullopt, install_url))) {
                std::unique_ptr<WebApp> web_app =
                    test::CreateWebApp(install_url, WebAppManagement::kDefault);
                web_app->AddInstallURLToManagementExternalConfigMap(
                    WebAppManagement::kDefault, install_url);
                {
                  ScopedRegistryUpdate update(&provider().sync_bridge_unsafe());
                  update->CreateApp(std::move(web_app));
                }

                externally_installed_app_prefs().Insert(
                    install_url,
                    GenerateAppId(/*manifest_id=*/absl::nullopt, install_url),
                    install_options.install_source);
                ++deduped_install_count_;
              }
              return ExternallyManagedAppManager::InstallResult(
                  webapps::InstallResultCode::kSuccessNewInstall);
            }));
    externally_managed_app_manager().SetHandleUninstallRequestCallback(
        base::BindLambdaForTesting(
            [this](const GURL& app_url,
                   ExternalInstallSource install_source) -> bool {
              absl::optional<AppId> app_id =
                  app_registrar().LookupExternalAppId(app_url);
              if (app_id.has_value()) {
                ScopedRegistryUpdate update(&provider().sync_bridge_unsafe());
                update->DeleteApp(app_id.value());
                deduped_uninstall_count_++;
              }
              return true;
            }));
  }

  void ForceSystemShutdown() { provider_->Shutdown(); }

  void Sync(const std::vector<GURL>& urls) {
    ResetCounts();

    std::vector<ExternalInstallOptions> install_options_list;
    install_options_list.reserve(urls.size());
    for (const auto& url : urls) {
      install_options_list.emplace_back(
          url, mojom::UserDisplayMode::kStandalone,
          ExternalInstallSource::kInternalDefault);
    }

    base::RunLoop run_loop;
    externally_managed_app_manager().SynchronizeInstalledApps(
        std::move(install_options_list),
        ExternalInstallSource::kInternalDefault,
        base::BindLambdaForTesting(
            [&run_loop, urls](
                std::map<GURL, ExternallyManagedAppManager::InstallResult>
                    install_results,
                std::map<GURL, bool> uninstall_results) { run_loop.Quit(); }));
    // Wait for SynchronizeInstalledApps to finish.
    run_loop.Run();
  }

  void Expect(int deduped_install_count,
              int deduped_uninstall_count,
              const std::vector<GURL>& installed_app_urls) {
    EXPECT_EQ(deduped_install_count, deduped_install_count_);
    EXPECT_EQ(deduped_uninstall_count, deduped_uninstall_count_);
    base::flat_map<AppId, base::flat_set<GURL>> apps =
        app_registrar().GetExternallyInstalledApps(
            ExternalInstallSource::kInternalDefault);
    std::vector<GURL> urls;
    for (const auto& it : apps) {
      base::ranges::copy(it.second, std::back_inserter(urls));
    }

    std::sort(urls.begin(), urls.end());
    EXPECT_EQ(installed_app_urls, urls);
  }

  void ResetCounts() {
    deduped_install_count_ = 0;
    deduped_uninstall_count_ = 0;
  }

  WebAppProvider& provider() { return *provider_; }

  WebAppRegistrar& app_registrar() { return provider().registrar_unsafe(); }

  ExternallyInstalledWebAppPrefs& externally_installed_app_prefs() {
    return *externally_installed_app_prefs_;
  }

  FakeExternallyManagedAppManager& externally_managed_app_manager() {
    return static_cast<FakeExternallyManagedAppManager&>(
        provider().externally_managed_app_manager());
  }

 private:
  int deduped_install_count_ = 0;
  int deduped_uninstall_count_ = 0;

  raw_ptr<FakeWebAppProvider> provider_;

  std::unique_ptr<ExternallyInstalledWebAppPrefs>
      externally_installed_app_prefs_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that destroying ExternallyManagedAppManager during a synchronize call
// that installs an app doesn't crash. Regression test for
// https://crbug.com/962808
TEST_P(ExternallyManagedAppManagerTest, DestroyDuringInstallInSynchronize) {
  std::vector<ExternalInstallOptions> install_options_list;
  install_options_list.emplace_back(GURL("https://foo.example"),
                                    mojom::UserDisplayMode::kStandalone,
                                    ExternalInstallSource::kInternalDefault);
  install_options_list.emplace_back(GURL("https://bar.example"),
                                    mojom::UserDisplayMode::kStandalone,
                                    ExternalInstallSource::kInternalDefault);

  externally_managed_app_manager().SynchronizeInstalledApps(
      std::move(install_options_list), ExternalInstallSource::kInternalDefault,
      // ExternallyManagedAppManager gives no guarantees about whether its
      // pending callbacks will be run or not when it gets destroyed.
      base::DoNothing());
  ForceSystemShutdown();
  base::RunLoop().RunUntilIdle();
}

// Test that destroying ExternallyManagedAppManager during a synchronize call
// that uninstalls an app doesn't crash. Regression test for
// https://crbug.com/962808
TEST_P(ExternallyManagedAppManagerTest, DestroyDuringUninstallInSynchronize) {
  // Install an app that will be uninstalled next.
  {
    std::vector<ExternalInstallOptions> install_options_list;
    install_options_list.emplace_back(GURL("https://foo.example"),
                                      mojom::UserDisplayMode::kStandalone,
                                      ExternalInstallSource::kInternalDefault);
    base::RunLoop run_loop;
    externally_managed_app_manager().SynchronizeInstalledApps(
        std::move(install_options_list),
        ExternalInstallSource::kInternalDefault,
        base::BindLambdaForTesting(
            [&](std::map<GURL, ExternallyManagedAppManager::InstallResult>
                    install_results,
                std::map<GURL, bool> uninstall_results) { run_loop.Quit(); }));
    run_loop.Run();
  }

  externally_managed_app_manager().SynchronizeInstalledApps(
      std::vector<ExternalInstallOptions>(),
      ExternalInstallSource::kInternalDefault,
      // ExternallyManagedAppManager gives no guarantees about whether its
      // pending callbacks will be run or not when it gets destroyed.
      base::DoNothing());
  ForceSystemShutdown();
  base::RunLoop().RunUntilIdle();
}

TEST_P(ExternallyManagedAppManagerTest, SynchronizeInstalledApps) {
  GURL a("https://a.example.com/");
  GURL b("https://b.example.com/");
  GURL c("https://c.example.com/");
  GURL d("https://d.example.com/");
  GURL e("https://e.example.com/");

  Sync(std::vector<GURL>{a, b, d});
  Expect(3, 0, std::vector<GURL>{a, b, d});

  Sync(std::vector<GURL>{b, e});
  Expect(1, 2, std::vector<GURL>{b, e});

  Sync(std::vector<GURL>{e});
  Expect(0, 1, std::vector<GURL>{e});

  Sync(std::vector<GURL>{c});
  Expect(1, 1, std::vector<GURL>{c});

  Sync(std::vector<GURL>{e, a, d});
  Expect(3, 1, std::vector<GURL>{a, d, e});

  Sync(std::vector<GURL>{c, a, b, d, e});
  Expect(2, 0, std::vector<GURL>{a, b, c, d, e});

  Sync(std::vector<GURL>{});
  Expect(0, 5, std::vector<GURL>{});

  // The remaining code tests duplicate inputs.

  Sync(std::vector<GURL>{b, a, b, c});
  Expect(3, 0, std::vector<GURL>{a, b, c});

  Sync(std::vector<GURL>{e, a, e, e, e, a});
  Expect(1, 2, std::vector<GURL>{a, e});

  Sync(std::vector<GURL>{b, c, d});
  Expect(3, 2, std::vector<GURL>{b, c, d});

  Sync(std::vector<GURL>{a, a, a, a, a, a});
  Expect(1, 3, std::vector<GURL>{a});

  Sync(std::vector<GURL>{});
  Expect(0, 1, std::vector<GURL>{});
}

#if BUILDFLAG(IS_CHROMEOS)
using ExternallyManagedAppManagerTestAndroidSMS =
    ExternallyManagedAppManagerTest;
// This test verifies that AndroidSMS is not uninstalled during the Syncing
// process.
TEST_P(ExternallyManagedAppManagerTestAndroidSMS,
       SynchronizeAppsAndroidSMSTest) {
  GURL android_sms_url1(
      "https://messages-web.sandbox.google.com/web/authentication");
  GURL android_sms_url2("https://messages.google.com/web/authentication");
  GURL extra_url("https://extra.com/");

  // Install all URLs first.
  Sync(std::vector<GURL>{android_sms_url1, android_sms_url2, extra_url});
  Expect(/*deduped_install_count=*/3, /*deduped_uninstall_count=*/0,
         std::vector<GURL>{extra_url, android_sms_url1, android_sms_url2});

  // Assume that extra_url is the only URL desired.
  // install_count = 0 as no new installs happen.
  // uninstall_count = 0 as android sms URLs does not get uninstalled.
  // Both android SMS URLs remain.
  Sync(std::vector<GURL>{extra_url});
  Expect(/*deduped_install_count=*/0, /*deduped_uninstall_count=*/0,
         std::vector<GURL>{extra_url, android_sms_url1, android_sms_url2});

  // Assume that android_sms_url1 is only required.
  // install_count = 0 as no new installs happen.
  // uninstall_count = 1 as extra.com gets uninstalled.
  // Both android SMS URLs remain.
  Sync(std::vector<GURL>{android_sms_url1});
  Expect(/*deduped_install_count=*/0, /*deduped_uninstall_count=*/1,
         std::vector<GURL>{android_sms_url1, android_sms_url2});

  // Assume that no URL is required.
  // install_count = 0 as no new installs happen.
  // uninstall_count = 0 as android sms URLs does not get uninstalled.
  // Both android SMS URLs remain.
  Sync(std::vector<GURL>{});
  Expect(/*deduped_install_count=*/0, /*deduped_uninstall_count=*/0,
         std::vector<GURL>{android_sms_url1, android_sms_url2});
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ExternallyManagedAppManagerTestAndroidSMS,
    ::testing::Values(
        test::ExternalPrefMigrationTestCases::kDisableMigrationReadPref,
        test::ExternalPrefMigrationTestCases::kDisableMigrationReadDB,
        test::ExternalPrefMigrationTestCases::kEnableMigrationReadPref,
        test::ExternalPrefMigrationTestCases::kEnableMigrationReadDB),
    test::GetExternalPrefMigrationTestName);

#endif

INSTANTIATE_TEST_SUITE_P(
    All,
    ExternallyManagedAppManagerTest,
    ::testing::Values(
        test::ExternalPrefMigrationTestCases::kDisableMigrationReadPref,
        test::ExternalPrefMigrationTestCases::kDisableMigrationReadDB,
        test::ExternalPrefMigrationTestCases::kEnableMigrationReadPref,
        test::ExternalPrefMigrationTestCases::kEnableMigrationReadDB),
    test::GetExternalPrefMigrationTestName);

namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

// Test harness that keep the system as real as possible.
class ExternallyAppManagerTest : public WebAppTest {
 public:
  using InstallResults = std::map<GURL /*install_url*/,
                                  ExternallyManagedAppManager::InstallResult>;
  using UninstallResults = std::map<GURL /*install_url*/, bool /*succeeded*/>;
  using SynchronizeFuture =
      base::test::TestFuture<InstallResults, UninstallResults>;

  ExternallyAppManagerTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kUseWebAppDBInsteadOfExternalPrefs},
        {features::kMigrateExternalPrefsToWebAppDB});
  }

  void SetUp() override {
    WebAppTest::SetUp();
    provider_ = web_app::FakeWebAppProvider::Get(profile());
    std::unique_ptr<ExternallyManagedAppManagerImpl> external_app_manager =
        std::make_unique<ExternallyManagedAppManagerImpl>(profile());
    external_app_manager->SetUrlLoaderForTesting(
        web_contents_manager_.CreateUrlLoader());

    external_app_manager->SetDataRetrieverFactoryForTesting(
        base::BindLambdaForTesting(
            [fake_web_contents = web_contents_manager_.GetWeakPtr()]() {
              CHECK(fake_web_contents);
              return fake_web_contents->CreateDataRetriever();
            }));
    provider_->SetExternallyManagedAppManager(std::move(external_app_manager));
    // TODO(http://b/278922549): Disable the external management apps so we
    // don't compete with the policy app manager for our installs /
    // synchronization.
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    provider().Shutdown();
    WebAppTest::TearDown();
  }

  std::vector<ExternalInstallOptions> CreateExternalInstallOptionsFromTemplate(
      std::vector<GURL> install_urls,
      ExternalInstallSource source,
      absl::optional<ExternalInstallOptions> template_options = absl::nullopt) {
    std::vector<ExternalInstallOptions> output;
    std::transform(
        install_urls.begin(), install_urls.end(), std::back_inserter(output),
        [source, &template_options](const GURL& install_url) {
          ExternalInstallOptions options = template_options.value_or(
              ExternalInstallOptions(install_url, absl::nullopt, source));
          options.install_url = install_url;
          options.install_source = source;
          return options;
        });
    return output;
  }

  WebAppProvider& provider() { return *provider_; }

  WebAppRegistrar& app_registrar() { return provider().registrar_unsafe(); }

  ExternallyManagedAppManagerImpl& external_mananager() {
    return static_cast<ExternallyManagedAppManagerImpl&>(
        provider().externally_managed_app_manager());
  }

  FakeWebContentsManager& web_contents_manager() {
    return web_contents_manager_;
  }

  AppId PopulateBasicInstallPageWithManifest(GURL install_url,
                                             GURL manifest_url,
                                             GURL start_url) {
    auto& install_page_state =
        web_contents_manager().GetOrCreatePageState(install_url);
    install_page_state.url_load_result = WebAppUrlLoaderResult::kUrlLoaded;

    install_page_state.page_install_info =
        std::make_unique<WebAppInstallInfo>();
    install_page_state.page_install_info->title = u"Basic app title";

    install_page_state.manifest_url = manifest_url;
    install_page_state.valid_manifest_for_web_app = true;

    install_page_state.opt_manifest = blink::mojom::Manifest::New();
    install_page_state.opt_manifest->scope =
        url::Origin::Create(start_url).GetURL();
    install_page_state.opt_manifest->start_url = start_url;
    install_page_state.opt_manifest->display =
        blink::mojom::DisplayMode::kStandalone;
    install_page_state.opt_manifest->short_name = u"Basic app name";

    return GenerateAppId(/*manifest_id=*/absl::nullopt, start_url);
  }

 private:
  raw_ptr<FakeWebAppProvider> provider_;
  FakeWebContentsManager web_contents_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ExternallyAppManagerTest, NoNetworkNoPlaceholder) {
  const GURL kInstallUrl = GURL("https://www.example.com/install_url.html");

  // Not populating the `FakeWebContentsManager` means it treats the network as
  // non-functional / not available.

  SynchronizeFuture result;
  provider().externally_managed_app_manager().SynchronizeInstalledApps(
      CreateExternalInstallOptionsFromTemplate(
          {kInstallUrl}, ExternalInstallSource::kExternalPolicy),
      ExternalInstallSource::kExternalPolicy, result.GetCallback());
  ASSERT_TRUE(result.Wait());

  // Empty uninstall results.
  EXPECT_THAT(result.Get<UninstallResults>(), IsEmpty());

  // Install should have failed.
  std::map<GURL, ExternallyManagedAppManager::InstallResult> install_results =
      result.Get<InstallResults>();
  EXPECT_THAT(install_results,
              ElementsAre(std::make_pair(
                  kInstallUrl,
                  ExternallyManagedAppManager::InstallResult(
                      webapps::InstallResultCode::kInstallURLLoadFailed))));
}

TEST_F(ExternallyAppManagerTest, SimpleInstall) {
  const GURL kStartUrl = GURL("https://www.example.com/index.html");
  const GURL kInstallUrl =
      GURL("https://www.example.com/nested/install_url.html");
  const GURL kManifestUrl = GURL("https://www.example.com/manifest.json");

  AppId app_id = PopulateBasicInstallPageWithManifest(kInstallUrl, kManifestUrl,
                                                      kStartUrl);

  SynchronizeFuture result;
  provider().externally_managed_app_manager().SynchronizeInstalledApps(
      CreateExternalInstallOptionsFromTemplate(
          {kInstallUrl}, ExternalInstallSource::kExternalPolicy),
      ExternalInstallSource::kExternalPolicy, result.GetCallback());
  ASSERT_TRUE(result.Wait());

  // Empty uninstall results.
  EXPECT_THAT(result.Get<UninstallResults>(), IsEmpty());

  // Install should succeed.
  std::map<GURL, ExternallyManagedAppManager::InstallResult> install_results =
      result.Get<InstallResults>();
  EXPECT_THAT(
      install_results,
      ElementsAre(std::make_pair(
          kInstallUrl,
          ExternallyManagedAppManager::InstallResult(
              webapps::InstallResultCode::kSuccessNewInstall, app_id))));
}

TEST_F(ExternallyAppManagerTest, TwoInstallUrlsSameApp) {
  const GURL kStartUrl = GURL("https://www.example.com/index.html");
  const GURL kInstallUrl1 =
      GURL("https://www.example.com/nested/install_url.html");
  const GURL kInstallUrl2 =
      GURL("https://www.example.com/nested/install_url2.html");
  const GURL kManifestUrl = GURL("https://www.example.com/manifest.json");

  AppId app_id = PopulateBasicInstallPageWithManifest(kInstallUrl1,
                                                      kManifestUrl, kStartUrl);
  AppId app_id2 = PopulateBasicInstallPageWithManifest(kInstallUrl2,
                                                       kManifestUrl, kStartUrl);
  EXPECT_EQ(app_id, app_id2);

  SynchronizeFuture result;
  provider().externally_managed_app_manager().SynchronizeInstalledApps(
      CreateExternalInstallOptionsFromTemplate(
          {kInstallUrl1, kInstallUrl2}, ExternalInstallSource::kExternalPolicy),
      ExternalInstallSource::kExternalPolicy, result.GetCallback());
  ASSERT_TRUE(result.Wait());

  // Empty uninstall results.
  EXPECT_THAT(result.Get<UninstallResults>(), IsEmpty());

  // Installs should have both succeeded.
  std::map<GURL, ExternallyManagedAppManager::InstallResult> install_results =
      result.Get<InstallResults>();
  EXPECT_THAT(
      install_results,
      UnorderedElementsAre(
          std::make_pair(
              kInstallUrl1,
              ExternallyManagedAppManager::InstallResult(
                  webapps::InstallResultCode::kSuccessNewInstall, app_id)),
          std::make_pair(
              kInstallUrl2,
              ExternallyManagedAppManager::InstallResult(
                  webapps::InstallResultCode::kSuccessNewInstall, app_id))));

  EXPECT_EQ(app_registrar().GetAppIds().size(), 1ul);
  const WebApp* app = app_registrar().GetAppById(app_id);
  ASSERT_TRUE(app);
  EXPECT_THAT(app->management_to_external_config_map(),
              ElementsAre(std::make_pair(
                  WebAppManagement::kPolicy,
                  WebApp::ExternalManagementConfig(
                      /*is_placeholder=*/false,
                      /*install_urls=*/{kInstallUrl1, kInstallUrl2},
                      /*additional_policy_ids=*/{}))));
}

TEST_F(ExternallyAppManagerTest, InstallUrlChanges) {
  const GURL kStartUrl = GURL("https://www.example.com/index.html");
  const GURL kInstallUrl =
      GURL("https://www.example.com/nested/install_url.html");
  const GURL kInstallUrl2 =
      GURL("https://www.example.com/nested/install_url2.html");
  const GURL kManifestUrl = GURL("https://www.example.com/manifest.json");

  AppId app_id = PopulateBasicInstallPageWithManifest(kInstallUrl, kManifestUrl,
                                                      kStartUrl);
  AppId app_id2 = PopulateBasicInstallPageWithManifest(kInstallUrl2,
                                                       kManifestUrl, kStartUrl);
  EXPECT_EQ(app_id, app_id2);

  // First synchronize will install the app.
  {
    SynchronizeFuture result;
    provider().externally_managed_app_manager().SynchronizeInstalledApps(
        CreateExternalInstallOptionsFromTemplate(
            {kInstallUrl}, ExternalInstallSource::kExternalPolicy),
        ExternalInstallSource::kExternalPolicy, result.GetCallback());
    ASSERT_TRUE(result.Wait());

    std::map<GURL, ExternallyManagedAppManager::InstallResult> install_results =
        result.Get<InstallResults>();
    EXPECT_THAT(
        install_results,
        ElementsAre(std::make_pair(
            kInstallUrl,
            ExternallyManagedAppManager::InstallResult(
                webapps::InstallResultCode::kSuccessNewInstall, app_id))));
  }

  // Second synchronize with a different install url should succeed and update
  // the install urls correctly.
  {
    SynchronizeFuture result;
    provider().externally_managed_app_manager().SynchronizeInstalledApps(
        CreateExternalInstallOptionsFromTemplate(
            {kInstallUrl2}, ExternalInstallSource::kExternalPolicy),
        ExternalInstallSource::kExternalPolicy, result.GetCallback());
    ASSERT_TRUE(result.Wait());
    std::map<GURL, ExternallyManagedAppManager::InstallResult> install_results =
        result.Get<InstallResults>();
    EXPECT_THAT(
        install_results,
        ElementsAre(std::make_pair(
            kInstallUrl2,
            ExternallyManagedAppManager::InstallResult(
                webapps::InstallResultCode::kSuccessNewInstall, app_id))));

    ASSERT_THAT(
        result.Get<UninstallResults>(),
        testing::UnorderedElementsAre(std::make_pair(kInstallUrl, true)));
  }

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  ASSERT_TRUE(app);
  EXPECT_THAT(app->management_to_external_config_map(),
              ElementsAre(std::make_pair(WebAppManagement::kPolicy,
                                         WebApp::ExternalManagementConfig(
                                             /*is_placeholder=*/false,
                                             /*install_urls=*/{kInstallUrl2},
                                             /*additional_policy_ids=*/{}))));
}

TEST_F(ExternallyAppManagerTest, PolicyAppOverridesUserInstalledApp) {
  const GURL kStartUrl = GURL("https://www.example.com/index.html");
  const GURL kInstallUrl =
      GURL("https://www.example.com/nested/install_url.html");
  const GURL kManifestUrl = GURL("https://www.example.com/manifest.json");

  AppId app_id = PopulateBasicInstallPageWithManifest(kInstallUrl, kManifestUrl,
                                                      kStartUrl);

  {
    // Install user app
    auto& install_page_state =
        web_contents_manager().GetOrCreatePageState(kInstallUrl);
    install_page_state.opt_manifest->short_name = u"Test user app";

    auto install_info = std::make_unique<WebAppInstallInfo>();
    install_info->start_url = kStartUrl;
    install_info->title = u"Test user app";
    absl::optional<AppId> user_app_id =
        test::InstallWebApp(profile(), std::move(install_info));

    ASSERT_TRUE(user_app_id.has_value());
    ASSERT_EQ(user_app_id.value(), app_id);
    ASSERT_TRUE(app_registrar().WasInstalledByUser(app_id));
    ASSERT_FALSE(app_registrar().HasExternalApp(app_id));
    ASSERT_EQ("Test user app", app_registrar().GetAppShortName(app_id));
  }
  {
    // Install policy app
    auto& install_page_state =
        web_contents_manager().GetOrCreatePageState(kInstallUrl);
    install_page_state.opt_manifest->short_name = u"Test policy app";

    SynchronizeFuture result;
    provider().externally_managed_app_manager().SynchronizeInstalledApps(
        CreateExternalInstallOptionsFromTemplate(
            {kInstallUrl}, ExternalInstallSource::kExternalPolicy),
        ExternalInstallSource::kExternalPolicy, result.GetCallback());
    ASSERT_TRUE(result.Wait());
    std::map<GURL, ExternallyManagedAppManager::InstallResult> install_results =
        result.Get<InstallResults>();
    EXPECT_THAT(
        install_results,
        ElementsAre(std::make_pair(
            kInstallUrl,
            ExternallyManagedAppManager::InstallResult(
                webapps::InstallResultCode::kSuccessNewInstall, app_id))));
    ASSERT_EQ("Test policy app", app_registrar().GetAppShortName(app_id));
  }
}

TEST_F(ExternallyAppManagerTest, NoNetworkWithPlaceholder) {
  const GURL kInstallUrl = GURL("https://www.example.com/install_url.html");
  ExternalInstallOptions template_options(
      GURL(), mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalPolicy);
  template_options.install_placeholder = true;

  SynchronizeFuture result;
  provider().externally_managed_app_manager().SynchronizeInstalledApps(
      CreateExternalInstallOptionsFromTemplate(
          {kInstallUrl}, ExternalInstallSource::kExternalPolicy,
          template_options),
      ExternalInstallSource::kExternalPolicy, result.GetCallback());
  ASSERT_TRUE(result.Wait());

  // The AppId should be created from teh install url.
  AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, kInstallUrl);

  // Install should succeed.
  std::map<GURL, ExternallyManagedAppManager::InstallResult> install_results =
      result.Get<InstallResults>();
  EXPECT_THAT(
      install_results,
      ElementsAre(std::make_pair(
          kInstallUrl,
          ExternallyManagedAppManager::InstallResult(
              webapps::InstallResultCode::kSuccessNewInstall, app_id))));

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  ASSERT_TRUE(app);
  EXPECT_THAT(app->management_to_external_config_map(),
              ElementsAre(std::make_pair(WebAppManagement::kPolicy,
                                         WebApp::ExternalManagementConfig(
                                             /*is_placeholder=*/true,
                                             /*install_urls=*/{kInstallUrl},
                                             /*additional_policy_ids=*/{}))));
}

TEST_F(ExternallyAppManagerTest, RedirectInstallUrlPlaceholder) {
  const GURL kInstallUrl = GURL("https://www.example.com/install_url.html");
  const GURL kRedirectToUrl =
      GURL("https://www.otherorigin.com/redirected.html");
  ExternalInstallOptions template_options(
      GURL(), mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalPolicy);
  template_options.install_placeholder = true;

  auto& page_state = web_contents_manager().GetOrCreatePageState(kInstallUrl);
  page_state.redirection_url = kRedirectToUrl;

  SynchronizeFuture result;
  provider().externally_managed_app_manager().SynchronizeInstalledApps(
      CreateExternalInstallOptionsFromTemplate(
          {kInstallUrl}, ExternalInstallSource::kExternalPolicy,
          template_options),
      ExternalInstallSource::kExternalPolicy, result.GetCallback());
  ASSERT_TRUE(result.Wait());

  // The AppId should be created from teh install url.
  AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, kInstallUrl);

  // Install should succeed.
  std::map<GURL, ExternallyManagedAppManager::InstallResult> install_results =
      result.Get<InstallResults>();
  EXPECT_THAT(
      install_results,
      ElementsAre(std::make_pair(
          kInstallUrl,
          ExternallyManagedAppManager::InstallResult(
              webapps::InstallResultCode::kSuccessNewInstall, app_id))));

  const WebApp* app = provider().registrar_unsafe().GetAppById(app_id);
  ASSERT_TRUE(app);
  EXPECT_THAT(app->management_to_external_config_map(),
              ElementsAre(std::make_pair(WebAppManagement::kPolicy,
                                         WebApp::ExternalManagementConfig(
                                             /*is_placeholder=*/true,
                                             /*install_urls=*/{kInstallUrl},
                                             /*additional_policy_ids=*/{}))));
}

}  // namespace
}  // namespace web_app
