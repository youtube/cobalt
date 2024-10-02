// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/arc_apps.h"

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/components/arc/test/fake_file_system_instance.h"
#include "ash/constants/ash_features.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/apps/app_service/publishers/arc_apps_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_list/arc/intent.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_bridge.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/intent_helper/intent_constants.h"
#include "components/arc/intent_helper/intent_filter.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/preferred_apps_list_handle.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"

namespace {

std::vector<arc::IntentFilter> CreateFilterList(
    const std::string& package_name,
    const std::vector<std::string>& authorities) {
  std::vector<arc::IntentFilter::AuthorityEntry> filter_authorities;
  for (const std::string& authority : authorities) {
    filter_authorities.emplace_back(authority, 0);
  }
  std::vector<arc::IntentFilter::PatternMatcher> patterns;
  patterns.emplace_back("/", arc::mojom::PatternType::PATTERN_PREFIX);

  auto filter = arc::IntentFilter(package_name, {arc::kIntentActionView},
                                  std::move(filter_authorities),
                                  std::move(patterns), {"https"}, {});
  std::vector<arc::IntentFilter> filters;
  filters.push_back(std::move(filter));
  return filters;
}

apps::IntentFilters CreateIntentFilters(
    const std::vector<std::string>& authorities) {
  apps::IntentFilters filters;
  apps::IntentFilterPtr filter = std::make_unique<apps::IntentFilter>();

  apps::ConditionValues values1;
  values1.push_back(std::make_unique<apps::ConditionValue>(
      apps_util::kIntentActionView, apps::PatternMatchType::kLiteral));
  filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kAction, std::move(values1)));

  apps::ConditionValues values2;
  values2.push_back(std::make_unique<apps::ConditionValue>(
      "https", apps::PatternMatchType::kLiteral));
  filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kScheme, std::move(values2)));

  apps::ConditionValues values;
  for (const std::string& authority : authorities) {
    values.push_back(std::make_unique<apps::ConditionValue>(
        authority, apps::PatternMatchType::kLiteral));
  }
  filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kHost, std::move(values)));

  apps::ConditionValues values3;
  values3.push_back(std::make_unique<apps::ConditionValue>(
      "/", apps::PatternMatchType::kPrefix));
  filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kPath, std::move(values3)));

  filters.push_back(std::move(filter));

  return filters;
}

// Returns a FileSystemURL, encoded as a GURL, that points to a file in the
// Downloads directory.
GURL FileInDownloads(Profile* profile, base::FilePath file) {
  url::Origin origin = file_manager::util::GetFilesAppOrigin();
  std::string mount_point_name =
      file_manager::util::GetDownloadsMountPointName(profile);
  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  mount_points->RegisterFileSystem(
      mount_point_name, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(),
      file_manager::util::GetDownloadsFolderForProfile(profile));
  return mount_points
      ->CreateExternalFileSystemURL(blink::StorageKey::CreateFirstParty(origin),
                                    mount_point_name, file)
      .ToGURL();
}

}  // namespace

class ArcAppsPublisherTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();

    // Do not destroy the ArcServiceManager during TearDown, so that Arc
    // KeyedServices can be correctly destroyed during profile shutdown.
    arc_test_.set_persist_service_manager(true);
    // We will manually start ArcApps after setting up IntentHelper, this allows
    // ArcApps to observe the correct IntentHelper during initialization.
    arc_test_.set_start_app_service_publisher(false);
    // We want to use the real ArcIntentHelper KeyedService so that it's the
    // same object that ArcApps uses.
    arc_test_.set_initialize_real_intent_helper_bridge(true);
    arc_test_.SetUp(profile());

    auto* arc_bridge_service =
        arc_test_.arc_service_manager()->arc_bridge_service();

    intent_helper_ =
        arc::ArcIntentHelperBridge::GetForBrowserContext(profile());
    arc_file_system_bridge_ = std::make_unique<arc::ArcFileSystemBridge>(
        profile(), arc_bridge_service);

    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

    app_service_test_.SetUp(&profile_);
    apps::ArcAppsFactory::GetForProfile(profile());
    // Ensure that the PreferredAppsList is fully initialized before running the
    // test.
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    arc_test_.StopArcInstance();
    apps::ArcAppsFactory::GetInstance()->ShutDownForTesting(profile());
    arc_test_.TearDown();
  }

  void VerifyIntentFilters(const std::string& app_id,
                           const std::vector<std::string>& authorities) {
    apps::IntentFilters source = CreateIntentFilters(authorities);

    apps::IntentFilters target;
    apps::AppServiceProxyFactory::GetForProfile(profile())
        ->AppRegistryCache()
        .ForOneApp(app_id, [&target](const apps::AppUpdate& update) {
          target = update.IntentFilters();
        });

    EXPECT_EQ(source.size(), target.size());
    for (size_t i = 0; i < source.size(); i++) {
      EXPECT_EQ(*source[i], *target[i]);
    }
  }

  void SetUpFileSystemInstance() {
    auto* arc_bridge_service =
        arc_test()->arc_service_manager()->arc_bridge_service();
    file_system_instance_ = std::make_unique<arc::FakeFileSystemInstance>();
    arc_bridge_service->file_system()->SetInstance(file_system_instance());
    arc::WaitForInstanceReady(arc_bridge_service->file_system());
  }

  TestingProfile* profile() { return &profile_; }

  apps::AppServiceProxy* app_service_proxy() {
    return apps::AppServiceProxyFactory::GetForProfile(profile());
  }

  arc::ArcIntentHelperBridge* intent_helper() { return intent_helper_; }

  arc::FakeIntentHelperInstance* intent_helper_instance() {
    return arc_test_.intent_helper_instance();
  }

  arc::FakeFileSystemInstance* file_system_instance() {
    return file_system_instance_.get();
  }

  ArcAppTest* arc_test() { return &arc_test_; }

  apps::PreferredAppsListHandle& preferred_apps() {
    return apps::AppServiceProxyFactory::GetForProfile(profile())
        ->PreferredAppsList();
  }

  std::vector<arc::mojom::SupportedLinksPackagePtr> CreateSupportedLinks(
      const std::string& package_name) {
    std::vector<arc::mojom::SupportedLinksPackagePtr> result;
    auto link = arc::mojom::SupportedLinksPackage::New();
    link->package_name = package_name;
    result.push_back(std::move(link));

    return result;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcAppTest arc_test_;
  TestingProfile profile_;
  apps::AppServiceTest app_service_test_;
  raw_ptr<arc::ArcIntentHelperBridge, ExperimentalAsh> intent_helper_;
  std::unique_ptr<arc::FakeFileSystemInstance> file_system_instance_;
  std::unique_ptr<arc::ArcFileSystemBridge> arc_file_system_bridge_;
};

// Verifies that a call to set the supported links preference from the ARC
// system doesn't change the setting in app service.
TEST_F(ArcAppsPublisherTest, SetSupportedLinksFromArcSystem) {
  constexpr char kTestAuthority[] = "www.example.com";
  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  // Update intent filters and supported links for the app, as if it was just
  // installed.
  intent_helper()->OnIntentFiltersUpdatedForPackage(
      package_name, CreateFilterList(package_name, {kTestAuthority}));
  VerifyIntentFilters(app_id, {kTestAuthority});
  intent_helper()->OnSupportedLinksChanged(
      CreateSupportedLinks(package_name), {},
      arc::mojom::SupportedLinkChangeSource::kArcSystem);

  ASSERT_EQ(absl::nullopt, preferred_apps().FindPreferredAppForUrl(
                               GURL("https://www.example.com/foo")));
}

// Verifies that a call to set the supported links preference from App Service
// syncs the setting to ARC.
TEST_F(ArcAppsPublisherTest, SetSupportedLinksFromAppService) {
  constexpr char kTestAuthority[] = "www.example.com";
  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);
  intent_helper()->OnIntentFiltersUpdatedForPackage(
      package_name, CreateFilterList(package_name, {kTestAuthority}));
  VerifyIntentFilters(app_id, {kTestAuthority});

  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->SetSupportedLinksPreference(app_id);

  ASSERT_TRUE(
      intent_helper_instance()->verified_links().find(package_name)->second);
}

// Verifies that the ARC system can still update preferred intent filters for
// apps which are already preferred.
TEST_F(ArcAppsPublisherTest, SetSupportedLinksAllowsUpdates) {
  constexpr char kTestAuthority[] = "www.example.com";
  constexpr char kTestAuthority2[] = "www.newexample.com";
  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  // Update intent filters and supported links for the app, as if it was just
  // installed.
  intent_helper()->OnIntentFiltersUpdatedForPackage(
      package_name, CreateFilterList(package_name, {kTestAuthority}));
  VerifyIntentFilters(app_id, {kTestAuthority});

  // Set a user preference for the app.
  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->SetSupportedLinksPreference(app_id);

  // Update filters with a new authority added.
  intent_helper()->OnIntentFiltersUpdatedForPackage(
      package_name,
      CreateFilterList(package_name, {kTestAuthority, kTestAuthority2}));
  VerifyIntentFilters(app_id, {kTestAuthority, kTestAuthority2});
  intent_helper()->OnSupportedLinksChanged(
      CreateSupportedLinks(package_name), {},
      arc::mojom::SupportedLinkChangeSource::kArcSystem);

  // Verify that the user preference has been extended to the new filter.
  ASSERT_EQ(app_id, preferred_apps().FindPreferredAppForUrl(
                        GURL("https://www.newexample.com/foo")));
}

// Verifies that the user can set an app as preferred through ARC settings.
TEST_F(ArcAppsPublisherTest, SetSupportedLinksAllowsUserChanges) {
  constexpr char kTestAuthority[] = "www.example.com";
  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  // Update intent filters and supported links for the app, as if it was just
  // installed.
  intent_helper()->OnIntentFiltersUpdatedForPackage(
      package_name, CreateFilterList(package_name, {kTestAuthority}));
  intent_helper()->OnSupportedLinksChanged(
      CreateSupportedLinks(package_name), {},
      arc::mojom::SupportedLinkChangeSource::kUserPreference);

  ASSERT_EQ(app_id, preferred_apps().FindPreferredAppForUrl(
                        GURL("https://www.example.com/foo")));
}

// Verifies that the Play Store app can be set as preferred by the system.
TEST_F(ArcAppsPublisherTest, SetSupportedLinksAllowsPlayStoreDefault) {
  constexpr char kTestAuthority[] = "play.google.com";

  std::vector<arc::mojom::AppInfoPtr> apps;
  apps.push_back(arc::mojom::AppInfo::New("Play Store", arc::kPlayStorePackage,
                                          arc::kPlayStoreActivity));
  arc_test()->app_instance()->SendRefreshAppList(apps);

  // Update intent filters and supported links for the app, as if it was just
  // installed.
  intent_helper()->OnIntentFiltersUpdatedForPackage(
      arc::kPlayStorePackage,
      CreateFilterList(arc::kPlayStorePackage, {kTestAuthority}));
  intent_helper()->OnSupportedLinksChanged(
      CreateSupportedLinks(arc::kPlayStorePackage), {},
      arc::mojom::SupportedLinkChangeSource::kArcSystem);

  ASSERT_EQ(arc::kPlayStoreAppId, preferred_apps().FindPreferredAppForUrl(
                                      GURL("https://play.google.com/foo")));
}

TEST_F(ArcAppsPublisherTest,
       LaunchAppWithIntent_EditIntent_SendsOpenUrlRequest) {
  SetUpFileSystemInstance();
  auto intent = apps_util::MakeEditIntent(
      FileInDownloads(profile(), base::FilePath("test.txt")), "text/plain");

  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  absl::optional<apps::State> result;
  app_service_proxy()->LaunchAppWithIntent(
      app_id, 0, std::move(intent), apps::LaunchSource::kFromFileManager,
      /*window_info=*/nullptr,
      base::BindLambdaForTesting(
          [&result](apps::LaunchResult&& callback_result) {
            result = callback_result.state;
          }));

  ASSERT_EQ(apps::State::SUCCESS, result.value_or(apps::State::FAILED));

  ASSERT_EQ(file_system_instance()->handledUrlRequests().size(), 1u);
  auto& url_request = file_system_instance()->handledUrlRequests()[0];
  ASSERT_EQ(url_request->action_type, arc::mojom::ActionType::EDIT);
  ASSERT_EQ(url_request->urls.size(), 1u);
  ASSERT_EQ(url_request->urls[0]->mime_type, "text/plain");
  ASSERT_TRUE(
      base::EndsWith(url_request->urls[0]->content_url.spec(), "test.txt"));
}

TEST_F(ArcAppsPublisherTest,
       LaunchAppWithIntent_EditIntent_NoArcFileSystem_ReturnsFalse) {
  // Do not start up ArcFileSystem, to simulate the intent being sent before ARC
  // starts.
  auto intent = apps_util::MakeEditIntent(
      FileInDownloads(profile(), base::FilePath("test.txt")), "text/plain");

  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  absl::optional<apps::State> result;
  app_service_proxy()->LaunchAppWithIntent(
      app_id, 0, std::move(intent), apps::LaunchSource::kFromFileManager,
      /*window_info=*/nullptr,
      base::BindLambdaForTesting(
          [&result](apps::LaunchResult&& callback_result) {
            result = callback_result.state;
          }));

  ASSERT_EQ(apps::State::FAILED, result.value_or(apps::State::SUCCESS));
}

TEST_F(
    ArcAppsPublisherTest,
    LaunchAppWithIntent_ViewFileIntent_SendsOpenUrlRequestWithIndividualFileMimeTypes) {
  SetUpFileSystemInstance();

  auto file1 = std::make_unique<apps::IntentFile>(
      FileInDownloads(profile(), base::FilePath("test1.png")));
  file1->mime_type = "image/png";

  auto file2 = std::make_unique<apps::IntentFile>(
      FileInDownloads(profile(), base::FilePath("test2.jpeg")));
  file2->mime_type = "image/jpeg";

  std::vector<apps::IntentFilePtr> files;
  files.push_back(std::move(file1));
  files.push_back(std::move(file2));

  auto intent = std::make_unique<apps::Intent>(apps_util::kIntentActionView,
                                               std::move(files));

  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  absl::optional<apps::State> result;
  app_service_proxy()->LaunchAppWithIntent(
      app_id, 0, std::move(intent), apps::LaunchSource::kFromFileManager,
      /*window_info=*/nullptr,
      base::BindLambdaForTesting(
          [&result](apps::LaunchResult&& callback_result) {
            result = callback_result.state;
          }));

  ASSERT_EQ(apps::State::SUCCESS, result.value_or(apps::State::FAILED));

  ASSERT_EQ(file_system_instance()->handledUrlRequests().size(), 1u);
  auto& url_request = file_system_instance()->handledUrlRequests()[0];
  ASSERT_EQ(url_request->action_type, arc::mojom::ActionType::VIEW);
  ASSERT_EQ(url_request->urls.size(), 2u);
  ASSERT_EQ(url_request->urls[0]->mime_type, "image/png");
  ASSERT_EQ(url_request->urls[1]->mime_type, "image/jpeg");
  ASSERT_TRUE(
      base::EndsWith(url_request->urls[0]->content_url.spec(), "test1.png"));
  ASSERT_TRUE(
      base::EndsWith(url_request->urls[1]->content_url.spec(), "test2.jpeg"));
}

TEST_F(ArcAppsPublisherTest,
       LaunchAppWithIntent_ShareFileIntent_SendsOpenUrlRequest) {
  SetUpFileSystemInstance();

  std::string mime_type = "image/jpeg";
  std::string file_name = "test.jpeg";

  GURL url = FileInDownloads(profile(), base::FilePath(file_name));
  auto intent = apps_util::MakeShareIntent({url}, {mime_type});

  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  absl::optional<apps::State> result;
  app_service_proxy()->LaunchAppWithIntent(
      app_id, 0, std::move(intent), apps::LaunchSource::kFromFileManager,
      /*window_info=*/nullptr,
      base::BindLambdaForTesting(
          [&result](apps::LaunchResult&& callback_result) {
            result = callback_result.state;
          }));

  ASSERT_EQ(apps::State::SUCCESS, result.value_or(apps::State::FAILED));

  ASSERT_EQ(file_system_instance()->handledUrlRequests().size(), 1u);
  auto& url_request = file_system_instance()->handledUrlRequests()[0];
  ASSERT_EQ(url_request->action_type, arc::mojom::ActionType::SEND);
  ASSERT_EQ(url_request->urls.size(), 1u);
  ASSERT_EQ(url_request->urls[0]->mime_type, mime_type);
  ASSERT_TRUE(
      base::EndsWith(url_request->urls[0]->content_url.spec(), file_name));
}

TEST_F(ArcAppsPublisherTest, LaunchAppWithIntent_ShareFilesIntent_SendsExtras) {
  SetUpFileSystemInstance();

  constexpr char kTestIntentText[] = "launch text";
  constexpr char kTestIntentTitle[] = "launch title";
  constexpr char kTestExtraKey[] = "extra_key";
  constexpr char kTestExtraValue[] = "extra_value";

  GURL url = FileInDownloads(profile(), base::FilePath("test.jpeg"));
  auto intent = apps_util::MakeShareIntent({url}, {"image/jpeg"},
                                           kTestIntentText, kTestIntentTitle);
  intent->extras = {std::make_pair(kTestExtraKey, kTestExtraValue)};

  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[0]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[0]->package_name,
                                                 fake_apps[0]->activity);
  arc_test()->app_instance()->SendRefreshAppList(fake_apps);

  app_service_proxy()->LaunchAppWithIntent(
      app_id, 0, std::move(intent), apps::LaunchSource::kFromFileManager,
      /*window_info=*/nullptr, base::DoNothing());

  ASSERT_EQ(file_system_instance()->handledUrlRequests().size(), 1u);
  auto& url_request = file_system_instance()->handledUrlRequests()[0];
  ASSERT_EQ(url_request->action_type, arc::mojom::ActionType::SEND);
  ASSERT_EQ(url_request->urls.size(), 1u);
  ASSERT_EQ(url_request->extras.value()[kTestExtraKey], kTestExtraValue);
  ASSERT_EQ(url_request->extras.value()["android.intent.extra.TEXT"],
            kTestIntentText);
  ASSERT_EQ(url_request->extras.value()["android.intent.extra.SUBJECT"],
            kTestIntentTitle);
}

TEST_F(ArcAppsPublisherTest, OnInstallationStarted_RegistersPromiseApp) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(ash::features::kPromiseIcons);
  app_service_proxy()->ReinitializeForTesting(profile());
  apps::PromiseAppRegistryCache* cache =
      app_service_proxy()->PromiseAppRegistryCache();

  std::string package_name = "com.example.this";
  apps::PackageId package_id =
      apps::PackageId(apps::AppType::kArc, package_name);

  // Verify that the promise app is not yet registered.
  const apps::PromiseApp* promise_app_before =
      cache->GetPromiseAppForTesting(package_id);
  EXPECT_FALSE(promise_app_before);

  arc_test()->app_instance()->SendInstallationStarted(package_name);

  // Verify that the promise app is now registered.
  const apps::PromiseApp* promise_app_after =
      cache->GetPromiseAppForTesting(package_id);
  EXPECT_TRUE(promise_app_after);
}
