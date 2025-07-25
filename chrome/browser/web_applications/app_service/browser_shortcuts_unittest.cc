// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/web_applications/app_service/browser_shortcuts.h"

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_constants.h"

namespace {
const char kUrl[] = "https://example.com/";
}

namespace web_app {

class BrowserShortcutsTest : public testing::Test,
                             public apps::ShortcutRegistryCache::Observer {
 public:
  // testing::Test implementation.
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kCrosWebAppShortcutUiUpdate);
    profile_ = std::make_unique<TestingProfile>();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  std::string CreateShortcut(const std::string& shortcut_name) {
    const GURL kAppUrl(kUrl);

    // Create a web app entry without scope, which would be recognised
    // as ShortcutApp in the web app system.
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->title = base::UTF8ToUTF16(shortcut_name);
    web_app_info->start_url = kAppUrl;

    std::string app_id =
        test::InstallWebApp(profile(), std::move(web_app_info),
                            /*overwrite_existing_manifest_fields=*/true);
    CHECK(
        WebAppProvider::GetForTest(profile())->registrar_unsafe().IsShortcutApp(
            app_id));
    return app_id;
  }

  std::string CreateWebApp(const std::string& app_name) {
    const GURL kAppUrl(kUrl);

    // Create a web app entry with scope, which would be recognised
    // as normal web app in the web app system.
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->title = base::UTF8ToUTF16(app_name);
    web_app_info->start_url = kAppUrl;
    web_app_info->scope = kAppUrl;

    std::string app_id =
        test::InstallWebApp(profile(), std::move(web_app_info),
                            /*overwrite_existing_manifest_fields=*/true);
    CHECK(!WebAppProvider::GetForTest(profile())
               ->registrar_unsafe()
               .IsShortcutApp(app_id));
    return app_id;
  }

  void InitializeBrowserShortcutPublisher() {
    base::RunLoop run_loop;
    web_app::BrowserShortcuts::SetInitializedCallbackForTesting(
        run_loop.QuitClosure());
    apps::AppServiceTest app_service_test;
    app_service_test.SetUp(profile());
    shortcut_registry_cache_observation_.Observe(
        apps::AppServiceProxyFactory::GetForProfile(profile())
            ->ShortcutRegistryCache());
    run_loop.Run();
  }

  Profile* profile() { return profile_.get(); }

  void SetOnShortcutRemovedCallback(
      base::OnceCallback<void(apps::ShortcutId)> callback) {
    on_shortcut_removed_callback_ = std::move(callback);
  }

 private:
  void OnShortcutUpdated(const apps::ShortcutUpdate& update) override {}

  void OnShortcutRemoved(const apps::ShortcutId& id) override {
    if (on_shortcut_removed_callback_) {
      std::move(on_shortcut_removed_callback_).Run(id);
    }
  }

  void OnShortcutRegistryCacheWillBeDestroyed(
      apps::ShortcutRegistryCache* cache) override {
    shortcut_registry_cache_observation_.Reset();
  }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  base::test::ScopedFeatureList scoped_feature_list_;

  base::OnceCallback<void(apps::ShortcutId)> on_shortcut_removed_callback_;

  base::ScopedObservation<apps::ShortcutRegistryCache,
                          apps::ShortcutRegistryCache::Observer>
      shortcut_registry_cache_observation_{this};
};

TEST_F(BrowserShortcutsTest, PublishExistingBrowserShortcut) {
  const std::string kShortcutName = "Shortcut";

  auto local_shortcut_id = CreateShortcut(kShortcutName);
  apps::ShortcutId expected_shortcut_id =
      apps::GenerateShortcutId(app_constants::kChromeAppId, local_shortcut_id);

  // For web app based browser shortcut, we just use the local_id
  // that is generated in the web app system, so that we can keep
  // all the launcher and shelf locations without needing to migrate the sync
  // data.
  ASSERT_EQ(local_shortcut_id, expected_shortcut_id.value());

  InitializeBrowserShortcutPublisher();

  apps::ShortcutRegistryCache* cache =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->ShortcutRegistryCache();
  ASSERT_EQ(cache->GetAllShortcuts().size(), 1u);
  ASSERT_TRUE(cache->HasShortcut(expected_shortcut_id));

  apps::ShortcutView stored_shortcut = cache->GetShortcut(expected_shortcut_id);
  ASSERT_TRUE(stored_shortcut);
  EXPECT_EQ(stored_shortcut->shortcut_id, expected_shortcut_id);
  EXPECT_EQ(stored_shortcut->name, "Shortcut");
  EXPECT_EQ(stored_shortcut->shortcut_source, apps::ShortcutSource::kUser);
  EXPECT_EQ(stored_shortcut->host_app_id, app_constants::kChromeAppId);
  EXPECT_EQ(stored_shortcut->local_id, local_shortcut_id);
  EXPECT_TRUE(stored_shortcut->icon_key.has_value());
  EXPECT_EQ(stored_shortcut->icon_key->icon_effects,
            apps::IconEffects::kCrOsStandardMask);
}

TEST_F(BrowserShortcutsTest, WebAppNotPublishedAsShortcut) {
  auto app_id = CreateWebApp("App");

  InitializeBrowserShortcutPublisher();

  apps::ShortcutRegistryCache* cache =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->ShortcutRegistryCache();
  EXPECT_EQ(cache->GetAllShortcuts().size(), 0u);

  auto new_app_id = CreateWebApp("NewApp");
  EXPECT_EQ(cache->GetAllShortcuts().size(), 0u);
}

TEST_F(BrowserShortcutsTest, PublishNewBrowserShortcut) {
  InitializeBrowserShortcutPublisher();
  apps::ShortcutRegistryCache* cache =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->ShortcutRegistryCache();
  ASSERT_EQ(cache->GetAllShortcuts().size(), 0u);

  const std::string kShortcutName = "Shortcut";

  auto local_shortcut_id = CreateShortcut(kShortcutName);
  apps::ShortcutId expected_shortcut_id =
      apps::GenerateShortcutId(app_constants::kChromeAppId, local_shortcut_id);

  // For web app based browser shortcut, we just use the local_id
  // that is generated in the web app system, so that we can keep
  // all the launcher and shelf locations without needing to migrate the sync
  // data.
  ASSERT_EQ(local_shortcut_id, expected_shortcut_id.value());

  ASSERT_EQ(cache->GetAllShortcuts().size(), 1u);
  ASSERT_TRUE(cache->HasShortcut(expected_shortcut_id));

  apps::ShortcutView stored_shortcut = cache->GetShortcut(expected_shortcut_id);
  ASSERT_TRUE(stored_shortcut);
  EXPECT_EQ(stored_shortcut->shortcut_id, expected_shortcut_id);
  EXPECT_EQ(stored_shortcut->name, "Shortcut");
  EXPECT_EQ(stored_shortcut->shortcut_source, apps::ShortcutSource::kUser);
  EXPECT_EQ(stored_shortcut->host_app_id, app_constants::kChromeAppId);
  EXPECT_EQ(stored_shortcut->local_id, local_shortcut_id);
  EXPECT_TRUE(stored_shortcut->icon_key.has_value());
  EXPECT_EQ(stored_shortcut->icon_key->icon_effects,
            apps::IconEffects::kCrOsStandardMask);
}

TEST_F(BrowserShortcutsTest, LaunchShortcut) {
  const std::string kShortcutName = "Shortcut";

  auto local_shortcut_id = CreateShortcut(kShortcutName);
  apps::ShortcutId expected_shortcut_id =
      apps::GenerateShortcutId(app_constants::kChromeAppId, local_shortcut_id);
  InitializeBrowserShortcutPublisher();

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  std::vector<apps::AppPtr> deltas;
  deltas.push_back(apps::AppPublisher::MakeApp(
      apps::AppType::kChromeApp, app_constants::kChromeAppId,
      apps::Readiness::kReady, "Chrome", apps::InstallReason::kUser,
      apps::InstallSource::kSystem));
  proxy->OnApps(std::move(deltas), apps::AppType::kChromeApp,
                /* should_notify_initialized */ true);

  base::test::TestFuture<apps::AppLaunchParams, LaunchWebAppWindowSetting>
      future;
  static_cast<FakeWebAppUiManager*>(
      &WebAppProvider::GetForTest(profile())->ui_manager())
      ->SetOnLaunchWebAppCallback(future.GetRepeatingCallback());
  int64_t display_id = display::kInvalidDisplayId;
  proxy->LaunchShortcut(expected_shortcut_id, display_id);
  apps::AppLaunchParams expected_params(
      local_shortcut_id, apps::LaunchContainer::kLaunchContainerTab,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      apps::LaunchSource::kFromAppListGrid, display_id);

  const apps::AppLaunchParams& params = future.Get<0>();
  LaunchWebAppWindowSetting setting = future.Get<1>();

  EXPECT_EQ(expected_params.app_id, params.app_id);
  EXPECT_EQ(expected_params.container, params.container);
  EXPECT_EQ(expected_params.disposition, params.disposition);
  EXPECT_EQ(expected_params.launch_source, params.launch_source);
  EXPECT_EQ(expected_params.display_id, params.display_id);

  EXPECT_EQ(setting, LaunchWebAppWindowSetting::kUseLaunchParams);
}

TEST_F(BrowserShortcutsTest, ShortcutRemoved) {
  InitializeBrowserShortcutPublisher();
  apps::ShortcutRegistryCache* cache =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->ShortcutRegistryCache();
  ASSERT_EQ(cache->GetAllShortcuts().size(), 0u);

  const std::string kShortcutName = "Shortcut";

  auto local_shortcut_id = CreateShortcut(kShortcutName);
  apps::ShortcutId expected_shortcut_id =
      apps::GenerateShortcutId(app_constants::kChromeAppId, local_shortcut_id);

  ASSERT_EQ(cache->GetAllShortcuts().size(), 1u);
  ASSERT_TRUE(cache->HasShortcut(expected_shortcut_id));

  test::UninstallWebApp(profile(), local_shortcut_id);

  EXPECT_EQ(cache->GetAllShortcuts().size(), 0u);
  EXPECT_FALSE(cache->HasShortcut(expected_shortcut_id));
}

TEST_F(BrowserShortcutsTest, RemoveShortcut) {
  const std::string kShortcutName = "Shortcut";

  auto local_shortcut_id = CreateShortcut(kShortcutName);
  apps::ShortcutId shortcut_id =
      apps::GenerateShortcutId(app_constants::kChromeAppId, local_shortcut_id);
  InitializeBrowserShortcutPublisher();

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  std::vector<apps::AppPtr> deltas;
  deltas.push_back(apps::AppPublisher::MakeApp(
      apps::AppType::kChromeApp, app_constants::kChromeAppId,
      apps::Readiness::kReady, "Chrome", apps::InstallReason::kUser,
      apps::InstallSource::kSystem));
  proxy->OnApps(std::move(deltas), apps::AppType::kChromeApp,
                /* should_notify_initialized */ true);

  base::test::TestFuture<apps::ShortcutId> future;

  SetOnShortcutRemovedCallback(future.GetCallback());
  proxy->RemoveShortcutSilently(shortcut_id, apps::UninstallSource::kUnknown);

  apps::ShortcutId removed_shortcut_id = future.Get();
  EXPECT_EQ(removed_shortcut_id, shortcut_id);
}

TEST_F(BrowserShortcutsTest, GetCompressedShortcutIcon) {
  const std::string kShortcutName = "Shortcut";

  auto local_shortcut_id = CreateShortcut(kShortcutName);
  apps::ShortcutId shortcut_id =
      apps::GenerateShortcutId(app_constants::kChromeAppId, local_shortcut_id);
  InitializeBrowserShortcutPublisher();

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());

  // Add icon in icon manager.
  IconBitmaps icon_bitmaps;
  constexpr int icon_size = 1000;
  web_app::AddGeneratedIcon(&icon_bitmaps.any, icon_size, SK_ColorGREEN);
  base::test::TestFuture<bool> future;
  WebAppProvider::GetForTest(profile())->icon_manager().WriteData(
      local_shortcut_id, std::move(icon_bitmaps), {}, {}, future.GetCallback());
  bool success = future.Get();
  EXPECT_TRUE(success);

  FakeWebAppProvider* fake_provider =
      static_cast<FakeWebAppProvider*>(WebAppProvider::GetForTest(profile()));
  WebApp* web_app =
      fake_provider->GetRegistrarMutable().GetAppByIdMutable(local_shortcut_id);
  web_app->SetDownloadedIconSizes(IconPurpose::ANY, {icon_size});

  ASSERT_TRUE(WebAppProvider::GetForTest(profile())->icon_manager().HasIcons(
      local_shortcut_id, IconPurpose::ANY, {icon_size}));

  base::test::TestFuture<apps::IconValuePtr> expect_result;
  apps::GetWebAppCompressedIconData(profile(), local_shortcut_id, icon_size,
                                    ui::ResourceScaleFactor::k100Percent,
                                    expect_result.GetCallback());
  apps::IconValuePtr expected_icon = expect_result.Take();

  base::test::TestFuture<apps::IconValuePtr> result;
  auto* shortcut_publisher =
      proxy->GetShortcutPublisherForTesting(apps::AppType::kChromeApp);
  shortcut_publisher->GetCompressedIconData(
      shortcut_id.value(), icon_size, ui::ResourceScaleFactor::k100Percent,
      result.GetCallback());
  apps::IconValuePtr icon = result.Take();
  ASSERT_EQ(expected_icon->compressed, icon->compressed);
}

TEST_F(BrowserShortcutsTest, ReplaceBetweenShortcutAndWebApp) {
  InitializeBrowserShortcutPublisher();
  apps::ShortcutRegistryCache* shortcut_cache =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->ShortcutRegistryCache();
  apps::AppRegistryCache& app_cache =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->AppRegistryCache();
  ASSERT_EQ(shortcut_cache->GetAllShortcuts().size(), 0u);

  auto web_app_id = CreateShortcut("Shortcut");
  EXPECT_TRUE(shortcut_cache->HasShortcut(apps::ShortcutId(web_app_id)));
  EXPECT_FALSE(app_cache.IsAppInstalled(web_app_id));

  // Install a web app with same url should replace the shortcut.
  CreateWebApp("App");
  EXPECT_FALSE(shortcut_cache->HasShortcut(apps::ShortcutId(web_app_id)));
  EXPECT_TRUE(app_cache.IsAppInstalled(web_app_id));

  // Create a shortcut with same url should replace the web app.
  CreateShortcut("Shortcut");
  EXPECT_TRUE(shortcut_cache->HasShortcut(apps::ShortcutId(web_app_id)));
  EXPECT_FALSE(app_cache.IsAppInstalled(web_app_id));
}

}  // namespace web_app
