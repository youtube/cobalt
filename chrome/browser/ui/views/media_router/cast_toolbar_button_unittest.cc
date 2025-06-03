// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_toolbar_button.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/media_router/media_router_ui_service.h"
#include "chrome/browser/ui/media_router/media_router_ui_service_factory.h"
#include "chrome/browser/ui/toolbar/media_router_contextual_menu.h"
#include "chrome/browser/ui/toolbar/mock_media_router_action_controller.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/mirroring_media_controller_host_impl.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/vector_icons/vector_icons.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"

using testing::_;
using testing::WithArg;

namespace media_router {

namespace {

std::unique_ptr<KeyedService> BuildUIService(content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  auto controller = std::make_unique<MockMediaRouterActionController>(profile);
  return std::make_unique<media_router::MediaRouterUIService>(
      profile, std::move(controller));
}

MediaRoute CreateLocalDisplayRoute() {
  return MediaRoute("routeId1", MediaSource("source1"), "sinkId1",
                    "description", true);
}

MediaRoute CreateNonLocalDisplayRoute() {
  return MediaRoute("routeId2", MediaSource("source2"), "sinkId2",
                    "description", false);
}

class MockContextMenuObserver : public MediaRouterContextualMenu::Observer {
 public:
  MOCK_METHOD(void, OnContextMenuShown, (), (override));
  MOCK_METHOD(void, OnContextMenuHidden, (), (override));
};

}  // namespace

class CastToolbarButtonTest : public ChromeViewsTestBase {
 public:
  CastToolbarButtonTest() = default;

  CastToolbarButtonTest(const CastToolbarButtonTest&) = delete;
  CastToolbarButtonTest& operator=(const CastToolbarButtonTest&) = delete;

  ~CastToolbarButtonTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    profile_ = std::make_unique<TestingProfile>();

    ChromeMediaRouterFactory::GetInstance()->SetTestingFactory(
        profile_.get(), base::BindRepeating(&MockMediaRouter::Create));
    MediaRouterUIServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(), base::BindRepeating(&BuildUIService));

    window_ = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams browser_params(profile_.get(), true);
    browser_params.window = window_.get();
    browser_ = std::unique_ptr<Browser>(Browser::Create(browser_params));
    media_router_ = static_cast<MockMediaRouter*>(
        MediaRouterFactory::GetApiForBrowserContext(profile_.get()));
    logger_ = std::make_unique<LoggerImpl>();
    ON_CALL(*media_router_, GetLogger())
        .WillByDefault(testing::Return(logger_.get()));
    mojo::Remote<media_router::mojom::MediaController> controller_remote;
    mirroring_controller_host_ =
        std::make_unique<MirroringMediaControllerHostImpl>(
            std::move(controller_remote));
    ON_CALL(*media_router_, GetMirroringMediaControllerHost(_))
        .WillByDefault(testing::Return(mirroring_controller_host_.get()));

    auto context_menu = std::make_unique<MediaRouterContextualMenu>(
        browser_.get(), false, &context_menu_observer_);

    // Button needs to be in a widget to be able to access ColorProvider.
    widget_ = CreateTestWidget();
    button_ = widget_->SetContentsView(std::make_unique<CastToolbarButton>(
        browser_.get(), media_router_, std::move(context_menu)));

    const ui::ColorProvider* color_provider = button_->GetColorProvider();
    idle_icon_ = gfx::Image(gfx::CreateVectorIcon(
        vector_icons::kMediaRouterIdleIcon,
        color_provider->GetColor(kColorToolbarButtonIcon)));
    idle_chrome_refresh_icon_ = gfx::Image(gfx::CreateVectorIcon(
        vector_icons::kMediaRouterIdleChromeRefreshIcon,
        color_provider->GetColor(kColorToolbarButtonIcon)));
    warning_icon_ = gfx::Image(gfx::CreateVectorIcon(
        vector_icons::kMediaRouterWarningIcon,
        color_provider->GetColor(kColorMediaRouterIconWarning)));
    warning_chrome_refresh_icon_ = gfx::Image(gfx::CreateVectorIcon(
        vector_icons::kMediaRouterWarningChromeRefreshIcon,
        color_provider->GetColor(kColorToolbarButtonIcon)));
    active_icon_ = gfx::Image(gfx::CreateVectorIcon(
        vector_icons::kMediaRouterActiveIcon,
        color_provider->GetColor(kColorMediaRouterIconActive)));
    active_chrome_refresh_icon_ = gfx::Image(gfx::CreateVectorIcon(
        vector_icons::kMediaRouterActiveChromeRefreshIcon,
        color_provider->GetColor(kColorMediaRouterIconActive)));
    paused_icon_ = gfx::Image(gfx::CreateVectorIcon(
        vector_icons::kMediaRouterPausedIcon,
        color_provider->GetColor(kColorToolbarButtonIcon)));
  }

  void TearDown() override {
    widget_.reset();
    browser_.reset();
    window_.reset();
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  gfx::Image GetIcon() {
    return gfx::Image(button_->GetImage(views::Button::STATE_NORMAL));
  }

  std::unique_ptr<BrowserWindow> window_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<CastToolbarButton, DanglingUntriaged> button_ =
      nullptr;  // owned by |widget_|.
  MockContextMenuObserver context_menu_observer_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<LoggerImpl> logger_;
  raw_ptr<MockMediaRouter, DanglingUntriaged> media_router_ = nullptr;
  std::unique_ptr<MirroringMediaControllerHostImpl> mirroring_controller_host_;

  gfx::Image idle_icon_;
  gfx::Image idle_chrome_refresh_icon_;
  gfx::Image warning_icon_;
  gfx::Image warning_chrome_refresh_icon_;
  gfx::Image active_icon_;
  gfx::Image active_chrome_refresh_icon_;
  gfx::Image paused_icon_;

  const std::vector<MediaRoute> local_display_route_list_ = {
      CreateLocalDisplayRoute()};
  const std::vector<MediaRoute> non_local_display_route_list_ = {
      CreateNonLocalDisplayRoute()};
};

TEST_F(CastToolbarButtonTest, ShowAndHideButton) {
  ASSERT_FALSE(button_->GetVisible());
  button_->ShowIcon();
  EXPECT_TRUE(button_->GetVisible());
  button_->HideIcon();
  EXPECT_FALSE(button_->GetVisible());
}

TEST_F(CastToolbarButtonTest, UpdateIssues) {
  button_->UpdateIcon();
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_icon_, GetIcon()));

  button_->OnIssue(Issue(IssueInfo(
      "title notification", IssueInfo::Severity::NOTIFICATION, "sinkId1")));
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_icon_, GetIcon()));

  button_->OnIssue(Issue(
      IssueInfo("title warning", IssueInfo::Severity::WARNING, "sinkId1")));
  EXPECT_TRUE(gfx::test::AreImagesEqual(warning_icon_, GetIcon()));

  button_->OnIssuesCleared();
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_icon_, GetIcon()));
}

TEST_F(CastToolbarButtonTest, UpdateIssuesChromeResfresh) {
  // Enable the proper features / prefs.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kChromeRefresh2023);

  button_->UpdateIcon();
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_chrome_refresh_icon_, GetIcon()));

  button_->OnIssue(Issue(IssueInfo(
      "title notification", IssueInfo::Severity::NOTIFICATION, "sinkId1")));
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_chrome_refresh_icon_, GetIcon()));

  button_->OnIssue(Issue(
      IssueInfo("title warning", IssueInfo::Severity::WARNING, "sinkId1")));
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(warning_chrome_refresh_icon_, GetIcon()));

  button_->OnIssuesCleared();
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_chrome_refresh_icon_, GetIcon()));
}

TEST_F(CastToolbarButtonTest, UpdateRoutes) {
  button_->UpdateIcon();
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_icon_, GetIcon()));

  button_->OnRoutesUpdated(local_display_route_list_);
  EXPECT_TRUE(gfx::test::AreImagesEqual(active_icon_, GetIcon()));

  // The idle icon should be shown when we only have non-local and/or
  // non-display routes.
  button_->OnRoutesUpdated(non_local_display_route_list_);
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_icon_, GetIcon()));

  button_->OnRoutesUpdated({});
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_icon_, GetIcon()));
}

TEST_F(CastToolbarButtonTest, UpdateRoutesChromeRefresh) {
  // Enable the proper features / prefs.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kChromeRefresh2023);

  button_->UpdateIcon();
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_chrome_refresh_icon_, GetIcon()));

  button_->OnRoutesUpdated(local_display_route_list_);
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(active_chrome_refresh_icon_, GetIcon()));

  // The idle icon should be shown when we only have non-local and/or
  // non-display routes.
  button_->OnRoutesUpdated(non_local_display_route_list_);
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_chrome_refresh_icon_, GetIcon()));

  button_->OnRoutesUpdated({});
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_chrome_refresh_icon_, GetIcon()));
}

TEST_F(CastToolbarButtonTest, PausedIcon) {
  // Enable the proper features / prefs.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAccessCodeCastFreezeUI);
  profile_->GetPrefs()->SetBoolean(prefs::kAccessCodeCastEnabled, true);

  button_->UpdateIcon();
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_chrome_refresh_icon_, GetIcon()));

  media_router::mojom::MediaStatusPtr status = mojom::MediaStatus::New();
  status->can_play_pause = true;
  status->play_state = mojom::MediaStatus::PlayState::PAUSED;
  mirroring_controller_host_.get()->OnMediaStatusUpdated(std::move(status));

  button_->OnRoutesUpdated(local_display_route_list_);
  EXPECT_TRUE(gfx::test::AreImagesEqual(paused_icon_, GetIcon()));
}

}  // namespace media_router
