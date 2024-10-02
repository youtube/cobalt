// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"

#include <tuple>

#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_test_helper.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/test/test_views.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_factory.h"
#include "ui/linux/linux_ui_getter.h"

class FakeLinuxUiGetter : public ui::LinuxUiGetter {
 public:
  explicit FakeLinuxUiGetter(bool use_system_theme)
      : use_system_theme_(use_system_theme) {}

  ui::LinuxUiTheme* GetForWindow(aura::Window* window) override {
    return GetForProfile(nullptr);
  }

  ui::LinuxUiTheme* GetForProfile(Profile* profile) override {
    return use_system_theme_ ? ui::GetDefaultLinuxUiTheme() : nullptr;
  }

 private:
  const bool use_system_theme_;
};
#endif

// Tests web-app windows that use the OpaqueBrowserFrameView implementation
// for their non client frames.
class WebAppOpaqueBrowserFrameViewTest : public InProcessBrowserTest {
 public:
  WebAppOpaqueBrowserFrameViewTest() = default;

  WebAppOpaqueBrowserFrameViewTest(const WebAppOpaqueBrowserFrameViewTest&) =
      delete;
  WebAppOpaqueBrowserFrameViewTest& operator=(
      const WebAppOpaqueBrowserFrameViewTest&) = delete;

  ~WebAppOpaqueBrowserFrameViewTest() override = default;

  static GURL GetAppURL() { return GURL("https://test.org"); }

  void SetUpOnMainThread() override {
    SetThemeMode(ThemeMode::kDefault);
#if BUILDFLAG(IS_LINUX)
    ui::LinuxUiGetter::set_instance(nullptr);
#endif
  }

  bool InstallAndLaunchWebApp(
      absl::optional<SkColor> theme_color = absl::nullopt) {
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = GetAppURL();
    web_app_info->scope = GetAppURL().GetWithoutFilename();
    web_app_info->theme_color = theme_color;

    web_app::AppId app_id = web_app::test::InstallWebApp(
        browser()->profile(), std::move(web_app_info));
    Browser* app_browser =
        web_app::LaunchWebAppBrowser(browser()->profile(), app_id);

    browser_view_ = BrowserView::GetBrowserViewForBrowser(app_browser);
    views::NonClientFrameView* frame_view =
        browser_view_->GetWidget()->non_client_view()->frame_view();

    // Not all platform configurations use OpaqueBrowserFrameView for their
    // browser windows, see |CreateBrowserNonClientFrameView()|.
    bool is_opaque_browser_frame_view =
        views::IsViewClass<OpaqueBrowserFrameView>(frame_view);
#if BUILDFLAG(IS_LINUX)
    DCHECK(is_opaque_browser_frame_view);
#else
    if (!is_opaque_browser_frame_view)
      return false;
#endif

    opaque_browser_frame_view_ =
        static_cast<OpaqueBrowserFrameView*>(frame_view);
    web_app_frame_toolbar_ = browser_view_->web_app_frame_toolbar_for_testing();
    DCHECK(web_app_frame_toolbar_);
    DCHECK(web_app_frame_toolbar_->GetVisible());

    return true;
  }

  int GetRestoredTitleBarHeight() {
    return opaque_browser_frame_view_->layout()->NonClientTopHeight(true);
  }

  enum class ThemeMode {
    kSystem,
    kDefault,
  };

  void SetThemeMode(ThemeMode theme_mode) {
#if BUILDFLAG(IS_LINUX)
    bool use_system_theme = theme_mode == ThemeMode::kSystem;
    linux_ui_getter_ = std::make_unique<FakeLinuxUiGetter>(use_system_theme);
#endif
    ThemeService* theme_service =
        ThemeServiceFactory::GetForProfile(browser()->profile());
    if (theme_mode == ThemeMode::kSystem)
      theme_service->UseSystemTheme();
    else
      theme_service->UseDefaultTheme();
    ASSERT_EQ(theme_service->UsingDefaultTheme(),
              theme_mode == ThemeMode::kDefault);
  }

  raw_ptr<BrowserView, DanglingUntriaged> browser_view_ = nullptr;
  raw_ptr<OpaqueBrowserFrameView, DanglingUntriaged>
      opaque_browser_frame_view_ = nullptr;
  raw_ptr<WebAppFrameToolbarView, DanglingUntriaged> web_app_frame_toolbar_ =
      nullptr;
#if BUILDFLAG(IS_LINUX)
  std::unique_ptr<ui::LinuxUiGetter> linux_ui_getter_;
#endif

  // Disable animations.
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode_{
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION};
};

IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewTest, NoThemeColor) {
  if (!InstallAndLaunchWebApp())
    return;
  EXPECT_EQ(web_app_frame_toolbar_->active_color_for_testing(),
            web_app_frame_toolbar_->GetColorProvider()->GetColor(
                kColorFrameCaptionActive));
}

#if BUILDFLAG(IS_LINUX)
// The app theme color should be ignored in system theme mode.
IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewTest, SystemThemeColor) {
  SetThemeMode(ThemeMode::kSystem);

  // Read unthemed native frame color.
  SkColor native_frame_color =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->frame()
          ->GetFrameView()
          ->GetFrameColor(BrowserFrameActiveState::kActive);
  SkColor expected_caption_color =
      color_utils::GetColorWithMaxContrast(native_frame_color);

  // Install web app with theme color contrasting against native frame color.
  SkColor theme_color =
      color_utils::GetColorWithMaxContrast(native_frame_color);
  EXPECT_NE(color_utils::IsDark(theme_color),
            color_utils::IsDark(native_frame_color));
  ASSERT_TRUE(InstallAndLaunchWebApp(theme_color));

  // App theme color should be ignored in favor of native system theme.
  EXPECT_EQ(opaque_browser_frame_view_->GetFrameColor(
                BrowserFrameActiveState::kActive),
            native_frame_color);
  EXPECT_EQ(opaque_browser_frame_view_->GetCaptionColor(
                BrowserFrameActiveState::kActive),
            expected_caption_color);
  EXPECT_EQ(web_app_frame_toolbar_->active_color_for_testing(),
            expected_caption_color);
}
#endif  // BUILDFLAG(IS_LINUX)

IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewTest, LightThemeColor) {
  if (!InstallAndLaunchWebApp(SK_ColorYELLOW))
    return;
  EXPECT_EQ(web_app_frame_toolbar_->active_color_for_testing(),
            web_app_frame_toolbar_->GetColorProvider()->GetColor(
                kColorFrameCaptionActive));
  EXPECT_TRUE(
      color_utils::IsDark(*web_app_frame_toolbar_->active_color_for_testing()));
}

IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewTest, DarkThemeColor) {
  if (!InstallAndLaunchWebApp(SK_ColorBLUE))
    return;
  EXPECT_EQ(web_app_frame_toolbar_->active_color_for_testing(),
            web_app_frame_toolbar_->GetColorProvider()->GetColor(
                kColorFrameCaptionActive));
  EXPECT_FALSE(
      color_utils::IsDark(*web_app_frame_toolbar_->active_color_for_testing()));
}

IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewTest, MediumThemeColor) {
  // Use the theme color for Gmail.
  if (!InstallAndLaunchWebApp(SkColorSetRGB(0xD6, 0x49, 0x3B)))
    return;
  EXPECT_EQ(web_app_frame_toolbar_->active_color_for_testing(),
            web_app_frame_toolbar_->GetColorProvider()->GetColor(
                kColorFrameCaptionActive));
  EXPECT_FALSE(
      color_utils::IsDark(*web_app_frame_toolbar_->active_color_for_testing()));
}

IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewTest, StaticTitleBarHeight) {
  if (!InstallAndLaunchWebApp())
    return;

  RunScheduledLayouts();
  const int title_bar_height = GetRestoredTitleBarHeight();
  EXPECT_GT(title_bar_height, 0);

  // Add taller children to the web app frame toolbar RHS.
  const int container_height = web_app_frame_toolbar_->height();
  web_app_frame_toolbar_->get_right_container_for_testing()->AddChildView(
      new views::StaticSizedView(gfx::Size(1, title_bar_height * 2)));
  RunScheduledLayouts();

  // The height of the web app frame toolbar and title bar should not be
  // affected.
  EXPECT_EQ(container_height, web_app_frame_toolbar_->height());
  EXPECT_EQ(title_bar_height, GetRestoredTitleBarHeight());
}

// Tests for the appearance of the origin text in the titlebar. The origin text
// shows and then hides both when the window is first opened and any time the
// titlebar's appearance changes.
// TODO(crbug.com/1337118): Revise this test.
IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewTest, OriginTextVisibility) {
  ui_test_utils::UrlLoadObserver url_observer(
      GetAppURL(), content::NotificationService::AllSources());

  if (!InstallAndLaunchWebApp())
    return;

  views::View* web_app_origin_text =
      web_app_frame_toolbar_->GetViewByID(VIEW_ID_WEB_APP_ORIGIN_TEXT);
  // Keep track of the number of times the view is made visible or hidden.
  int visible_count = 0, hidden_count = 0;
  auto visibility_change_counter = [](views::View* view, int* visible_count,
                                      int* hidden_count) {
    if (view->GetVisible())
      (*visible_count)++;
    else
      (*hidden_count)++;
  };
  auto subscription = web_app_origin_text->AddVisibleChangedCallback(
      base::BindRepeating(visibility_change_counter, web_app_origin_text,
                          &visible_count, &hidden_count));

  // Starts off visible, then animates out.
  {
    EXPECT_TRUE(web_app_origin_text->GetVisible());
    base::RunLoop view_hidden_runloop;
    auto callback_subscription = web_app_origin_text->AddVisibleChangedCallback(
        view_hidden_runloop.QuitClosure());
    view_hidden_runloop.Run();
    EXPECT_EQ(0, visible_count);
    EXPECT_EQ(1, hidden_count);
    EXPECT_FALSE(web_app_origin_text->GetVisible());
  }

  // The app changes the theme. The origin text should show again and then hide.
  {
    base::RunLoop view_hidden_runloop;
    base::RunLoop view_shown_runloop;
    auto quit_runloop = base::BindLambdaForTesting(
        [&web_app_origin_text, &view_hidden_runloop, &view_shown_runloop]() {
          if (web_app_origin_text->GetVisible())
            view_shown_runloop.Quit();
          else
            view_hidden_runloop.Quit();
        });
    auto callback_subscription =
        web_app_origin_text->AddVisibleChangedCallback(quit_runloop);
    // Make sure the navigation has finished before proceeding.
    url_observer.Wait();
    ASSERT_TRUE(ExecJs(
        browser_view_->GetActiveWebContents()->GetPrimaryMainFrame(),
        "var meta = document.head.appendChild(document.createElement('meta'));"
        "meta.name = 'theme-color';"
        "meta.content = '#123456';"));
    view_shown_runloop.Run();
    EXPECT_EQ(1, visible_count);
    view_hidden_runloop.Run();
    EXPECT_EQ(2, hidden_count);
    EXPECT_FALSE(web_app_origin_text->GetVisible());
  }
}

IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewTest, Fullscreen) {
  if (!InstallAndLaunchWebApp())
    return;

  opaque_browser_frame_view_->frame()->SetFullscreen(true);
  browser_view_->GetWidget()->LayoutRootViewIfNecessary();

  // Verify that all children except the ClientView are hidden when the window
  // is fullscreened.
  for (views::View* child : opaque_browser_frame_view_->children()) {
    EXPECT_EQ(views::IsViewClass<views::ClientView>(child),
              child->GetVisible());
  }
}

#if BUILDFLAG(IS_WIN)
class WebAppOpaqueBrowserFrameViewWindowControlsOverlayTest
    : public InProcessBrowserTest {
 public:
  WebAppOpaqueBrowserFrameViewWindowControlsOverlayTest() = default;
  WebAppOpaqueBrowserFrameViewWindowControlsOverlayTest(
      const WebAppOpaqueBrowserFrameViewWindowControlsOverlayTest&) = delete;
  WebAppOpaqueBrowserFrameViewWindowControlsOverlayTest& operator=(
      const WebAppOpaqueBrowserFrameViewWindowControlsOverlayTest&) = delete;

  ~WebAppOpaqueBrowserFrameViewWindowControlsOverlayTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    embedded_test_server()->ServeFilesFromDirectory(temp_dir_.GetPath());
    ASSERT_TRUE(embedded_test_server()->Start());

    InProcessBrowserTest::SetUp();
  }

  bool InstallAndLaunchWebAppWithWindowControlsOverlay() {
    GURL start_url = web_app_frame_toolbar_helper_
                         .LoadWindowControlsOverlayTestPageWithDataAndGetURL(
                             embedded_test_server(), &temp_dir_);

    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = start_url;
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->display_mode = blink::mojom::DisplayMode::kStandalone;
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;
    web_app_info->title = u"A Web App";
    web_app_info->display_override = {
        blink::mojom::DisplayMode::kWindowControlsOverlay};

    web_app::AppId app_id = web_app::test::InstallWebApp(
        browser()->profile(), std::move(web_app_info));

    Browser* app_browser =
        web_app::LaunchWebAppBrowser(browser()->profile(), app_id);

    web_app::NavigateToURLAndWait(app_browser, start_url);

    browser_view_ = BrowserView::GetBrowserViewForBrowser(app_browser);
    views::NonClientFrameView* frame_view =
        browser_view_->GetWidget()->non_client_view()->frame_view();

    // Not all platform configurations use OpaqueBrowserFrameView for their
    // browser windows, see |CreateBrowserNonClientFrameView()|.
    bool is_opaque_browser_frame_view =
        views::IsViewClass<OpaqueBrowserFrameView>(frame_view);

    if (!is_opaque_browser_frame_view)
      return false;

    opaque_browser_frame_view_ =
        static_cast<OpaqueBrowserFrameView*>(frame_view);
    auto* web_app_frame_toolbar =
        browser_view_->web_app_frame_toolbar_for_testing();
    DCHECK(web_app_frame_toolbar);
    DCHECK(web_app_frame_toolbar->GetVisible());

    return true;
  }

  void ToggleWindowControlsOverlayEnabledAndWait() {
    auto* web_contents = browser_view_->GetActiveWebContents();
    web_app_frame_toolbar_helper_.SetupGeometryChangeCallback(web_contents);
    base::test::TestFuture<void> future;
    browser_view_->ToggleWindowControlsOverlayEnabled(future.GetCallback());
    EXPECT_TRUE(future.Wait());
    content::TitleWatcher title_watcher(web_contents, u"ongeometrychange");
    std::ignore = title_watcher.WaitAndGetTitle();
  }

  raw_ptr<BrowserView, DanglingUntriaged> browser_view_ = nullptr;
  raw_ptr<OpaqueBrowserFrameView, DanglingUntriaged>
      opaque_browser_frame_view_ = nullptr;
  WebAppFrameToolbarTestHelper web_app_frame_toolbar_helper_;

 private:
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewWindowControlsOverlayTest,
                       CaptionButtonsTooltip) {
  if (!InstallAndLaunchWebAppWithWindowControlsOverlay())
    GTEST_SKIP() << "Skip test if it is not a OpaqueBrowserFrameView";

  auto* minimize_button = static_cast<const views::Button*>(
      opaque_browser_frame_view_->GetViewByID(VIEW_ID_MINIMIZE_BUTTON));
  auto* maximize_button = static_cast<const views::Button*>(
      opaque_browser_frame_view_->GetViewByID(VIEW_ID_MAXIMIZE_BUTTON));
  auto* restore_button = static_cast<const views::Button*>(
      opaque_browser_frame_view_->GetViewByID(VIEW_ID_RESTORE_BUTTON));
  auto* close_button = static_cast<const views::Button*>(
      opaque_browser_frame_view_->GetViewByID(VIEW_ID_CLOSE_BUTTON));

  // Verify tooltip text was first empty.
  EXPECT_EQ(minimize_button->GetTooltipText(), u"");
  EXPECT_EQ(maximize_button->GetTooltipText(), u"");
  EXPECT_EQ(restore_button->GetTooltipText(), u"");
  EXPECT_EQ(close_button->GetTooltipText(), u"");

  ToggleWindowControlsOverlayEnabledAndWait();

  // Verify tooltip text has been updated.
  EXPECT_EQ(minimize_button->GetTooltipText(),
            minimize_button->GetAccessibleName());
  EXPECT_EQ(maximize_button->GetTooltipText(),
            maximize_button->GetAccessibleName());
  EXPECT_EQ(restore_button->GetTooltipText(),
            restore_button->GetAccessibleName());
  EXPECT_EQ(close_button->GetTooltipText(), close_button->GetAccessibleName());

  ToggleWindowControlsOverlayEnabledAndWait();

  // Verify tooltip text has been cleared when the feature is toggled off.
  EXPECT_EQ(minimize_button->GetTooltipText(), u"");
  EXPECT_EQ(maximize_button->GetTooltipText(), u"");
  EXPECT_EQ(restore_button->GetTooltipText(), u"");
  EXPECT_EQ(close_button->GetTooltipText(), u"");
}

IN_PROC_BROWSER_TEST_F(WebAppOpaqueBrowserFrameViewWindowControlsOverlayTest,
                       CaptionButtonHitTest) {
  if (!InstallAndLaunchWebAppWithWindowControlsOverlay())
    GTEST_SKIP() << "Skip test if it is not a OpaqueBrowserFrameView";

  opaque_browser_frame_view_->GetWidget()->LayoutRootViewIfNecessary();

  // Avoid the top right resize corner.
  constexpr int kInset = 10;
  const gfx::Point kPoint(opaque_browser_frame_view_->width() - kInset, kInset);

  EXPECT_EQ(opaque_browser_frame_view_->NonClientHitTest(kPoint), HTCLOSE);

  ToggleWindowControlsOverlayEnabledAndWait();

  // Verify the component updates on toggle.
  EXPECT_EQ(opaque_browser_frame_view_->NonClientHitTest(kPoint), HTCLIENT);

  ToggleWindowControlsOverlayEnabledAndWait();

  // Verify the component clears when the feature is turned off.
  EXPECT_EQ(opaque_browser_frame_view_->NonClientHitTest(kPoint), HTCLOSE);
}
#endif
