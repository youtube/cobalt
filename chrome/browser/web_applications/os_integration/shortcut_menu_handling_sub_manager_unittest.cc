// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/install_result_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/web_applications/os_integration/web_app_shortcuts_menu_win.h"
#endif

using ::testing::ElementsAreArray;
using ::testing::Eq;

namespace web_app {

namespace {

class ShortcutMenuHandlingSubManagerTestBase : public WebAppTest {
 public:
  const GURL kWebAppUrl = GURL("https://example.com/path/index.html");

  ShortcutMenuHandlingSubManagerTestBase() = default;
  ~ShortcutMenuHandlingSubManagerTestBase() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_ =
          OsIntegrationTestOverrideImpl::OverrideForTesting(base::GetHomeDir());
    }

    provider_ = FakeWebAppProvider::Get(profile());

    auto file_handler_manager =
        std::make_unique<WebAppFileHandlerManager>(profile());
    auto protocol_handler_manager =
        std::make_unique<WebAppProtocolHandlerManager>(profile());
    auto shortcut_manager = std::make_unique<WebAppShortcutManager>(
        profile(), /*icon_manager=*/nullptr, file_handler_manager.get(),
        protocol_handler_manager.get());
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

  ShortcutsMenuIconBitmaps MakeIconBitmaps(
      const std::vector<GeneratedIconsInfo>& icons_info,
      int num_menu_items) {
    ShortcutsMenuIconBitmaps shortcuts_menu_icons;

    for (int i = 0; i < num_menu_items; ++i) {
      IconBitmaps menu_item_icon_map;
      for (const GeneratedIconsInfo& info : icons_info) {
        DCHECK_EQ(info.sizes_px.size(), info.colors.size());
        std::map<SquareSizePx, SkBitmap> generated_bitmaps;
        for (size_t j = 0; j < info.sizes_px.size(); ++j) {
          AddGeneratedIcon(&generated_bitmaps, info.sizes_px[j],
                           info.colors[j]);
        }
        menu_item_icon_map.SetBitmapsForPurpose(info.purpose,
                                                std::move(generated_bitmaps));
      }
      shortcuts_menu_icons.push_back(std::move(menu_item_icon_map));
    }

    return shortcuts_menu_icons;
  }

  std::vector<WebAppShortcutsMenuItemInfo>
  CreateShortcutMenuItemInfoFromBitmaps(
      const ShortcutsMenuIconBitmaps& menu_bitmaps) {
    std::vector<WebAppShortcutsMenuItemInfo> item_infos;
    int index = 0;
    for (const auto& icon_bitmap : menu_bitmaps) {
      WebAppShortcutsMenuItemInfo shortcut_info;
      shortcut_info.name = base::UTF8ToUTF16(
          base::StrCat({"shortcut_name", base::NumberToString(index)}));
      shortcut_info.url =
          GURL(base::StrCat({kWebAppUrl.spec(), base::NumberToString(index)}));

      // The URLs used do not matter because Execute() does not take the urls
      // into account, but we still need those to initialize the mock data
      // structure so that the GURL checks in WebAppDatabase can pass.
      for (const auto& [size, data] : icon_bitmap.any) {
        WebAppShortcutsMenuItemInfo::Icon icon_data;
        icon_data.square_size_px = size;
        icon_data.url = GURL("https://icon.any/");
        shortcut_info.any.push_back(std::move(icon_data));
      }

      for (const auto& [size, data] : icon_bitmap.maskable) {
        WebAppShortcutsMenuItemInfo::Icon icon_data;
        icon_data.square_size_px = size;
        icon_data.url = GURL("https://icon.maskable/");
        shortcut_info.maskable.push_back(std::move(icon_data));
      }

      for (const auto& [size, data] : icon_bitmap.monochrome) {
        WebAppShortcutsMenuItemInfo::Icon icon_data;
        icon_data.square_size_px = size;
        icon_data.url = GURL("https://icon.monochrome/");
        shortcut_info.monochrome.push_back(std::move(icon_data));
      }

      item_infos.push_back(std::move(shortcut_info));
      index++;
    }
    return item_infos;
  }

  web_app::AppId InstallWebAppWithShortcutMenuIcons(
      ShortcutsMenuIconBitmaps shortcuts_menu_icons) {
    std::unique_ptr<WebAppInstallInfo> info =
        std::make_unique<WebAppInstallInfo>();
    info->start_url = kWebAppUrl;
    info->title = u"Test App";
    info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
    info->shortcuts_menu_icon_bitmaps = shortcuts_menu_icons;
    info->shortcuts_menu_item_infos =
        CreateShortcutMenuItemInfoFromBitmaps(shortcuts_menu_icons);
    base::test::TestFuture<const AppId&, webapps::InstallResultCode> result;
    // InstallFromInfoWithParams is used instead of InstallFromInfo, because
    // InstallFromInfo doesn't register OS integration.
    provider().scheduler().InstallFromInfoWithParams(
        std::move(info), /*overwrite_existing_manifest_fields=*/true,
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        result.GetCallback(), WebAppInstallParams());
    bool success = result.Wait();
    EXPECT_TRUE(success);
    if (!success) {
      return AppId();
    }
    EXPECT_EQ(result.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessNewInstall);
    return result.Get<AppId>();
  }

 protected:
  WebAppProvider& provider() { return *provider_; }

 private:
  raw_ptr<FakeWebAppProvider> provider_;
  std::unique_ptr<OsIntegrationTestOverrideImpl::BlockingRegistration>
      test_override_;
};

// Synchronize tests only. Tests here should only verify DB updates.
class ShortcutMenuHandlingSubManagerConfigureTest
    : public ShortcutMenuHandlingSubManagerTestBase,
      public ::testing::WithParamInterface<OsIntegrationSubManagersState> {
 public:
  void SetUp() override {
    if (GetParam() == OsIntegrationSubManagersState::kSaveStateToDB) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kOsIntegrationSubManagers, {{"stage", "write_config"}});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{features::kOsIntegrationSubManagers});
    }
    ShortcutMenuHandlingSubManagerTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(ShortcutMenuHandlingSubManagerConfigureTest, TestConfigure) {
  const int num_menu_items = 2;

  const std::vector<int> sizes = {icon_size::k64, icon_size::k128};
  const std::vector<SkColor> colors = {SK_ColorRED, SK_ColorRED};
  const AppId& app_id = InstallWebAppWithShortcutMenuIcons(
      MakeIconBitmaps({{IconPurpose::ANY, sizes, colors},
                       {IconPurpose::MASKABLE, sizes, colors},
                       {IconPurpose::MONOCHROME, sizes, colors}},
                      num_menu_items));

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  if (AreOsIntegrationSubManagersEnabled()) {
    EXPECT_TRUE(
        os_integration_state.shortcut_menus().shortcut_menu_info_size() ==
        num_menu_items);

    int num_sizes = static_cast<int>(sizes.size());

    for (int menu_index = 0; menu_index < num_menu_items; menu_index++) {
      EXPECT_THAT(os_integration_state.shortcut_menus()
                      .shortcut_menu_info(menu_index)
                      .shortcut_name(),
                  testing::Eq(base::StrCat(
                      {"shortcut_name", base::NumberToString(menu_index)})));

      EXPECT_THAT(os_integration_state.shortcut_menus()
                      .shortcut_menu_info(menu_index)
                      .shortcut_launch_url(),
                  testing::Eq(base::StrCat(
                      {kWebAppUrl.spec(), base::NumberToString(menu_index)})));

      EXPECT_EQ(os_integration_state.shortcut_menus()
                    .shortcut_menu_info(menu_index)
                    .icon_data_any_size(),
                num_sizes);
      EXPECT_EQ(os_integration_state.shortcut_menus()
                    .shortcut_menu_info(menu_index)
                    .icon_data_maskable_size(),
                num_sizes);
      EXPECT_EQ(os_integration_state.shortcut_menus()
                    .shortcut_menu_info(menu_index)
                    .icon_data_monochrome_size(),
                num_sizes);

      for (int size_index = 0; size_index < num_sizes; size_index++) {
        EXPECT_TRUE(os_integration_state.shortcut_menus()
                        .shortcut_menu_info(menu_index)
                        .icon_data_any(size_index)
                        .icon_size() == sizes[size_index]);
        EXPECT_TRUE(os_integration_state.shortcut_menus()
                        .shortcut_menu_info(menu_index)
                        .icon_data_any(size_index)
                        .has_timestamp());
        EXPECT_TRUE(os_integration_state.shortcut_menus()
                        .shortcut_menu_info(menu_index)
                        .icon_data_maskable(size_index)
                        .icon_size() == sizes[size_index]);
        EXPECT_TRUE(os_integration_state.shortcut_menus()
                        .shortcut_menu_info(menu_index)
                        .icon_data_maskable(size_index)
                        .has_timestamp());
        EXPECT_TRUE(os_integration_state.shortcut_menus()
                        .shortcut_menu_info(menu_index)
                        .icon_data_monochrome(size_index)
                        .icon_size() == sizes[size_index]);
        EXPECT_TRUE(os_integration_state.shortcut_menus()
                        .shortcut_menu_info(menu_index)
                        .icon_data_monochrome(size_index)
                        .has_timestamp());
      }
    }
  } else {
    ASSERT_FALSE(os_integration_state.has_shortcut_menus());
  }
}

// This tests our handling of https://crbug.com/1427444.
TEST_P(ShortcutMenuHandlingSubManagerConfigureTest, NoDownloadedIcons_1427444) {
  const int num_menu_items = 2;

  const std::vector<int> sizes = {icon_size::k64, icon_size::k128};
  const std::vector<SkColor> colors = {SK_ColorRED, SK_ColorRED};
  const AppId& app_id = InstallWebAppWithShortcutMenuIcons(
      MakeIconBitmaps({{IconPurpose::ANY, sizes, colors},
                       {IconPurpose::MASKABLE, sizes, colors},
                       {IconPurpose::MONOCHROME, sizes, colors}},
                      num_menu_items));
  // Remove the downloaded icons & resync os integration.
  {
    ScopedRegistryUpdate remove_downloaded(&provider().sync_bridge_unsafe());
    remove_downloaded->UpdateApp(app_id)->SetDownloadedShortcutsMenuIconsSizes(
        {});
  }
  if (AreOsIntegrationSubManagersEnabled()) {
    base::test::TestFuture<void> future;
    provider().scheduler().SynchronizeOsIntegration(app_id,
                                                    future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  if (AreOsIntegrationSubManagersEnabled()) {
    EXPECT_TRUE(
        os_integration_state.shortcut_menus().shortcut_menu_info_size() ==
        num_menu_items);

    for (int menu_index = 0; menu_index < num_menu_items; menu_index++) {
      EXPECT_THAT(os_integration_state.shortcut_menus()
                      .shortcut_menu_info(menu_index)
                      .shortcut_name(),
                  testing::Eq(base::StrCat(
                      {"shortcut_name", base::NumberToString(menu_index)})));

      EXPECT_THAT(os_integration_state.shortcut_menus()
                      .shortcut_menu_info(menu_index)
                      .shortcut_launch_url(),
                  testing::Eq(base::StrCat(
                      {kWebAppUrl.spec(), base::NumberToString(menu_index)})));

      EXPECT_EQ(os_integration_state.shortcut_menus()
                    .shortcut_menu_info(menu_index)
                    .icon_data_any_size(),
                0);
      EXPECT_EQ(os_integration_state.shortcut_menus()
                    .shortcut_menu_info(menu_index)
                    .icon_data_maskable_size(),
                0);
      EXPECT_EQ(os_integration_state.shortcut_menus()
                    .shortcut_menu_info(menu_index)
                    .icon_data_monochrome_size(),
                0);
    }
  } else {
    ASSERT_FALSE(os_integration_state.has_shortcut_menus());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ShortcutMenuHandlingSubManagerConfigureTest,
    ::testing::Values(OsIntegrationSubManagersState::kSaveStateToDB,
                      OsIntegrationSubManagersState::kDisabled),
    test::GetOsIntegrationSubManagersTestName);

// Synchronize and Execute tests from here onwards. Tests here should
// verify both DB updates as well as OS registrations/unregistrations.
class ShortcutMenuHandlingSubManagerExecuteTest
    : public ShortcutMenuHandlingSubManagerTestBase,
      public ::testing::WithParamInterface<OsIntegrationSubManagersState> {
 public:
  ShortcutMenuHandlingSubManagerExecuteTest() = default;
  ~ShortcutMenuHandlingSubManagerExecuteTest() override = default;

  void SetUp() override {
    if (GetParam() == OsIntegrationSubManagersState::kSaveStateAndExecute) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kOsIntegrationSubManagers,
          {{"stage", "execute_and_write_config"}});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{features::kOsIntegrationSubManagers});
    }
    ShortcutMenuHandlingSubManagerTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(ShortcutMenuHandlingSubManagerExecuteTest, InstallWritesCorrectData) {
  const int num_menu_items = 2;

  const std::vector<int> sizes = {icon_size::k64, icon_size::k128};
  const std::vector<SkColor> colors = {SK_ColorRED, SK_ColorRED};
  const AppId& app_id = InstallWebAppWithShortcutMenuIcons(
      MakeIconBitmaps({{IconPurpose::ANY, sizes, colors},
                       {IconPurpose::MASKABLE, sizes, colors},
                       {IconPurpose::MONOCHROME, sizes, colors}},
                      num_menu_items));

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();

  if (AreSubManagersExecuteEnabled()) {
#if BUILDFLAG(IS_WIN)
    const std::wstring app_user_model_id =
        web_app::GenerateAppUserModelId(profile()->GetPath(), app_id);
    ASSERT_TRUE(
        OsIntegrationTestOverrideImpl::Get()->IsShortcutsMenuRegisteredForApp(
            app_user_model_id));
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->GetCountOfShortcutIconsCreated(
            app_user_model_id),
        testing::Eq(num_menu_items));
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->GetIconColorsForShortcutsMenu(
            app_user_model_id),
        testing::ElementsAreArray(colors));
#else
    ASSERT_FALSE(
        OsIntegrationTestOverrideImpl::Get()->AreShortcutsMenuRegistered());
#endif
  } else {
    ASSERT_FALSE(os_integration_state.has_shortcut_menus());
  }
}

TEST_P(ShortcutMenuHandlingSubManagerExecuteTest,
       UninstallRemovesShortcutMenuItems) {
  const int num_menu_items = 3;

  const std::vector<int> sizes = {icon_size::k64, icon_size::k128,
                                  icon_size::k256};
  const std::vector<SkColor> colors = {SK_ColorBLUE, SK_ColorBLUE,
                                       SK_ColorBLUE};
  const AppId& app_id = InstallWebAppWithShortcutMenuIcons(
      MakeIconBitmaps({{IconPurpose::ANY, sizes, colors},
                       {IconPurpose::MASKABLE, sizes, colors},
                       {IconPurpose::MONOCHROME, sizes, colors}},
                      num_menu_items));

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();

  if (AreSubManagersExecuteEnabled()) {
#if BUILDFLAG(IS_WIN)
    const std::wstring app_user_model_id =
        web_app::GenerateAppUserModelId(profile()->GetPath(), app_id);
    ASSERT_TRUE(
        OsIntegrationTestOverrideImpl::Get()->IsShortcutsMenuRegisteredForApp(
            app_user_model_id));
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->GetCountOfShortcutIconsCreated(
            app_user_model_id),
        testing::Eq(num_menu_items));
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->GetIconColorsForShortcutsMenu(
            app_user_model_id),
        testing::ElementsAreArray(colors));
#else
    ASSERT_FALSE(
        OsIntegrationTestOverrideImpl::Get()->AreShortcutsMenuRegistered());
#endif
  } else {
    ASSERT_FALSE(os_integration_state.has_shortcut_menus());
  }

  test::UninstallAllWebApps(profile());

  if (AreSubManagersExecuteEnabled()) {
#if BUILDFLAG(IS_WIN)
    const std::wstring app_user_model_id =
        web_app::GenerateAppUserModelId(profile()->GetPath(), app_id);
    ASSERT_TRUE(os_integration_state.has_shortcut_menus());
    ASSERT_FALSE(
        OsIntegrationTestOverrideImpl::Get()->IsShortcutsMenuRegisteredForApp(
            app_user_model_id));
#else
    ASSERT_FALSE(
        OsIntegrationTestOverrideImpl::Get()->AreShortcutsMenuRegistered());
#endif
  } else {
    ASSERT_FALSE(os_integration_state.has_shortcut_menus());
  }
}

TEST_P(ShortcutMenuHandlingSubManagerExecuteTest, UpdateShortcutMenuItems) {
  const int num_menu_items = 2;
  const std::vector<int> sizes = {icon_size::k32, icon_size::k48};
  const std::vector<SkColor> colors = {SK_ColorCYAN, SK_ColorCYAN};
  const AppId& app_id = InstallWebAppWithShortcutMenuIcons(
      MakeIconBitmaps({{IconPurpose::ANY, sizes, colors},
                       {IconPurpose::MASKABLE, sizes, colors},
                       {IconPurpose::MONOCHROME, sizes, colors}},
                      num_menu_items));

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();

  if (AreSubManagersExecuteEnabled()) {
#if BUILDFLAG(IS_WIN)
    const std::wstring app_user_model_id =
        web_app::GenerateAppUserModelId(profile()->GetPath(), app_id);
    ASSERT_TRUE(
        OsIntegrationTestOverrideImpl::Get()->IsShortcutsMenuRegisteredForApp(
            app_user_model_id));
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->GetCountOfShortcutIconsCreated(
            app_user_model_id),
        testing::Eq(num_menu_items));
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->GetIconColorsForShortcutsMenu(
            app_user_model_id),
        testing::ElementsAreArray(colors));
#else
    ASSERT_FALSE(
        OsIntegrationTestOverrideImpl::Get()->AreShortcutsMenuRegistered());
#endif
  } else {
    ASSERT_FALSE(os_integration_state.has_shortcut_menus());
  }

  const int updated_num_menu_items = 3;
  const std::vector<int> updated_sizes = {icon_size::k64, icon_size::k128,
                                          icon_size::k256};
  const std::vector<SkColor> updated_colors = {SK_ColorYELLOW, SK_ColorYELLOW,
                                               SK_ColorYELLOW};
  const AppId& updated_app_id =
      InstallWebAppWithShortcutMenuIcons(MakeIconBitmaps(
          {{IconPurpose::ANY, updated_sizes, updated_colors},
           {IconPurpose::MASKABLE, updated_sizes, updated_colors},
           {IconPurpose::MONOCHROME, updated_sizes, updated_colors}},
          updated_num_menu_items));
  ASSERT_THAT(updated_app_id, testing::Eq(app_id));

  state = provider().registrar_unsafe().GetAppCurrentOsIntegrationState(
      updated_app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& updated_os_integration_state =
      state.value();

  if (AreSubManagersExecuteEnabled()) {
#if BUILDFLAG(IS_WIN)
    const std::wstring updated_model_id =
        web_app::GenerateAppUserModelId(profile()->GetPath(), updated_app_id);
    ASSERT_TRUE(
        OsIntegrationTestOverrideImpl::Get()->IsShortcutsMenuRegisteredForApp(
            updated_model_id));
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->GetCountOfShortcutIconsCreated(
            updated_model_id),
        testing::Eq(updated_num_menu_items));
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->GetIconColorsForShortcutsMenu(
            updated_model_id),
        testing::ElementsAreArray(updated_colors));
#else
    ASSERT_FALSE(
        OsIntegrationTestOverrideImpl::Get()->AreShortcutsMenuRegistered());
#endif
  } else {
    ASSERT_FALSE(updated_os_integration_state.has_shortcut_menus());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ShortcutMenuHandlingSubManagerExecuteTest,
    ::testing::Values(OsIntegrationSubManagersState::kSaveStateAndExecute,
                      OsIntegrationSubManagersState::kDisabled),
    test::GetOsIntegrationSubManagersTestName);

}  // namespace

}  // namespace web_app
