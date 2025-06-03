// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/cfi_buildflags.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/intent_chip_button.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event_utils.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/any_widget_observer.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/intent_helper/preferred_apps_test_util.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#endif

class IntentChipButtonBrowserTest
    : public web_app::WebAppNavigationBrowserTest {
 public:
  IntentChipButtonBrowserTest() {
    apps::EnableLinkCapturingUXForTesting(scoped_feature_list_);
  }

  void SetUpOnMainThread() override {
    web_app::WebAppNavigationBrowserTest::SetUpOnMainThread();
    InstallTestWebApp();
  }

  void TearDownOnMainThread() override {
    web_app::test::UninstallWebApp(profile(), test_web_app_id());
    if (!overlapping_app_id_.empty()) {
      web_app::test::UninstallWebApp(profile(), overlapping_app_id_);
    }
    web_app::WebAppNavigationBrowserTest::TearDownOnMainThread();
  }

  template <typename Action>
  void DoAndWaitForIntentPickerIconUpdate(Action action) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    base::RunLoop run_loop;
    auto* tab_helper = IntentPickerTabHelper::FromWebContents(web_contents);
    tab_helper->SetIconUpdateCallbackForTesting(run_loop.QuitClosure());
    action();
    run_loop.Run();
  }

  void OpenNewTab(const GURL& url) {
    chrome::NewTab(browser());
    DoAndWaitForIntentPickerIconUpdate(
        [this] { NavigateToLaunchingPage(browser()); });
    ClickLinkAndWaitForIconUpdate(
        browser()->tab_strip_model()->GetActiveWebContents(), url);
  }

  IntentChipButton* GetIntentChip() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetIntentChipButton();
  }

  // Clicks the intent chip, and optionally waits for a browser app window to
  // appear if `wait_for_browser` is true. If waiting is specified, the new
  // browser window is returned; if waiting is not specified, null is returned.
  Browser* ClickIntentChip(bool wait_for_browser) {
    ui_test_utils::BrowserChangeObserver browser_opened(
        nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);

    views::test::ButtonTestApi test_api(GetIntentChip());
    ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), 0, 0);
    test_api.NotifyClick(e);

    if (wait_for_browser) {
      return browser_opened.Wait();
    }

    return nullptr;
  }

  void ClickLinkAndWaitForIconUpdate(content::WebContents* web_contents,
                                     const GURL& link_url) {
    auto* tab_helper = IntentPickerTabHelper::FromWebContents(web_contents);

    base::RunLoop run_loop;
    tab_helper->SetIconUpdateCallbackForTesting(run_loop.QuitClosure());
    ClickLinkAndWait(web_contents, link_url, LinkTarget::SELF, "");
    run_loop.Run();
  }

  // Installs a web app on the same host as InstallTestWebApp(), but with "/" as
  // a scope, so it overlaps with all URLs in the test app scope.
  void InstallOverlappingApp() {
    auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
    const char* app_host = GetAppUrlHost();
    web_app_info->start_url = https_server().GetURL(app_host, "/");
    web_app_info->scope = https_server().GetURL(app_host, "/");
    web_app_info->title = base::UTF8ToUTF16(GetAppName());
    web_app_info->description = u"Test description";
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;

    overlapping_app_id_ =
        web_app::test::InstallWebApp(profile(), std::move(web_app_info));
    DCHECK(!overlapping_app_id_.empty());
    apps::AppReadinessWaiter(profile(), overlapping_app_id_).Await();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  webapps::AppId overlapping_app_id_;
};

IN_PROC_BROWSER_TEST_F(IntentChipButtonBrowserTest,
                       NavigationToInScopeLinkShowsIntentChip) {
  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  DoAndWaitForIntentPickerIconUpdate([this, in_scope_url] {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), in_scope_url));
  });

  EXPECT_TRUE(GetIntentChip()->GetVisible());

// If a single app is installed, then clicking on the intent chip button
// opens the intent picker view on ChromeOS, and directly launches the
// app on other desktop platforms.
#if BUILDFLAG(IS_CHROMEOS)
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "IntentPickerBubbleView");
  ClickIntentChip(/*wait_for_browser=*/false);

  waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(IntentPickerBubbleView::intent_picker_bubble());
#else
  Browser* app_browser = ClickIntentChip(/*wait_for_browser=*/true);
  ASSERT_TRUE(app_browser);
  ASSERT_TRUE(app_browser->is_type_app());
#endif  // BUILDFLAG(IS_CHROMEOS)
}

IN_PROC_BROWSER_TEST_F(IntentChipButtonBrowserTest,
                       NavigationToOutOfScopeLinkDoesNotShowsIntentChip) {
  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), out_of_scope_url));

  EXPECT_FALSE(GetIntentChip()->GetVisible());
}

// TODO(crbug.com/1395393): This test is flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_IconVisibilityAfterTabSwitching \
  DISABLED_IconVisibilityAfterTabSwitching
#else
#define MAYBE_IconVisibilityAfterTabSwitching IconVisibilityAfterTabSwitching
#endif
IN_PROC_BROWSER_TEST_F(IntentChipButtonBrowserTest,
                       MAYBE_IconVisibilityAfterTabSwitching) {
  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());

  OmniboxChipButton* intent_chip_button = GetIntentChip();

  OpenNewTab(in_scope_url);
  EXPECT_TRUE(intent_chip_button->GetVisible());
  OpenNewTab(out_of_scope_url);
  EXPECT_FALSE(intent_chip_button->GetVisible());

  chrome::SelectPreviousTab(browser());
  EXPECT_TRUE(intent_chip_button->GetVisible());
  chrome::SelectNextTab(browser());
  EXPECT_FALSE(intent_chip_button->GetVisible());
}

#if BUILDFLAG(IS_CHROMEOS)
// Using the Intent Chip for an app which is set as preferred should launch
// directly into the app. Preferred apps are only available on ChromeOS.
IN_PROC_BROWSER_TEST_F(IntentChipButtonBrowserTest, OpensAppForPreferredApp) {
  apps_util::SetSupportedLinksPreferenceAndWait(profile(), test_web_app_id());

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  DoAndWaitForIntentPickerIconUpdate([this, in_scope_url] {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), in_scope_url));
  });

  Browser* app_browser = ClickIntentChip(/*wait_for_browser=*/true);

  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser,
                                                         test_web_app_id()));
}

IN_PROC_BROWSER_TEST_F(IntentChipButtonBrowserTest,
                       ShowsIntentChipExpandedForPreferredApp) {
  apps_util::SetSupportedLinksPreferenceAndWait(profile(), test_web_app_id());

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());

  // First three visits will always show as expanded.
  for (int i = 0; i < 3; i++) {
    DoAndWaitForIntentPickerIconUpdate([this, in_scope_url] {
      ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), in_scope_url));
    });
    EXPECT_TRUE(GetIntentChip()->GetVisible());
    EXPECT_FALSE(GetIntentChip()->is_fully_collapsed());

    DoAndWaitForIntentPickerIconUpdate([this, out_of_scope_url] {
      ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), out_of_scope_url));
    });
    EXPECT_FALSE(GetIntentChip()->GetVisible());
  }

  // Fourth visit should show as expanded because the app is set as preferred
  // for this URL.
  DoAndWaitForIntentPickerIconUpdate([this, in_scope_url] {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), in_scope_url));
  });
  EXPECT_TRUE(GetIntentChip()->GetVisible());
  EXPECT_FALSE(GetIntentChip()->is_fully_collapsed());
}

#endif  // BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(IntentChipButtonBrowserTest, ShowsAppIconInChip) {
  // With ChromeRefresh2023, the same icon is always shown in the chip and this
  // test is no longer meaningful.
  if (features::IsChromeRefresh2023()) {
    GTEST_SKIP() << "With ChromeRefresh2023, the same icon is always shown in "
                    "the chip and this test is no longer meaningful.";
  }

  InstallOverlappingApp();

  const GURL root_url = https_server().GetURL(GetAppUrlHost(), "/");
  const GURL overlapped_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  const GURL non_overlapped_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ClickLinkAndWaitForIconUpdate(web_contents, root_url);

  auto icon1 =
      GetIntentChip()->GetImage(views::Button::ButtonState::STATE_NORMAL);
  ASSERT_FALSE(IntentPickerTabHelper::FromWebContents(web_contents)
                   ->app_icon()
                   .IsEmpty());

  ClickLinkAndWaitForIconUpdate(web_contents, non_overlapped_url);

  // The chip should still be showing the same app icon.
  auto icon2 =
      GetIntentChip()->GetImage(views::Button::ButtonState::STATE_NORMAL);
  ASSERT_TRUE(icon1.BackedBySameObjectAs(icon2));

  ClickLinkAndWaitForIconUpdate(web_contents, overlapped_url);

  // Loading a URL with multiple apps available should switch to a generic icon.
  auto icon3 =
      GetIntentChip()->GetImage(views::Button::ButtonState::STATE_NORMAL);
  ASSERT_FALSE(icon1.BackedBySameObjectAs(icon3));
  ASSERT_TRUE(IntentPickerTabHelper::FromWebContents(web_contents)
                  ->app_icon()
                  .IsEmpty());
}

class IntentChipButtonBrowserUiTest : public UiBrowserTest {
 public:
  IntentChipButtonBrowserUiTest() {
    apps::EnableLinkCapturingUXForTesting(scoped_feature_list_);
  }

  // UiBrowserTest:
  void ShowUi(const std::string& name) override {
    auto* const web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    auto* const tab_helper =
        IntentPickerTabHelper::FromWebContents(web_contents);
    base::RunLoop run_loop;
    tab_helper->SetIconUpdateCallbackForTesting(run_loop.QuitClosure());
    tab_helper->MaybeShowIconForApps(
        {{apps::PickerEntryType::kWeb, ui::ImageModel(), "app_id",
          "Test app"}});
    run_loop.Run();
  }

  bool VerifyUi() override {
    auto* const location_bar =
        BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBarView();
    const auto* const intent_chip = location_bar->intent_chip();
    if (!intent_chip || !intent_chip->GetVisible() ||
        intent_chip->is_fully_collapsed()) {
      return false;
    }

    const auto* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    return VerifyPixelUi(location_bar, test_info->test_case_name(),
                         test_info->name()) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {
    // Consider closing the browser to be dismissal.
    ui_test_utils::WaitForBrowserToClose();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(IntentChipButtonBrowserUiTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
