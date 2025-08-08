// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/external_app_resolution_command.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/test/to_vector.h"
#include "chrome/browser/web_applications/commands/callback_command.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/fake_data_retriever.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "net/http/http_status_code.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {
namespace {

struct PageStateOptions {
  bool empty_web_app_info = false;
  WebAppUrlLoaderResult url_load_result = WebAppUrlLoaderResult::kUrlLoaded;
};

class ExternalAppResolutionCommandTest : public WebAppTest {
 public:
  const GURL kWebAppUrl = GURL("https://example.com/path/index.html");
  const GURL kWebAppScope = GURL("https://example.com/path/");
  const webapps::AppId kWebAppId =
      GenerateAppId(/*manifest_id_path=*/absl::nullopt, kWebAppUrl);
  const GURL kWebAppManifestUrl =
      GURL("https://example.com/path/manifest.json");

  using BitmapData = std::map<SquareSizePx, SkBitmap>;

  ExternallyManagedAppManager::InstallResult InstallAndWait(
      const ExternalInstallOptions& install_options,
      std::unique_ptr<WebAppDataRetriever> data_retriever = nullptr) {
    base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;

    webapps::AppId placeholder_app_id =
        GenerateAppId(absl::nullopt, install_options.install_url);
    const bool is_placeholder_installed = registrar().IsPlaceholderApp(
        placeholder_app_id,
        ConvertExternalInstallSourceToSource(install_options.install_source));
    std::unique_ptr<ExternalAppResolutionCommand> command =
        std::make_unique<ExternalAppResolutionCommand>(
            *profile(), install_options,
            is_placeholder_installed
                ? absl::optional<webapps::AppId>(placeholder_app_id)
                : absl::nullopt,
            future.GetCallback());
    if (data_retriever) {
      command->SetDataRetrieverForTesting(std::move(data_retriever));
    }
    provider()->command_manager().ScheduleCommand(std::move(command));
    return future.Get<0>();
  }

  blink::mojom::ManifestPtr CreateValidManifest() {
    blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
    manifest->name = u"Example App";
    manifest->short_name = u"App";
    manifest->start_url = kWebAppUrl;
    manifest->id = GenerateManifestIdFromStartUrlOnly(kWebAppUrl);
    manifest->display = blink::mojom::DisplayMode::kStandalone;
    return manifest;
  }

  void SetUp() override {
    WebAppTest::SetUp();
    auto shortcut_manager = std::make_unique<TestShortcutManager>(profile());
    shortcut_manager_ = shortcut_manager.get();
    FakeWebAppProvider::Get(profile())
        ->GetOsIntegrationManager()
        .AsTestOsIntegrationManager()
        ->SetShortcutManager(std::move(shortcut_manager));

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    shortcut_manager_ = nullptr;
    WebAppTest::TearDown();
  }

  apps::IconInfo GetMockIconInfo(const SquareSizePx& sq_size) {
    apps::IconInfo info;
    const GURL url("https://example.com/");
    info.url = url.Resolve("icon_any" + base::NumberToString(sq_size) + ".png");
    info.square_size_px = sq_size;
    return info;
  }

  IconsMap CreateAndLoadIconMapFromSizes(
      const std::vector<SquareSizePx>& sizes_px,
      const std::vector<SkColor>& colors,
      blink::mojom::Manifest* manifest) {
    DCHECK_EQ(colors.size(), sizes_px.size());
    IconsMap icons_map;
    const GURL url = GURL("https://example.com/");

    for (size_t i = 0; i < sizes_px.size(); i++) {
      GURL icon_url =
          url.Resolve("icon_any" + base::NumberToString(sizes_px[i]) + ".png");
      manifest->icons.push_back(
          CreateSquareImageResource(icon_url, sizes_px[i], {IconPurpose::ANY}));
      icons_map[icon_url] = {CreateSquareIcon(sizes_px[i], colors[i])};
    }
    return icons_map;
  }

  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }

  void LoadIconsFromDB(const webapps::AppId& app_id,
                       const std::vector<SquareSizePx>& sizes_px) {
    BitmapData icon_bitmaps;
    base::test::TestFuture<BitmapData> future;
    WebAppIconManager& icon_manager = provider()->icon_manager();

    // We can use this to test if icons of a specific size do not exist in the
    // DB. This is to ensure we do not trigger the same condition as a DCHECK
    // inside WebAppIconManager when calling ReadIcons().
    if (!icon_manager.HasIcons(app_id, IconPurpose::ANY, sizes_px)) {
      app_to_icons_data_[app_id] = icon_bitmaps;
      return;
    }

    icon_manager.ReadIcons(app_id, IconPurpose::ANY, sizes_px,
                           future.GetCallback());
    app_to_icons_data_[app_id] = future.Take();
  }

  std::vector<SquareSizePx> GetIconSizesForApp(const webapps::AppId& app_id) {
    DCHECK(base::Contains(app_to_icons_data_, app_id));
    return base::test::ToVector(
        app_to_icons_data_[app_id],
        [](const auto& icon_data) { return icon_data.first; });
  }

  std::vector<SkColor> GetIconColorsForApp(const webapps::AppId& app_id) {
    DCHECK(base::Contains(app_to_icons_data_, app_id));
    return base::test::ToVector(
        app_to_icons_data_[app_id],
        [](const auto& icon_data) { return icon_data.second.getColor(0, 0); });
  }

  void SetPageState(ExternalInstallOptions options,
                    const PageStateOptions& mock_options = {}) {
    FakeWebContentsManager::FakePageState& state =
        fake_web_contents_manager().GetOrCreatePageState(options.install_url);
    state.opt_manifest = blink::mojom::Manifest::New();
    state.opt_manifest->start_url = options.install_url;
    state.opt_manifest->id =
        GenerateManifestIdFromStartUrlOnly(options.install_url);
    state.opt_manifest->name = u"Manifest Name";

    state.return_null_info = mock_options.empty_web_app_info;

    state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;
    state.url_load_result = mock_options.url_load_result;
  }

  bool IsPlaceholderAppUrl(const GURL& url) {
    return registrar()
        .LookupPlaceholderAppId(url, WebAppManagement::kPolicy)
        .has_value();
  }

  bool IsPlaceholderAppId(const webapps::AppId& app_id) {
    return registrar().IsPlaceholderApp(app_id, WebAppManagement::kPolicy);
  }

  WebAppRegistrar& registrar() { return fake_provider().registrar_unsafe(); }

  FakeOsIntegrationManager* os_integration_manager() {
    return WebAppProvider::GetForTest(profile())
        ->os_integration_manager()
        .AsTestOsIntegrationManager();
  }

  TestShortcutManager* shortcut_manager() { return shortcut_manager_; }

  FakeWebContentsManager& fake_web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        fake_provider().web_contents_manager());
  }

  FakeWebAppUiManager& fake_ui_manager() {
    return static_cast<FakeWebAppUiManager&>(fake_provider().ui_manager());
  }

  TestFileUtils& file_utils() {
    return *fake_provider().file_utils()->AsTestFileUtils();
  }

 private:
  base::flat_map<webapps::AppId, BitmapData> app_to_icons_data_;
  raw_ptr<TestShortcutManager> shortcut_manager_ = nullptr;
};

TEST_F(ExternalAppResolutionCommandTest, SuccessInternalDefault) {
  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kBrowser,
      ExternalInstallSource::kInternalDefault);

  SetPageState(install_options);

  auto result = InstallAndWait(install_options);
  EXPECT_EQ(result.code, webapps::InstallResultCode::kSuccessNewInstall);
  ASSERT_TRUE(result.app_id.has_value());
  EXPECT_TRUE(registrar().IsLocallyInstalled(*result.app_id));
  EXPECT_FALSE(IsPlaceholderAppUrl(kWebAppUrl));
  absl::optional<webapps::AppId> id =
      registrar().LookupExternalAppId(kWebAppUrl);
  ASSERT_TRUE(id.has_value());
  EXPECT_EQ(*result.app_id, *id);
  EXPECT_EQ(0, fake_ui_manager().num_reparent_tab_calls());
  EXPECT_TRUE(registrar().GetAppById(id.value()));
  EXPECT_EQ(registrar().GetAppUserDisplayMode(id.value()),
            mojom::UserDisplayMode::kBrowser);
  EXPECT_EQ(registrar().GetLatestAppInstallSource(id.value()),
            webapps::WebappInstallSource::INTERNAL_DEFAULT);
}

TEST_F(ExternalAppResolutionCommandTest, SuccessAppFromPolicy) {
  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kBrowser,
      ExternalInstallSource::kExternalDefault);

  SetPageState(install_options);

  auto result = InstallAndWait(install_options);
  EXPECT_EQ(result.code, webapps::InstallResultCode::kSuccessNewInstall);
  ASSERT_TRUE(result.app_id.has_value());
  EXPECT_TRUE(registrar().IsLocallyInstalled(*result.app_id));
  EXPECT_FALSE(IsPlaceholderAppUrl(kWebAppUrl));
  absl::optional<webapps::AppId> id =
      registrar().LookupExternalAppId(kWebAppUrl);
  ASSERT_TRUE(id.has_value());
  EXPECT_EQ(*result.app_id, *id);
  EXPECT_EQ(0, fake_ui_manager().num_reparent_tab_calls());
  EXPECT_TRUE(registrar().GetAppById(id.value()));
  EXPECT_EQ(registrar().GetAppUserDisplayMode(id.value()),
            mojom::UserDisplayMode::kBrowser);
  EXPECT_EQ(registrar().GetLatestAppInstallSource(id.value()),
            webapps::WebappInstallSource::EXTERNAL_DEFAULT);
}

TEST_F(ExternalAppResolutionCommandTest, InstallFails) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kBrowser,
      ExternalInstallSource::kExternalDefault);

  SetPageState(install_options, {.empty_web_app_info = true});

  auto result = InstallAndWait(
      install_options, fake_web_contents_manager().CreateDataRetriever());

  absl::optional<webapps::AppId> id =
      registrar().LookupExternalAppId(kWebAppUrl);

  EXPECT_EQ(webapps::InstallResultCode::kGetWebAppInstallInfoFailed,
            result.code);
  EXPECT_FALSE(result.app_id.has_value());
  EXPECT_FALSE(id.has_value());
}

TEST_F(ExternalAppResolutionCommandTest, SuccessInstallPlaceholder) {
  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalPolicy);
  install_options.install_placeholder = true;

  SetPageState(
      install_options,
      {.url_load_result = WebAppUrlLoaderResult::kRedirectedUrlLoaded});

  auto result = InstallAndWait(install_options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_TRUE(IsPlaceholderAppUrl(kWebAppUrl));

  ASSERT_TRUE(result.app_id.has_value());

  webapps::AppId app_id = result.app_id.value();
  EXPECT_EQ(registrar().GetLatestAppInstallSource(app_id),
            webapps::WebappInstallSource::EXTERNAL_POLICY);

  EXPECT_EQ(registrar().GetAppShortName(app_id), kWebAppUrl.spec());
  EXPECT_EQ(registrar().GetAppStartUrl(app_id), kWebAppUrl);
  EXPECT_EQ(registrar().GetAppUserDisplayMode(app_id),
            mojom::UserDisplayMode::kStandalone);
  EXPECT_TRUE(registrar().GetAppIconInfos(app_id).empty());
  EXPECT_TRUE(registrar().GetAppDownloadedIconSizesAny(app_id).empty());
  EXPECT_FALSE(fake_provider().icon_manager().HasSmallestIcon(
      app_id, {IconPurpose::ANY}, /*min_size=*/0));
}

TEST_F(ExternalAppResolutionCommandTest, InstallPlaceholderTwice) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options(kWebAppUrl,
                                 mojom::UserDisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  webapps::AppId placeholder_app_id;

  // Install a placeholder app.
  {
    SetPageState(options, {.url_load_result =
                               WebAppUrlLoaderResult::kRedirectedUrlLoaded});

    auto result = InstallAndWait(options);

    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    ASSERT_TRUE(result.app_id.has_value());
    placeholder_app_id = result.app_id.value();

    const WebApp* web_app = registrar().GetAppById(placeholder_app_id);
    ASSERT_TRUE(web_app);
    EXPECT_TRUE(web_app->HasOnlySource(WebAppManagement::Type::kPolicy));
    EXPECT_TRUE(IsPlaceholderAppId(placeholder_app_id));
  }

  // Try to install it again.
  SetPageState(options, {.url_load_result =
                             WebAppUrlLoaderResult::kRedirectedUrlLoaded});
  auto data_retriever = std::make_unique<FakeDataRetriever>();
  data_retriever->BuildDefaultDataToRetrieve(kWebAppUrl, kWebAppScope);

  auto result = InstallAndWait(options, std::move(data_retriever));

  EXPECT_EQ(webapps::InstallResultCode::kSuccessAlreadyInstalled, result.code);
  EXPECT_EQ(placeholder_app_id, result.app_id.value());

  // It should still be a placeholder.
  const WebApp* web_app = registrar().GetAppById(placeholder_app_id);
  ASSERT_TRUE(web_app);
  EXPECT_TRUE(web_app->HasOnlySource(WebAppManagement::Type::kPolicy));
  EXPECT_TRUE(IsPlaceholderAppId(placeholder_app_id));
}

TEST_F(ExternalAppResolutionCommandTest, ReinstallPlaceholderSucceeds) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options(kWebAppUrl,
                                 mojom::UserDisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  webapps::AppId placeholder_app_id;

  // Install a placeholder app.
  {
    SetPageState(options, {.url_load_result =
                               WebAppUrlLoaderResult::kRedirectedUrlLoaded});

    auto result = InstallAndWait(options);

    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    ASSERT_TRUE(result.app_id.has_value());
    placeholder_app_id = result.app_id.value();

    const WebApp* web_app = registrar().GetAppById(placeholder_app_id);
    ASSERT_TRUE(web_app);
    EXPECT_TRUE(web_app->HasOnlySource(WebAppManagement::Type::kPolicy));
    EXPECT_TRUE(IsPlaceholderAppId(placeholder_app_id));
  }

  // Replace the placeholder with a real app.
  SetPageState(options);
  auto data_retriever = std::make_unique<FakeDataRetriever>();
  data_retriever->BuildDefaultDataToRetrieve(kWebAppUrl, kWebAppScope);

  auto result = InstallAndWait(options, std::move(data_retriever));

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  ASSERT_TRUE(result.app_id.has_value());
  EXPECT_EQ(result.app_id.value(), placeholder_app_id);

  const WebApp* web_app = registrar().GetAppById(placeholder_app_id);
  ASSERT_TRUE(web_app);
  EXPECT_TRUE(web_app->HasOnlySource(WebAppManagement::Type::kPolicy));

  EXPECT_FALSE(IsPlaceholderAppUrl(kWebAppUrl));
  EXPECT_FALSE(IsPlaceholderAppId(placeholder_app_id));
}

// With go/external_app_refactoring the placeholder is updated in-place and not
// uninstalled anymore.
TEST_F(ExternalAppResolutionCommandTest,
       ReinstallPlaceholderMigrationSucceedsWithFailingFileDeletion) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options(kWebAppUrl,
                                 mojom::UserDisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  webapps::AppId placeholder_app_id;

  // Install a placeholder app.
  {
    webapps::AppId expected_app_id =
        GenerateAppId(/*manifest_id_path=*/absl::nullopt, kWebAppUrl);

    SetPageState(options, {.url_load_result =
                               WebAppUrlLoaderResult::kRedirectedUrlLoaded});

    auto result = InstallAndWait(options);

    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    ASSERT_TRUE(result.app_id.has_value());
    placeholder_app_id = result.app_id.value();
    EXPECT_EQ(expected_app_id, placeholder_app_id);

    EXPECT_TRUE(registrar()
                    .GetAppById(placeholder_app_id)
                    ->HasOnlySource(WebAppManagement::Type::kPolicy));
    EXPECT_TRUE(IsPlaceholderAppId(placeholder_app_id));
    EXPECT_TRUE(registrar().IsInstalled(placeholder_app_id));
  }

  // Replace the placeholder with a real app.
  SetPageState(options);
  auto data_retriever = std::make_unique<FakeDataRetriever>();
  data_retriever->BuildDefaultDataToRetrieve(kWebAppUrl, kWebAppScope);

  // Simulate disk failure to uninstall the placeholder.
  file_utils().SetNextDeleteFileRecursivelyResult(false);

  auto result = InstallAndWait(options, std::move(data_retriever));

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  ASSERT_TRUE(result.app_id.has_value());
  EXPECT_EQ(result.app_id.value(), placeholder_app_id);

  EXPECT_TRUE(registrar()
                  .GetAppById(placeholder_app_id)
                  ->HasOnlySource(WebAppManagement::Type::kPolicy));

  EXPECT_FALSE(IsPlaceholderAppUrl(kWebAppUrl));
  EXPECT_FALSE(IsPlaceholderAppId(placeholder_app_id));
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(ExternalAppResolutionCommandTest, InstallPlaceholderCustomName) {
  const GURL kWebAppUrl("https://foo.example");
  const std::string kCustomName("Custom äpp näme");
  ExternalInstallOptions options(kWebAppUrl,
                                 mojom::UserDisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  options.override_name = kCustomName;
  SetPageState(options, {.url_load_result =
                             WebAppUrlLoaderResult::kRedirectedUrlLoaded});

  auto result = InstallAndWait(options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  ASSERT_TRUE(result.app_id.has_value());

  EXPECT_EQ(registrar().GetAppShortName(result.app_id.value()), kCustomName);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(ExternalAppResolutionCommandTest, UninstallAndReplace) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options = {kWebAppUrl, absl::nullopt,
                                    ExternalInstallSource::kInternalDefault};
  webapps::AppId app_id;
  {
    // Migrate app1 and app2.
    options.uninstall_and_replace = {"app1", "app2"};

    SetPageState(options);

    auto result = InstallAndWait(options);

    ASSERT_TRUE(result.app_id.has_value());
    app_id = result.app_id.value();

    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    EXPECT_EQ(result.app_id, *registrar().LookupExternalAppId(kWebAppUrl));
  }
  {
    // Migration should run on every install of the app.
    options.uninstall_and_replace = {"app3"};

    SetPageState(options);

    auto result = InstallAndWait(options);

    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    ASSERT_TRUE(result.app_id.has_value());
    EXPECT_EQ(app_id, result.app_id.value());
  }
}

TEST_F(ExternalAppResolutionCommandTest, InstallURLLoadFailed) {
  const GURL kWebAppUrl("https://foo.example");
  struct ResultPair {
    WebAppUrlLoader::Result loader_result;
    webapps::InstallResultCode install_result;
  } result_pairs[] = {{WebAppUrlLoader::Result::kRedirectedUrlLoaded,
                       webapps::InstallResultCode::kInstallURLRedirected},
                      {WebAppUrlLoader::Result::kFailedUnknownReason,
                       webapps::InstallResultCode::kInstallURLLoadFailed},
                      {WebAppUrlLoader::Result::kFailedPageTookTooLong,
                       webapps::InstallResultCode::kInstallURLLoadTimeOut}};

  for (const auto& result_pair : result_pairs) {
    ExternalInstallOptions install_options(
        kWebAppUrl, mojom::UserDisplayMode::kStandalone,
        ExternalInstallSource::kInternalDefault);
    fake_web_contents_manager()
        .GetOrCreatePageState(install_options.install_url)
        .url_load_result = result_pair.loader_result;

    auto result = InstallAndWait(install_options);

    EXPECT_EQ(result.code, result_pair.install_result);
  }
}

TEST_F(ExternalAppResolutionCommandTest, InstallWithWebAppInfoSucceeds) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options(kWebAppUrl,
                                 mojom::UserDisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalDefault);
  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindLambdaForTesting([&kWebAppUrl]() {
    auto info = std::make_unique<WebAppInstallInfo>();
    info->start_url = kWebAppUrl;
    info->scope = kWebAppUrl.GetWithoutFilename();
    info->title = u"Foo Web App";
    return info;
  });

  auto result = InstallAndWait(options);

  absl::optional<webapps::AppId> id =
      registrar().LookupExternalAppId(kWebAppUrl);
  EXPECT_EQ(webapps::InstallResultCode::kSuccessOfflineOnlyInstall,
            result.code);
  ASSERT_TRUE(result.app_id.has_value());
  webapps::AppId app_id = result.app_id.value();

  EXPECT_FALSE(IsPlaceholderAppUrl(kWebAppUrl));

  EXPECT_EQ(app_id, id.value_or("absent"));

  EXPECT_EQ(fake_ui_manager().num_reparent_tab_calls(), 0);

  EXPECT_EQ(registrar().GetAppUserDisplayMode(app_id),
            mojom::UserDisplayMode::kStandalone);
  EXPECT_EQ(registrar().GetLatestAppInstallSource(app_id),
            webapps::WebappInstallSource::EXTERNAL_DEFAULT);
}

TEST_F(ExternalAppResolutionCommandTest, InstallWithWebAppInfoFails) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options(kWebAppUrl,
                                 mojom::UserDisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalDefault);
  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindLambdaForTesting([&kWebAppUrl]() {
    auto info = std::make_unique<WebAppInstallInfo>();
    info->start_url = kWebAppUrl;
    info->scope = kWebAppUrl.GetWithoutFilename();
    info->title = u"Foo Web App";
    return info;
  });

  // Induce an error: Simulate "Disk Full" for writing icon files.
  file_utils().SetRemainingDiskSpaceSize(0);

  auto result = InstallAndWait(options);

  absl::optional<webapps::AppId> id =
      registrar().LookupExternalAppId(kWebAppUrl);

  EXPECT_EQ(webapps::InstallResultCode::kWriteDataFailed, result.code);
  EXPECT_FALSE(result.app_id.has_value());

  EXPECT_FALSE(id.has_value());
}

TEST_F(ExternalAppResolutionCommandTest, SucessInstallForcedContainerWindow) {
  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kInternalDefault);

  SetPageState(install_options);

  auto result = InstallAndWait(install_options);
  EXPECT_EQ(result.code, webapps::InstallResultCode::kSuccessNewInstall);
  ASSERT_TRUE(result.app_id.has_value());
  EXPECT_TRUE(registrar().IsLocallyInstalled(*result.app_id));
  EXPECT_FALSE(IsPlaceholderAppUrl(kWebAppUrl));
  absl::optional<webapps::AppId> id =
      registrar().LookupExternalAppId(kWebAppUrl);
  ASSERT_TRUE(id.has_value());
  EXPECT_EQ(*result.app_id, *id);
  EXPECT_EQ(0, fake_ui_manager().num_reparent_tab_calls());
  EXPECT_TRUE(registrar().GetAppById(id.value()));
  EXPECT_EQ(registrar().GetAppUserDisplayMode(id.value()),
            mojom::UserDisplayMode::kStandalone);
  EXPECT_EQ(registrar().GetLatestAppInstallSource(id.value()),
            webapps::WebappInstallSource::INTERNAL_DEFAULT);
}

TEST_F(ExternalAppResolutionCommandTest, GetWebAppInstallInfoFailed) {
  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalDefault);

  SetPageState(
      {kWebAppUrl, absl::nullopt, ExternalInstallSource::kInternalDefault},
      {.empty_web_app_info = true});

  auto result = InstallAndWait(install_options);
  EXPECT_EQ(result.code,
            webapps::InstallResultCode::kGetWebAppInstallInfoFailed);
  ASSERT_FALSE(result.app_id.has_value());
  EXPECT_FALSE(registrar().IsLocallyInstalled(kWebAppId));
}

TEST_F(ExternalAppResolutionCommandTest,
       InstallWebAppWithParams_DisplayModeFromWebAppInstallInfo) {
  {
    GURL url("https://example1.com/");
    auto data_retriever = std::make_unique<FakeDataRetriever>();
    data_retriever->BuildDefaultDataToRetrieve(url, url);

    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->user_display_mode = mojom::UserDisplayMode::kBrowser;
    data_retriever->SetRendererWebAppInstallInfo(std::move(web_app_info));
    SetPageState({url, absl::nullopt, ExternalInstallSource::kInternalDefault});

    ExternalInstallOptions install_options(
        url, /*user_display_mode=*/absl::nullopt,
        ExternalInstallSource::kExternalDefault);
    auto result = InstallAndWait(install_options, std::move(data_retriever));

    ASSERT_TRUE(result.app_id.has_value());
    EXPECT_EQ(mojom::UserDisplayMode::kBrowser, provider()
                                                    ->registrar_unsafe()
                                                    .GetAppById(*result.app_id)
                                                    ->user_display_mode());
  }
  {
    GURL url("https://example2.com/");
    auto data_retriever = std::make_unique<FakeDataRetriever>();
    data_retriever->BuildDefaultDataToRetrieve(url, url);

    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
    data_retriever->SetRendererWebAppInstallInfo(std::move(web_app_info));
    SetPageState({url, absl::nullopt, ExternalInstallSource::kInternalDefault});

    ExternalInstallOptions install_options(
        url, /*user_display_mode=*/absl::nullopt,
        ExternalInstallSource::kExternalDefault);
    auto result = InstallAndWait(install_options, std::move(data_retriever));

    ASSERT_TRUE(result.app_id.has_value());
    EXPECT_EQ(mojom::UserDisplayMode::kStandalone,
              provider()
                  ->registrar_unsafe()
                  .GetAppById(*result.app_id)
                  ->user_display_mode());
  }
}

TEST_F(ExternalAppResolutionCommandTest,
       InstallWebAppWithParams_DisplayModeOverrideByExternalInstallOptions) {
  {
    GURL url("https://example3.com/");
    auto data_retriever = std::make_unique<FakeDataRetriever>();
    data_retriever->BuildDefaultDataToRetrieve(url, url);

    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
    data_retriever->SetRendererWebAppInstallInfo(std::move(web_app_info));
    SetPageState({url, absl::nullopt, ExternalInstallSource::kInternalDefault});

    ExternalInstallOptions install_options(
        url, mojom::UserDisplayMode::kBrowser,
        ExternalInstallSource::kExternalDefault);
    auto result = InstallAndWait(install_options, std::move(data_retriever));

    ASSERT_TRUE(result.app_id.has_value());
    EXPECT_EQ(mojom::UserDisplayMode::kBrowser, provider()
                                                    ->registrar_unsafe()
                                                    .GetAppById(*result.app_id)
                                                    ->user_display_mode());
  }
  {
    GURL url("https://example4.com/");
    auto data_retriever = std::make_unique<FakeDataRetriever>();
    data_retriever->BuildDefaultDataToRetrieve(url, url);
    SetPageState({url, absl::nullopt, ExternalInstallSource::kInternalDefault});

    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->user_display_mode = mojom::UserDisplayMode::kBrowser;
    data_retriever->SetRendererWebAppInstallInfo(std::move(web_app_info));

    ExternalInstallOptions install_options(
        url, mojom::UserDisplayMode::kStandalone,
        ExternalInstallSource::kExternalDefault);
    auto result = InstallAndWait(install_options, std::move(data_retriever));

    ASSERT_TRUE(result.app_id.has_value());
    EXPECT_EQ(mojom::UserDisplayMode::kStandalone,
              provider()
                  ->registrar_unsafe()
                  .GetAppById(*result.app_id)
                  ->user_display_mode());
  }
}

TEST_F(ExternalAppResolutionCommandTest, UpgradeLock) {
  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalDefault);

  auto data_retriever = std::make_unique<FakeDataRetriever>();
  data_retriever->BuildDefaultDataToRetrieve(kWebAppUrl, kWebAppScope);
  SetPageState(
      {kWebAppUrl, absl::nullopt, ExternalInstallSource::kInternalDefault});

  base::flat_set<webapps::AppId> app_ids{
      GenerateAppId(/*manifest_id_path=*/absl::nullopt, kWebAppUrl)};

  bool callback_command_run = false;
  auto callback_command = std::make_unique<CallbackCommand<AppLock>>(
      "", std::make_unique<AppLockDescription>(app_ids),
      base::BindLambdaForTesting(
          [&](AppLock&) { callback_command_run = true; }));

  bool callback_command_2_run = false;
  base::RunLoop callback_runloop;
  auto callback_command_2 = std::make_unique<CallbackCommand<AppLock>>(
      "", std::make_unique<AppLockDescription>(app_ids),
      base::BindLambdaForTesting([&](AppLock&) {
        callback_command_2_run = true;
        callback_runloop.Quit();
      }));

  base::RunLoop run_loop;
  ExternallyManagedAppManager::InstallResult result;
  webapps::AppId placeholder_app_id =
      GenerateAppId(absl::nullopt, install_options.install_url);
  const bool is_placeholder_installed = registrar().IsPlaceholderApp(
      placeholder_app_id,
      ConvertExternalInstallSourceToSource(install_options.install_source));
  auto command = std::make_unique<ExternalAppResolutionCommand>(
      *profile(), install_options,
      is_placeholder_installed
          ? absl::optional<webapps::AppId>(placeholder_app_id)
          : absl::nullopt,
      base::BindLambdaForTesting(
          [&](ExternallyManagedAppManager::InstallResult install_result) {
            result = std::move(install_result);
            run_loop.Quit();
          }));

  // Schedules another callback command that acquires the same app lock after
  // current command upgrades to app lock.
  command->SetOnLockUpgradedCallbackForTesting(
      base::BindLambdaForTesting([&]() {
        provider()->command_manager().ScheduleCommand(
            std::move(callback_command_2));
      }));

  provider()->command_manager().ScheduleCommand(std::move(command));
  // Immediately schedule a callback command, this will request the app lock
  // before the ExternallyManagedInstallCommand.
  provider()->command_manager().ScheduleCommand(std::move(callback_command));

  run_loop.Run();

  EXPECT_EQ(result.code, webapps::InstallResultCode::kSuccessNewInstall);
  ASSERT_TRUE(result.app_id.has_value());
  EXPECT_TRUE(registrar().IsLocallyInstalled(*result.app_id));

  EXPECT_TRUE(callback_command_run);

  EXPECT_FALSE(callback_command_2_run);

  callback_runloop.Run();
  EXPECT_TRUE(callback_command_2_run);
}

TEST_F(ExternalAppResolutionCommandTest,
       IconDownloadSuccessOverwriteNoIconsBefore) {
  // First install the app normally.
  {
    auto data_retriever = std::make_unique<FakeDataRetriever>();
    data_retriever->BuildDefaultDataToRetrieve(kWebAppUrl, kWebAppScope);

    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
    data_retriever->SetRendererWebAppInstallInfo(std::move(web_app_info));
    SetPageState(
        {kWebAppUrl, absl::nullopt, ExternalInstallSource::kInternalDefault});

    ExternalInstallOptions install_options(
        kWebAppUrl, mojom::UserDisplayMode::kStandalone,
        ExternalInstallSource::kExternalDefault);
    auto result = InstallAndWait(install_options, std::move(data_retriever));

    EXPECT_EQ(result.code, webapps::InstallResultCode::kSuccessNewInstall);
    ASSERT_TRUE(result.app_id.has_value());
    EXPECT_EQ(0u, registrar().GetAppIconInfos(*result.app_id).size());
  }
  // Now install the same app with a manifest containing valid icons and
  // verify successful icon downloads and writes to DB.
  {
    std::vector<SquareSizePx> new_sizes{icon_size::k64, icon_size::k512};
    std::vector<SkColor> icon_colors{SK_ColorGREEN, SK_ColorRED};
    auto manifest = CreateValidManifest();
    IconsMap icons_map =
        CreateAndLoadIconMapFromSizes(new_sizes, icon_colors, manifest.get());
    DownloadedIconsHttpResults http_results;
    for (const auto& url_and_bitmap : icons_map) {
      http_results[url_and_bitmap.first] = net::HttpStatusCode::HTTP_OK;
    }

    // Set up data retriever and load everything.
    auto new_data_retriever = std::make_unique<FakeDataRetriever>();
    new_data_retriever->SetIconsDownloadedResult(
        IconsDownloadedResult::kCompleted);
    new_data_retriever->SetDownloadedIconsHttpResults(std::move(http_results));
    new_data_retriever->SetIcons(std::move(icons_map));
    new_data_retriever->SetManifest(
        std::move(manifest), webapps::InstallableStatusCode::NO_ERROR_DETECTED);
    new_data_retriever->SetEmptyRendererWebAppInstallInfo();
    SetPageState(
        {kWebAppUrl, absl::nullopt, ExternalInstallSource::kInternalDefault});

    ExternalInstallOptions new_install_options(
        kWebAppUrl, mojom::UserDisplayMode::kStandalone,
        ExternalInstallSource::kExternalPolicy);
    auto result =
        InstallAndWait(new_install_options, std::move(new_data_retriever));
    ASSERT_TRUE(result.app_id.has_value());
    const webapps::AppId& installed_app_id = *result.app_id;

    EXPECT_EQ(result.code, webapps::InstallResultCode::kSuccessNewInstall);

    // Verify icon information.
    const std::vector<apps::IconInfo> icon_info =
        registrar().GetAppIconInfos(installed_app_id);
    EXPECT_EQ(new_sizes.size(), icon_info.size());
    EXPECT_EQ(GetMockIconInfo(icon_size::k64), icon_info[0]);
    EXPECT_EQ(GetMockIconInfo(icon_size::k512), icon_info[1]);
    LoadIconsFromDB(installed_app_id, new_sizes);
    EXPECT_EQ(GetIconSizesForApp(installed_app_id), new_sizes);
    EXPECT_EQ(GetIconColorsForApp(installed_app_id), icon_colors);
  }
}

TEST_F(ExternalAppResolutionCommandTest, IconDownloadSuccessOverwriteOldIcons) {
  // First install the app normally, having 1 icon only.
  {
    std::vector<SquareSizePx> old_sizes{icon_size::k256};
    std::vector<SkColor> old_colors{SK_ColorBLUE};
    auto manifest = CreateValidManifest();
    IconsMap icons_map =
        CreateAndLoadIconMapFromSizes(old_sizes, old_colors, manifest.get());
    DownloadedIconsHttpResults http_results;
    for (const auto& url_and_bitmap : icons_map) {
      http_results[url_and_bitmap.first] = net::HttpStatusCode::HTTP_OK;
    }

    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;

    auto data_retriever = std::make_unique<FakeDataRetriever>();
    data_retriever->BuildDefaultDataToRetrieve(kWebAppUrl, kWebAppScope);
    data_retriever->SetIconsDownloadedResult(IconsDownloadedResult::kCompleted);
    data_retriever->SetDownloadedIconsHttpResults(std::move(http_results));
    data_retriever->SetIcons(std::move(icons_map));
    data_retriever->SetManifest(
        std::move(manifest), webapps::InstallableStatusCode::NO_ERROR_DETECTED);
    data_retriever->SetRendererWebAppInstallInfo(std::move(web_app_info));
    SetPageState(
        {kWebAppUrl, absl::nullopt, ExternalInstallSource::kInternalDefault});

    ExternalInstallOptions install_options(
        kWebAppUrl, mojom::UserDisplayMode::kStandalone,
        ExternalInstallSource::kExternalDefault);
    auto result = InstallAndWait(install_options, std::move(data_retriever));
    ASSERT_TRUE(result.app_id.has_value());
    const webapps::AppId& installed_app_id = *result.app_id;

    EXPECT_EQ(result.code, webapps::InstallResultCode::kSuccessNewInstall);
    const std::vector<apps::IconInfo>& icons_info_pre_reinstall =
        registrar().GetAppIconInfos(installed_app_id);
    EXPECT_EQ(old_sizes.size(), icons_info_pre_reinstall.size());
    EXPECT_EQ(GetMockIconInfo(icon_size::k256), icons_info_pre_reinstall[0]);
    LoadIconsFromDB(installed_app_id, old_sizes);
    EXPECT_EQ(GetIconSizesForApp(installed_app_id), old_sizes);
    EXPECT_EQ(GetIconColorsForApp(installed_app_id), old_colors);
  }
  // Now install the same app with a manifest containing different icons from
  // before, and verify that the icons are updated in the DB.
  {
    std::vector<SquareSizePx> new_sizes{icon_size::k64, icon_size::k512};
    std::vector<SkColor> new_colors{SK_ColorYELLOW, SK_ColorRED};
    auto new_manifest = CreateValidManifest();
    IconsMap new_icons_map = CreateAndLoadIconMapFromSizes(
        new_sizes, new_colors, new_manifest.get());
    DownloadedIconsHttpResults new_http_results;
    for (const auto& url_and_bitmap : new_icons_map) {
      new_http_results[url_and_bitmap.first] = net::HttpStatusCode::HTTP_OK;
    }

    // Set up data retriever and load everything.
    auto new_data_retriever = std::make_unique<FakeDataRetriever>();
    new_data_retriever->SetIconsDownloadedResult(
        IconsDownloadedResult::kCompleted);
    new_data_retriever->SetDownloadedIconsHttpResults(
        std::move(new_http_results));
    new_data_retriever->SetIcons(std::move(new_icons_map));
    new_data_retriever->SetManifest(
        std::move(new_manifest),
        webapps::InstallableStatusCode::NO_ERROR_DETECTED);
    new_data_retriever->SetEmptyRendererWebAppInstallInfo();
    SetPageState(
        {kWebAppUrl, absl::nullopt, ExternalInstallSource::kInternalDefault});

    ExternalInstallOptions new_install_options(
        kWebAppUrl, mojom::UserDisplayMode::kStandalone,
        ExternalInstallSource::kExternalPolicy);
    auto updated_result =
        InstallAndWait(new_install_options, std::move(new_data_retriever));
    ASSERT_TRUE(updated_result.app_id.has_value());
    const webapps::AppId& updated_app_id = *updated_result.app_id;

    EXPECT_EQ(updated_result.code,
              webapps::InstallResultCode::kSuccessNewInstall);

    // Verify icon information.
    const std::vector<apps::IconInfo> new_icon_info =
        registrar().GetAppIconInfos(updated_app_id);
    EXPECT_EQ(new_sizes.size(), new_icon_info.size());
    EXPECT_EQ(GetMockIconInfo(icon_size::k64), new_icon_info[0]);
    EXPECT_EQ(GetMockIconInfo(icon_size::k512), new_icon_info[1]);
    LoadIconsFromDB(updated_app_id, new_sizes);
    EXPECT_EQ(GetIconSizesForApp(updated_app_id), new_sizes);
    EXPECT_EQ(GetIconColorsForApp(updated_app_id), new_colors);
  }
}

TEST_F(ExternalAppResolutionCommandTest,
       IconDownloadFailDoesNotOverwriteOldIcons) {
  // First install the app normally, having 1 icon only.
  std::vector<SquareSizePx> old_sizes{icon_size::k64};
  std::vector<SkColor> old_colors{SK_ColorRED};
  {
    auto manifest = CreateValidManifest();
    IconsMap icons_map =
        CreateAndLoadIconMapFromSizes(old_sizes, old_colors, manifest.get());
    DownloadedIconsHttpResults http_results;
    for (const auto& url_and_bitmap : icons_map) {
      http_results[url_and_bitmap.first] = net::HttpStatusCode::HTTP_OK;
    }

    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;

    auto data_retriever = std::make_unique<FakeDataRetriever>();
    data_retriever->BuildDefaultDataToRetrieve(kWebAppUrl, kWebAppScope);
    data_retriever->SetIconsDownloadedResult(IconsDownloadedResult::kCompleted);
    data_retriever->SetDownloadedIconsHttpResults(std::move(http_results));
    data_retriever->SetIcons(std::move(icons_map));
    data_retriever->SetManifest(
        std::move(manifest), webapps::InstallableStatusCode::NO_ERROR_DETECTED);
    data_retriever->SetRendererWebAppInstallInfo(std::move(web_app_info));
    SetPageState(
        {kWebAppUrl, absl::nullopt, ExternalInstallSource::kInternalDefault});

    ExternalInstallOptions install_options(
        kWebAppUrl, mojom::UserDisplayMode::kStandalone,
        ExternalInstallSource::kExternalDefault);
    auto result = InstallAndWait(install_options, std::move(data_retriever));
    ASSERT_TRUE(result.app_id.has_value());
    const webapps::AppId& installed_app_id = *result.app_id;

    EXPECT_EQ(result.code, webapps::InstallResultCode::kSuccessNewInstall);
    const std::vector<apps::IconInfo>& icons_info_pre_reinstall =
        registrar().GetAppIconInfos(installed_app_id);
    EXPECT_EQ(1u, icons_info_pre_reinstall.size());
    EXPECT_EQ(GetMockIconInfo(icon_size::k64), icons_info_pre_reinstall[0]);
    LoadIconsFromDB(installed_app_id, old_sizes);
    EXPECT_EQ(GetIconSizesForApp(installed_app_id), old_sizes);
    EXPECT_EQ(GetIconColorsForApp(installed_app_id), old_colors);
  }
  // Now install the same app with a manifest containing multiple icons, and
  // fail the icon downloading. This should prevent icons from getting
  // overwritten in the DB.
  {
    std::vector<SquareSizePx> new_sizes{icon_size::k256, icon_size::k512};
    std::vector<SkColor> new_colors{SK_ColorGREEN, SK_ColorBLUE};
    auto new_manifest = CreateValidManifest();
    IconsMap new_icons_map = CreateAndLoadIconMapFromSizes(
        new_sizes, new_colors, new_manifest.get());
    DownloadedIconsHttpResults new_http_results;
    for (const auto& url_and_bitmap : new_icons_map) {
      new_http_results[url_and_bitmap.first] = net::HttpStatusCode::HTTP_OK;
    }

    // Set up data retriever and load everything.
    auto new_data_retriever = std::make_unique<FakeDataRetriever>();
    new_data_retriever->SetIconsDownloadedResult(
        IconsDownloadedResult::kAbortedDueToFailure);
    new_data_retriever->SetDownloadedIconsHttpResults(
        std::move(new_http_results));
    new_data_retriever->SetIcons(std::move(new_icons_map));
    new_data_retriever->SetManifest(
        std::move(new_manifest),
        webapps::InstallableStatusCode::NO_ERROR_DETECTED);
    new_data_retriever->SetEmptyRendererWebAppInstallInfo();
    SetPageState(
        {kWebAppUrl, absl::nullopt, ExternalInstallSource::kInternalDefault});

    ExternalInstallOptions new_install_options(
        kWebAppUrl, mojom::UserDisplayMode::kStandalone,
        ExternalInstallSource::kExternalPolicy);
    auto updated_result =
        InstallAndWait(new_install_options, std::move(new_data_retriever));
    ASSERT_TRUE(updated_result.app_id.has_value());
    const webapps::AppId& installed_app_id = *updated_result.app_id;

    EXPECT_EQ(updated_result.code,
              webapps::InstallResultCode::kSuccessNewInstall);

    // Verify icon information, that new data is not written into the DB
    // and the old data still persists.
    const std::vector<apps::IconInfo> new_icon_info =
        registrar().GetAppIconInfos(*updated_result.app_id);
    EXPECT_NE(new_sizes.size(), new_icon_info.size());
    LoadIconsFromDB(installed_app_id, new_sizes);
    // Trying to load new icons returns blank data because the new sizes and
    // colors have not been overwritten.
    EXPECT_EQ(0u, GetIconSizesForApp(installed_app_id).size());
    EXPECT_EQ(0u, GetIconColorsForApp(installed_app_id).size());

    EXPECT_EQ(old_sizes.size(), new_icon_info.size());
    EXPECT_EQ(GetMockIconInfo(icon_size::k64), new_icon_info[0]);
    LoadIconsFromDB(installed_app_id, old_sizes);
    EXPECT_EQ(GetIconSizesForApp(installed_app_id), old_sizes);
    EXPECT_EQ(GetIconColorsForApp(installed_app_id), old_colors);
  }
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(ExternalAppResolutionCommandTest, SuccessWithUninstallAndReplace) {
  GURL old_app_url("http://old-app.com");
  const webapps::AppId old_app =
      test::InstallDummyWebApp(profile(), "old_app", old_app_url);
  auto shortcut_info = std::make_unique<ShortcutInfo>();
  shortcut_info->url = old_app_url;
  shortcut_manager()->SetShortcutInfoForApp(old_app, std::move(shortcut_info));

  ShortcutLocations shortcut_locations;
  shortcut_locations.on_desktop = false;
  shortcut_locations.in_quick_launch_bar = true;
  shortcut_locations.in_startup = true;
  shortcut_manager()->SetAppExistingShortcuts(old_app_url, shortcut_locations);

  ExternalInstallOptions install_options(
      kWebAppUrl, mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kExternalDefault);
  install_options.uninstall_and_replace = {old_app};

  auto data_retriever = std::make_unique<FakeDataRetriever>();
  data_retriever->BuildDefaultDataToRetrieve(kWebAppUrl, kWebAppScope);
  SetPageState(
      {kWebAppUrl, absl::nullopt, ExternalInstallSource::kInternalDefault});

  auto result = InstallAndWait(install_options, std::move(data_retriever));
  EXPECT_EQ(result.code, webapps::InstallResultCode::kSuccessNewInstall);
  ASSERT_TRUE(result.app_id.has_value());
  EXPECT_TRUE(registrar().IsLocallyInstalled(*result.app_id));

  EXPECT_TRUE(os_integration_manager()->did_add_to_desktop());
  auto options = os_integration_manager()->get_last_install_options();
  EXPECT_FALSE(options->add_to_desktop);
  EXPECT_TRUE(options->add_to_quick_launch_bar);
  EXPECT_TRUE(options->os_hooks[OsHookType::kRunOnOsLogin]);
  if (AreOsIntegrationSubManagersEnabled()) {
    absl::optional<proto::WebAppOsIntegrationState> os_state =
        registrar().GetAppCurrentOsIntegrationState(*result.app_id);
    ASSERT_TRUE(os_state.has_value());
    EXPECT_TRUE(os_state->has_shortcut());
    EXPECT_EQ(os_state->run_on_os_login().run_on_os_login_mode(),
              proto::RunOnOsLoginMode::WINDOWED);
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace
}  // namespace web_app
