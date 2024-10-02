// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/permissions/chip_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/browser/open_tab_provider.h"
#include "components/permissions/features.h"
#include "components/permissions/test/permission_request_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/animation/animation_test_api.h"

namespace {

void RequestPermission(Browser* browser) {
  test::PermissionRequestManagerTestApi test_api(browser);
  permissions::PermissionRequestObserver observer(
      browser->tab_strip_model()->GetActiveWebContents());

  EXPECT_NE(nullptr, test_api.manager());
  test_api.AddSimpleRequest(
      browser->tab_strip_model()->GetActiveWebContents()->GetPrimaryMainFrame(),
      permissions::RequestType::kGeolocation);

  observer.Wait();
}

LocationBarView* GetLocationBarView(Browser* browser) {
  return BrowserView::GetBrowserViewForBrowser(browser)
      ->toolbar()
      ->location_bar();
}

}  // namespace

class PermissionRequestChipGestureSensitiveBrowserTest
    : public InProcessBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures({permissions::features::kPermissionChip},
                                   {});
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PermissionRequestChipGestureSensitiveBrowserTest,
                       ChipFinalizedWhenInteractingWithOmnibox) {
  RequestPermission(browser());
  LocationBarView* lbv = GetLocationBarView(browser());
  auto* animation = lbv->chip_controller()->chip()->animation_for_testing();

  // Animate the chip expand.
  gfx::AnimationTestApi animation_api(animation);
  base::TimeTicks now = base::TimeTicks::Now();
  animation_api.SetStartTime(now);
  animation_api.Step(now + animation->GetSlideDuration());

  // After animation ended, the chip is expanded and the bubble is shown because
  // the gesture sensitive request feature is enabled.
  EXPECT_TRUE(lbv->chip_controller()->IsPermissionPromptChipVisible());
  EXPECT_TRUE(lbv->chip_controller()->IsBubbleShowing());

  // Because the bubble is shown, callback timers should be abandoned
  EXPECT_FALSE(lbv->chip_controller()->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(lbv->chip_controller()->is_dismiss_timer_running_for_testing());

  // Type something in the omnibox.
  auto* omnibox_view = lbv->GetOmniboxView();
  omnibox_view->SetUserText(u"search query");
  omnibox_view->model()->SetInputInProgress(true);

  base::RunLoop().RunUntilIdle();

  // While the user is interacting with the omnibox, the chip is hidden, the
  // location icon isn't offset by the chip and the bubble is hidden.
  EXPECT_FALSE(lbv->chip_controller()->IsPermissionPromptChipVisible());
  EXPECT_FALSE(lbv->chip_controller()->IsBubbleShowing());
  EXPECT_EQ(lbv->location_icon_view()->bounds().x(),
            GetLayoutConstant(LOCATION_BAR_ELEMENT_PADDING));

  // Ensure no callbacks are pending.
  EXPECT_FALSE(lbv->chip_controller()->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(lbv->chip_controller()->is_dismiss_timer_running_for_testing());
}

IN_PROC_BROWSER_TEST_F(PermissionRequestChipGestureSensitiveBrowserTest,
                       ChipIsNotShownWhenInteractingWithOmnibox) {
  LocationBarView* lbv = GetLocationBarView(browser());

  // The chip is not shown because there is no active permission request.
  EXPECT_FALSE(lbv->chip_controller()->IsPermissionPromptChipVisible());

  // Type something in the omnibox.
  auto* omnibox_view = lbv->GetOmniboxView();
  omnibox_view->SetUserText(u"search query");
  omnibox_view->model()->SetInputInProgress(true);

  RequestPermission(browser());

  // While the user is interacting with the omnibox, an incoming permission
  // request will be automatically ignored. The chip is not shown.
  EXPECT_FALSE(lbv->chip_controller()->IsPermissionPromptChipVisible());
}

// This is an end-to-end test that verifies that a permission prompt bubble will
// not be shown because of the empty address bar. Under the normal conditions
// such a test should be placed in PermissionsSecurityModelInteractiveUITest but
// due to dependency issues (see crbug.com/1112591) `//chrome/browser` is not
// allowed to have dependencies on `//chrome/browser/ui/views/*`.
IN_PROC_BROWSER_TEST_F(PermissionRequestChipGestureSensitiveBrowserTest,
                       PermissionRequestIsAutoIgnored) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histograms;

  content::WebContents* embedder_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(embedder_contents);
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  content::WebContents::FromRenderFrameHost(main_rfh)->Focus();

  ASSERT_TRUE(main_rfh);

  constexpr char kCheckMicrophone[] = R"(
    new Promise(async resolve => {
      const PermissionStatus =
        await navigator.permissions.query({name: 'microphone'});
      resolve(PermissionStatus.state === 'granted');
    })
    )";

  constexpr char kRequestMicrophone[] = R"(
    new Promise(async resolve => {
      var constraints = { audio: true };
      window.focus();
      try {
        const stream = await navigator.mediaDevices.getUserMedia(constraints);
        resolve('granted');
      } catch(error) {
        resolve('denied')
      }
    })
    )";

  EXPECT_FALSE(content::EvalJs(main_rfh, kCheckMicrophone,
                               content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                   .value.GetBool());

  LocationBarView* location_bar =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->location_bar();

  // Type something in the omnibox.
  OmniboxView* omnibox_view = location_bar->GetOmniboxView();
  omnibox_view->SetUserText(u"search query");
  omnibox_view->model()->SetInputInProgress(true);

  auto* manager =
      permissions::PermissionRequestManager::FromWebContents(embedder_contents);
  permissions::PermissionRequestObserver observer(embedder_contents);

  EXPECT_FALSE(manager->IsRequestInProgress());

  EXPECT_TRUE(content::ExecJs(
      main_rfh, kRequestMicrophone,
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Wait until a permission request is shown or finalized.
  observer.Wait();

  // Permission request was is in progress without showing a prompt bubble.
  EXPECT_TRUE(manager->IsRequestInProgress());
  EXPECT_FALSE(observer.request_shown());
  EXPECT_TRUE(observer.is_view_recreate_failed());
  EXPECT_FALSE(manager->view_for_testing());

  EXPECT_FALSE(content::EvalJs(main_rfh, kCheckMicrophone,
                               content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1)
                   .value.GetBool());

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.Prompt.AudioCapture.Gesture.Attempt", true, 1);
}

IN_PROC_BROWSER_TEST_F(PermissionRequestChipGestureSensitiveBrowserTest,
                       ShouldUpdateActiverPRMAndObservations) {
  constexpr char kRequestNotifications[] = R"(
    new Promise(resolve => {
      Notification.requestPermission().then(function (permission) {
        resolve(permission)
      });
    })
    )";

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));

  // Setup: open 2 tabs at the same origin
  TabStripModel* tab_strip = browser()->tab_strip_model();
  content::WebContents* embedder_contents_tab_0 =
      tab_strip->GetActiveWebContents();
  ASSERT_TRUE(embedder_contents_tab_0);
  content::RenderFrameHost* rfh_tab_0 =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  content::WebContents::FromRenderFrameHost(rfh_tab_0)->Focus();
  embedder_contents_tab_0 = tab_strip->GetActiveWebContents();
  auto* manager_tab_0 = permissions::PermissionRequestManager::FromWebContents(
      embedder_contents_tab_0);
  permissions::PermissionRequestObserver observer_tab_0(
      embedder_contents_tab_0);

  chrome::NewTabToRight(browser());
  EXPECT_EQ(2, tab_strip->count());

  tab_strip->ActivateTabAt(1);
  content::RenderFrameHost* rfh_tab_1 =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  content::WebContents* embedder_contents_tab_1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* manager_tab_1 = permissions::PermissionRequestManager::FromWebContents(
      embedder_contents_tab_1);
  permissions::PermissionRequestObserver observer_tab_1(
      embedder_contents_tab_1);

  tab_strip->ActivateTabAt(0);

  // Obtain the chip controller instance. The chip controller instance is tied
  // to the location bar view instance. Since the location bar view instance is
  // reused across multiple tabs, this in turn also means that the chip
  // controller instance is the same across multiple tabs.
  LocationBarView* location_bar =
      BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBarView();
  ASSERT_TRUE(location_bar);
  ChipController* chip_controller = location_bar->chip_controller();

  // Trigger permission request on first tab
  EXPECT_FALSE(manager_tab_0->IsRequestInProgress());
  EXPECT_TRUE(content::ExecJs(
      rfh_tab_0, kRequestNotifications,
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  observer_tab_0.Wait();
  EXPECT_TRUE(manager_tab_0->IsRequestInProgress());

  // Close first tab
  tab_strip->CloseWebContentsAt(0, TabCloseTypes::CLOSE_USER_GESTURE);
  EXPECT_FALSE(manager_tab_1->IsRequestInProgress());

  // After closing the first tab, the chip controller should no longer be
  // observing any permission request manager. It should also no longer hold a
  // reference to a Permission Request Manager instance.
  ASSERT_FALSE(chip_controller->permissions::PermissionRequestManager::
                   Observer::IsInObserverList());
  ASSERT_FALSE(chip_controller->active_permission_request_manager_for_testing()
                   .has_value());

  // Trigger a request on the second (the now only remaining) tab.
  EXPECT_TRUE(content::ExecJs(
      rfh_tab_1, kRequestNotifications,
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  observer_tab_1.Wait();

  // During the request, the chip controller should be observing the correct
  // permission request manager instance, and have a reference to the same.
  EXPECT_TRUE(manager_tab_1->IsRequestInProgress());
  ASSERT_TRUE(chip_controller->active_permission_request_manager_for_testing()
                  .has_value());
  ASSERT_EQ(
      chip_controller->active_permission_request_manager_for_testing().value(),
      manager_tab_1);
  ASSERT_TRUE(manager_tab_1->get_observer_list_for_testing()->HasObserver(
      chip_controller));
}

class PermissionRequestChipGestureInsensitiveBrowserTest
    : public InProcessBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures({permissions::features::kPermissionChip},
                                   {});
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PermissionRequestChipGestureInsensitiveBrowserTest,
                       CallbacksResetWhenInteractingWithOmnibox) {
  RequestPermission(browser());
  LocationBarView* lbv = GetLocationBarView(browser());
  auto* animation = lbv->chip_controller()->chip()->animation_for_testing();

  // Animate the chip expand.
  gfx::AnimationTestApi animation_api(animation);
  base::TimeTicks now = base::TimeTicks::Now();
  animation_api.SetStartTime(now);
  animation_api.Step(now + animation->GetSlideDuration());

  // After animation ended, the chip is expanded and a bubble is shown.
  EXPECT_TRUE(lbv->chip_controller()->IsPermissionPromptChipVisible());
  EXPECT_TRUE(lbv->chip_controller()->IsBubbleShowing());

  // Because a bubble is shown, the collapse callback timer should not be
  // running.
  EXPECT_FALSE(lbv->chip_controller()->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(lbv->chip_controller()->is_dismiss_timer_running_for_testing());

  // Type something in the omnibox.
  auto* omnibox_view = lbv->GetOmniboxView();
  omnibox_view->SetUserText(u"search query");
  omnibox_view->model()->SetInputInProgress(true);

  base::RunLoop().RunUntilIdle();

  // Ensure chip is no longer visible and callbacks are no longer running.
  EXPECT_FALSE(lbv->chip_controller()->IsPermissionPromptChipVisible());
  EXPECT_FALSE(lbv->chip_controller()->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(lbv->chip_controller()->is_dismiss_timer_running_for_testing());
}

class PermissionRequestChipDialogBrowserTest : public UiBrowserTest {
 public:
  PermissionRequestChipDialogBrowserTest() {
    feature_list_.InitAndEnableFeature(permissions::features::kPermissionChip);
  }

  PermissionRequestChipDialogBrowserTest(
      const PermissionRequestChipDialogBrowserTest&) = delete;
  PermissionRequestChipDialogBrowserTest& operator=(
      const PermissionRequestChipDialogBrowserTest&) = delete;

  // UiBrowserTest:
  void ShowUi(const std::string& name) override {
    RequestPermission(browser());

    LocationBarView* lbv = GetLocationBarView(browser());
    lbv->GetFocusManager()->ClearFocus();
    lbv->chip_controller()->chip()->SetForceExpandedForTesting(true);
  }

  bool VerifyUi() override {
    LocationBarView* lbv = GetLocationBarView(browser());
    OmniboxChipButton* chip = lbv->chip_controller()->chip();
    if (!chip)
      return false;

// TODO(olesiamrukhno): VerifyPixelUi works only for these platforms, revise
// this if supported platforms change.
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    const std::string screenshot_name =
        base::StrCat({test_info->test_case_name(), "_", test_info->name()});
    return VerifyPixelUi(chip, "BrowserUi", screenshot_name);
#else
    return true;
#endif
  }

  void WaitForUserDismissal() override {
    // Consider closing the browser to be dismissal.
    ui_test_utils::WaitForBrowserToClose();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Temporarily disabled per https://crbug.com/1197280
IN_PROC_BROWSER_TEST_F(PermissionRequestChipDialogBrowserTest,
                       DISABLED_InvokeUi_geolocation) {
  ShowAndVerifyUi();
}
