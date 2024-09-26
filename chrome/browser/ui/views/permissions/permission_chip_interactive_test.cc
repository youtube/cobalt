// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/omnibox_chip_theme.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "chrome/browser/ui/views/permissions/chip_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/permissions/features.h"
#include "components/permissions/origin_keyed_permission_action_service.h"
#include "components/permissions/permission_ui_selector.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/permissions/test/permission_request_observer.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/permissions_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "permission_prompt_chip.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/test_event.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/button_test_api.h"

namespace {

enum ChipFeatureConfig {
  REQUEST_CHIP,
  REQUEST_CHIP_LOCATION_BAR_ICON_OVERRIDE,
  REQUEST_AND_CONFIRMATION_CHIP,
  REQUEST_AND_CONFIRMATION_CHIP_LOCATION_BAR_ICON_OVERRIDE
};

constexpr char kAddNotificationsEventListener[] = R"(
    new Promise(async resolve => {
      const PermissionStatus =
        await navigator.permissions.query({name: 'notifications'});
        PermissionStatus.onchange = () => {};
      resolve(true);
    })
    )";

constexpr char kCheckNotifications[] = R"(
    new Promise(async resolve => {
      const PermissionStatus =
        await navigator.permissions.query({name: 'notifications'});
      resolve(PermissionStatus.state === 'granted');
    })
    )";

constexpr char kRequestNotifications[] = R"(
      new Promise(resolve => {
        Notification.requestPermission().then(function (permission) {
          resolve(permission)
        });
      })
      )";

// Test implementation of PermissionUiSelector that always returns a canned
// decision.
class TestQuietNotificationPermissionUiSelector
    : public permissions::PermissionUiSelector {
 public:
  explicit TestQuietNotificationPermissionUiSelector(
      const Decision& canned_decision)
      : canned_decision_(canned_decision) {}
  ~TestQuietNotificationPermissionUiSelector() override = default;

 protected:
  // permissions::PermissionUiSelector:
  void SelectUiToUse(permissions::PermissionRequest* request,
                     DecisionMadeCallback callback) override {
    std::move(callback).Run(canned_decision_);
  }

  bool IsPermissionRequestSupported(
      permissions::RequestType request_type) override {
    return request_type == permissions::RequestType::kNotifications;
  }

 private:
  Decision canned_decision_;
};

class ChipExpansionObserver : OmniboxChipButton::Observer {
 public:
  explicit ChipExpansionObserver(OmniboxChipButton* chip) {
    observation_.Observe(chip);
  }

  void WaitForChipToExpand() { loop_.Run(); }

  void OnExpandAnimationEnded() override { loop_.Quit(); }

  base::ScopedObservation<OmniboxChipButton, OmniboxChipButton::Observer>
      observation_{this};
  base::RunLoop loop_;
};

}  // namespace

class PermissionChipInteractiveTest : public InProcessBrowserTest {
 public:
  PermissionChipInteractiveTest() {
    scoped_feature_list_.InitWithFeatures(
        {permissions::features::kPermissionChip}, {});
  }

  PermissionChipInteractiveTest(const PermissionChipInteractiveTest&) = delete;
  PermissionChipInteractiveTest& operator=(
      const PermissionChipInteractiveTest&) = delete;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    test_api_ =
        std::make_unique<test::PermissionRequestManagerTestApi>(browser());
  }

  content::RenderFrameHost* GetActiveMainFrame() {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetPrimaryMainFrame();
  }

  void RequestPermission(permissions::RequestType type) {
    GURL requesting_origin("https://example.com");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), requesting_origin));
    test_api_->AddSimpleRequest(GetActiveMainFrame(), type);
    base::RunLoop().RunUntilIdle();
  }

  LocationBarView* GetLocationBarView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->toolbar()->location_bar();
  }

  OmniboxChipButton* GetChip() {
    return GetLocationBarView()->chip_controller()->chip();
  }

  ChipController* GetChipController() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    LocationBarView* lbv = browser_view->toolbar()->location_bar();

    return lbv->chip_controller();
  }

  PermissionPromptChip* GetPermissionPromptChip() {
    return static_cast<PermissionPromptChip*>(
        test_api_->manager()->view_for_testing());
  }

  void ClickOnChip(OmniboxChipButton* chip) {
    ASSERT_TRUE(chip != nullptr);
    ASSERT_TRUE(chip->GetVisible());
    ASSERT_FALSE(GetChipController()->GetBubbleWidget());

    views::test::ButtonTestApi(chip).NotifyClick(
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    base::RunLoop().RunUntilIdle();
  }

  void ClickOnLock() {
    views::test::ButtonTestApi(GetLocationBarView()->location_icon_view())
        .NotifyClick(ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                    gfx::Point(), ui::EventTimeForNow(),
                                    ui::EF_LEFT_MOUSE_BUTTON, 0));
    base::RunLoop().RunUntilIdle();
  }

  void ExpectQuietAbusiveChip() {
    // PermissionChip lifetime is bound to a permission prompt view.
    ASSERT_TRUE(test_api_->manager()->view_for_testing());

    // The quiet chip will be shown even if the chip experiment is disabled.
    OmniboxChipButton* chip_ = GetChip();
    ASSERT_TRUE(chip_);

    EXPECT_FALSE(GetPermissionPromptChip()
                     ->get_chip_controller_for_testing()
                     ->should_expand_for_testing());
    EXPECT_FALSE(chip_->is_animating());
    EXPECT_EQ(OmniboxChipTheme::kLowVisibility, chip_->get_theme_for_testing());
  }

  void ExpectQuietChip() {
    // PermissionChip lifetime is bound to a permission prompt view.
    ASSERT_TRUE(test_api_->manager()->view_for_testing());

    // The quiet chip will be shown even if the chip experiment is disabled.
    OmniboxChipButton* chip_ = GetChip();
    ASSERT_TRUE(chip_);

    EXPECT_TRUE(GetPermissionPromptChip()
                    ->get_chip_controller_for_testing()
                    ->should_expand_for_testing());
    EXPECT_TRUE(chip_->is_animating());
    EXPECT_EQ(OmniboxChipTheme::kLowVisibility, chip_->get_theme_for_testing());
  }

  void ExpectNormalChip() {
    // PermissionChip lifetime is bound to a permission prompt view.
    ASSERT_TRUE(test_api_->manager()->view_for_testing());
    OmniboxChipButton* chip_ = GetChip();
    ASSERT_TRUE(chip_);

    EXPECT_TRUE(GetPermissionPromptChip()
                    ->get_chip_controller_for_testing()
                    ->should_expand_for_testing());
    EXPECT_TRUE(chip_->is_animating());
    // TODO(crbug.com/1232460): Verify that OmniboxChipButton::is_animating is
    // true. Right now the value is flaky.
    EXPECT_EQ(OmniboxChipTheme::kNormalVisibility,
              chip_->get_theme_for_testing());
  }

  ContentSettingImageView& GetContentSettingImageView(
      ContentSettingImageModel::ImageType image_type) {
    LocationBarView* location_bar_view =
        BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBarView();
    return **base::ranges::find(
        location_bar_view->GetContentSettingViewsForTest(), image_type,
        &ContentSettingImageView::GetTypeForTesting);
  }

  // Create an <iframe> inside |parent_rfh|, and navigate it toward |url|.
  // |permission_policy| can be used to set permission policy to the iframe.
  // For instance:
  // ```
  // child = CreateIframe(parent, url, "geolocation *; camera *");
  // ```
  // This returns the new RenderFrameHost associated with new document created
  // in the iframe.
  content::RenderFrameHost* CreateIframe(
      content::RenderFrameHost* parent_rfh,
      const GURL& url,
      const std::string& permission_policy = "") {
    std::string script = content::JsReplace(R"(
    new Promise((resolve) => {
      const iframe = document.createElement("iframe");
      iframe.src = $1;
      iframe.allow = $2;
      iframe.onload = _ => { resolve("iframe loaded"); };
      document.body.appendChild(iframe);
    }))",
                                            url, permission_policy);

    EXPECT_EQ("iframe loaded", content::EvalJs(parent_rfh, script));

    return ChildFrameAt(parent_rfh, 0);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<test::PermissionRequestManagerTestApi> test_api_;
};

class LocationBarIconOverrideTest
    : public PermissionChipInteractiveTest,
      public ::testing::WithParamInterface<ChipFeatureConfig> {
 public:
  LocationBarIconOverrideTest() {
    std::vector<base::test::FeatureRef> disabled_features = {};

    switch (GetParam()) {
      case REQUEST_CHIP:
        scoped_feature_list_.InitWithFeatures(
            {permissions::features::kPermissionChip}, disabled_features);
        break;
      case REQUEST_CHIP_LOCATION_BAR_ICON_OVERRIDE:
        scoped_feature_list_.InitWithFeatures(
            {permissions::features::kPermissionChip,
             permissions::features::kChipLocationBarIconOverride},
            disabled_features);
        break;
      case REQUEST_AND_CONFIRMATION_CHIP:
        scoped_feature_list_.InitWithFeatures(
            {permissions::features::kPermissionChip,
             permissions::features::kConfirmationChip},
            disabled_features);
        break;
      case REQUEST_AND_CONFIRMATION_CHIP_LOCATION_BAR_ICON_OVERRIDE:
        scoped_feature_list_.InitWithFeatures(
            {permissions::features::kPermissionChip,
             permissions::features::kConfirmationChip,
             permissions::features::kChipLocationBarIconOverride},
            disabled_features);
        break;
    }
  }

  bool IsLocationIconVisible() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->GetLocationBarView()
        ->location_icon_view()
        ->GetVisible();
  }

  bool IsTestWithOverridenLocationBarIcon() {
    return base::FeatureList::IsEnabled(
        permissions::features::kChipLocationBarIconOverride);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(LocationBarIconOverrideTest,
                       OverrideLocationBarIconDuringChipOnlyForOverrideFlags) {
  // Initially the location bar icon should be visible for any feature flag
  // configuration
  EXPECT_TRUE(IsLocationIconVisible());

  RequestPermission(permissions::RequestType::kGeolocation);

  // After a request, a chip is shown, which should override the lock icon for
  // feature flags featuring this.
  if (IsTestWithOverridenLocationBarIcon()) {
    EXPECT_FALSE(IsLocationIconVisible());
  } else {
    EXPECT_TRUE(IsLocationIconVisible());
  }

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

  test_api_->manager()->Accept();

  base::RunLoop().RunUntilIdle();

  // Force synchronous update of layout values. In the actual code,
  // InvalidateLayout() is sufficient, but leaves stale visibility values for
  // testing.
  BrowserView::GetBrowserViewForBrowser(browser())
      ->GetLocationBarView()
      ->Layout();

  if (base::FeatureList::IsEnabled(permissions::features::kConfirmationChip)) {
    // Test with confirmation chip.
    // Verify chip is still visible and has the confirmation text
    EXPECT_TRUE(GetChip()->GetVisible());
    EXPECT_TRUE(GetChip()->GetText() ==
                l10n_util::GetStringUTF16(
                    IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION));

    if (IsTestWithOverridenLocationBarIcon()) {
      EXPECT_FALSE(IsLocationIconVisible());
    } else {
      EXPECT_TRUE(IsLocationIconVisible());
    }

    // Check collapse timer is running and fast forward fire callback. Then,
    // fast forward animation to trigger callback and wait until it completes.
    EXPECT_TRUE(GetChipController()->is_collapse_timer_running_for_testing());
    GetChipController()->fire_collapse_timer_for_testing();
    GetChip()->animation_for_testing()->End();
    base::RunLoop().RunUntilIdle();

    // Force synchronous update of layout values. In the actual code,
    // InvalidateLayout() is sufficient, but leaves stale visibility values for
    // testing.
    BrowserView::GetBrowserViewForBrowser(browser())
        ->GetLocationBarView()
        ->Layout();
  }

  // With any feature flag configuration, we have to ensure that the location
  // bar icon is visible after the chip collapsed.
  EXPECT_FALSE(GetChip()->GetVisible());
  EXPECT_TRUE(IsLocationIconVisible());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    LocationBarIconOverrideTest,
    ::testing::Values(
        REQUEST_CHIP,
        REQUEST_CHIP_LOCATION_BAR_ICON_OVERRIDE,
        REQUEST_AND_CONFIRMATION_CHIP,
        REQUEST_AND_CONFIRMATION_CHIP_LOCATION_BAR_ICON_OVERRIDE));

class ConfirmationChipEnabledInteractiveTest
    : public PermissionChipInteractiveTest,
      public ::testing::WithParamInterface<ChipFeatureConfig> {
 public:
  ConfirmationChipEnabledInteractiveTest() {
    std::vector<base::test::FeatureRef> disabled_features = {};
    switch (GetParam()) {
      case REQUEST_AND_CONFIRMATION_CHIP:
        scoped_feature_list_.InitWithFeatures(
            {permissions::features::kPermissionChip,
             permissions::features::kConfirmationChip},
            disabled_features);
        break;
      case REQUEST_AND_CONFIRMATION_CHIP_LOCATION_BAR_ICON_OVERRIDE:
        scoped_feature_list_.InitWithFeatures(
            {permissions::features::kPermissionChip,
             permissions::features::kConfirmationChip,
             permissions::features::kChipLocationBarIconOverride},
            disabled_features);
        break;
      default:
        NOTREACHED_NORETURN();
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ConfirmationChipEnabledInteractiveTest,
                       ShouldDisplayAllowAndDenyConfirmationCorrectly) {
  RequestPermission(permissions::RequestType::kGeolocation);
  base::RunLoop().RunUntilIdle();

  // Chip should be visible and show geolocation request
  EXPECT_TRUE(GetChip()->GetVisible());
  EXPECT_TRUE(GetChip()->GetText() ==
              l10n_util::GetStringUTF16(IDS_GEOLOCATION_PERMISSION_CHIP));

  test_api_->manager()->Accept();

  // Confirmation chip should be visible
  EXPECT_TRUE(GetChip()->GetVisible());
  EXPECT_TRUE(GetChip()->GetText() ==
              l10n_util::GetStringUTF16(
                  IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION));
  EXPECT_EQ(GetChip()->get_theme_for_testing(),
            OmniboxChipTheme::kNormalVisibility);

  // Check collapse timer is running and fast forward fire callback. Then,
  // fast forward animation to trigger callback and wait until it completes.
  EXPECT_TRUE(GetChipController()->is_collapse_timer_running_for_testing());
  GetChipController()->fire_collapse_timer_for_testing();
  GetChip()->animation_for_testing()->End();
  base::RunLoop().RunUntilIdle();

  // Chip should no longer be visible.
  EXPECT_FALSE(GetChip()->GetVisible());

  // Request second permission
  RequestPermission(permissions::RequestType::kNotifications);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(GetChip()->GetVisible());
  EXPECT_EQ(GetChip()->GetText(),
            l10n_util::GetStringUTF16(IDS_NOTIFICATION_PERMISSIONS_CHIP));

  test_api_->manager()->Deny();

  // After deny, the deny confirmation should be displayed
  EXPECT_TRUE(GetChip()->GetVisible());
  EXPECT_EQ(GetChip()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION));
  EXPECT_EQ(GetChip()->get_theme_for_testing(),
            OmniboxChipTheme::kLowVisibility);
}

IN_PROC_BROWSER_TEST_P(ConfirmationChipEnabledInteractiveTest,
                       IncomingRequestShouldOverrideConfirmation) {
  RequestPermission(permissions::RequestType::kGeolocation);
  base::RunLoop().RunUntilIdle();

  test_api_->manager()->Accept();

  RequestPermission(permissions::RequestType::kNotifications);
  base::RunLoop().RunUntilIdle();

  // Since a new request came in, the new request should be displayed
  EXPECT_TRUE(GetChip()->GetVisible());
  EXPECT_EQ(GetChip()->GetText(),
            l10n_util::GetStringUTF16(IDS_NOTIFICATION_PERMISSIONS_CHIP));

  test_api_->manager()->Deny();

  // After the deny, the deny confirmation should be displayed
  EXPECT_TRUE(GetChip()->GetVisible());
  EXPECT_EQ(GetChip()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION));
}

IN_PROC_BROWSER_TEST_P(ConfirmationChipEnabledInteractiveTest,
                       ClickOnConfirmationChipShouldOpenPageInfoDialog) {
  RequestPermission(permissions::RequestType::kGeolocation);
  base::RunLoop().RunUntilIdle();

  test_api_->manager()->Accept();

  ClickOnChip(GetChip());

  base::RunLoop().RunUntilIdle();
  views::View* bubble_view =
      GetChipController()->get_prompt_bubble_view_for_testing();
  PageInfoBubbleView* page_info_bubble =
      static_cast<PageInfoBubbleView*>(bubble_view);
  ASSERT_NE(page_info_bubble, nullptr);

  // Ensure closing the bubble works, and that this will start the collapse
  // animation of the chip.
  page_info_bubble->CloseBubble();

  // Fast forward animation to trigger callback and wait until it completes.
  GetChip()->animation_for_testing()->End();
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(GetChip()->GetVisible());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ConfirmationChipEnabledInteractiveTest,
    ::testing::Values(
        REQUEST_AND_CONFIRMATION_CHIP,
        REQUEST_AND_CONFIRMATION_CHIP_LOCATION_BAR_ICON_OVERRIDE));

class ConfirmationChipUmaInteractiveTest
    : public PermissionChipInteractiveTest {
 public:
  ConfirmationChipUmaInteractiveTest() {
    scoped_feature_list_.InitWithFeatures(
        {permissions::features::kConfirmationChip}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ConfirmationChipUmaInteractiveTest, VerifyUmaMetrics) {
  base::HistogramTester histograms;

  ClickOnLock();

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.ConfirmationChip.PageInfoDialogAccessType",
      static_cast<int>(permissions::PageInfoDialogAccessType::LOCK_CLICK), 1);

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                              false, false, false));
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kGeolocation);
  test_api_->manager()->Accept();

  ClickOnChip(GetChip());

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.ConfirmationChip.PageInfoDialogAccessType",
      static_cast<int>(
          permissions::PageInfoDialogAccessType::CONFIRMATION_CHIP_CLICK),
      1);

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                              false, false, false));

  base::RunLoop().RunUntilIdle();

  GetLocationBarView()->SetConfirmationChipShownTimeForTesting(
      base::TimeTicks::Now() - base::Seconds(10));

  ClickOnLock();

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.ConfirmationChip.PageInfoDialogAccessType",
      static_cast<int>(permissions::PageInfoDialogAccessType::
                           LOCK_CLICK_SHORTLY_AFTER_CONFIRMATION_CHIP),
      1);

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                              false, false, false));

  base::RunLoop().RunUntilIdle();

  GetLocationBarView()->SetConfirmationChipShownTimeForTesting(
      base::TimeTicks::Now() - base::Seconds(21));

  ClickOnLock();

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.ConfirmationChip.PageInfoDialogAccessType",
      static_cast<int>(permissions::PageInfoDialogAccessType::LOCK_CLICK), 2);
}

class PageInfoChangedWithin1mUmaTest : public PermissionChipInteractiveTest {
 public:
  PageInfoChangedWithin1mUmaTest() {
    scoped_feature_list_.InitWithFeatures(
        {permissions::features::kConfirmationChip}, {});
  }

  void InitAndRequestNotification() {
    ASSERT_TRUE(embedded_test_server()->Start());
    url_ = (embedded_test_server()->GetURL("/empty.html"));
    content::RenderFrameHost* main_rfh =
        ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                                  url_, 1);
    content::WebContents::FromRenderFrameHost(main_rfh)->Focus();
    content::WebContents* embedder_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(embedder_contents);

    permissions::PermissionRequestObserver observer(embedder_contents);

    EXPECT_TRUE(content::ExecJs(
        main_rfh, kRequestNotifications,
        content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

    observer.Wait();
  }

  PageInfoBubbleView* OpenAndGetPageInfoBubbleView() {
    base::RunLoop run_loop;
    GetPageInfoDialogCreatedCallbackForTesting() = run_loop.QuitClosure();
    OpenPageInfoBubble(browser());
    run_loop.Run();
    auto* bubble_view = static_cast<PageInfoBubbleView*>(
        PageInfoBubbleView::GetPageInfoBubbleForTesting());
    EXPECT_TRUE(bubble_view);

    return bubble_view;
  }

  void OpenPageInfoAndTogglePermission() {
    PageInfoBubbleView* page_info = OpenAndGetPageInfoBubbleView();
    views::View* permisison_toggle =
        GetViewWithinPageInfo(
            page_info, PageInfoViewFactory::VIEW_ID_PAGE_INFO_PERMISSION_VIEW)
            ->GetViewByID(PageInfoViewFactory::
                              VIEW_ID_PERMISSION_TOGGLE_ROW_TOGGLE_BUTTON);

    ASSERT_TRUE(permisison_toggle);

    PerformMouseClickOnView(
        static_cast<views::ToggleButton*>(permisison_toggle));
  }

  void OpenPageInfoAndClickReset() {
    PageInfoBubbleView* page_info = OpenAndGetPageInfoBubbleView();
    views::View* reset_permissions_view = GetViewWithinPageInfo(
        page_info,
        PageInfoViewFactory::VIEW_ID_PAGE_INFO_RESET_PERMISSIONS_BUTTON);

    PerformMouseClickOnView(
        static_cast<views::MdTextButton*>(reset_permissions_view));
  }

 private:
  void OpenPageInfoBubble(Browser* browser) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    LocationIconView* location_icon_view =
        browser_view->toolbar()->location_bar()->location_icon_view();
    ASSERT_TRUE(location_icon_view);
    ui::test::TestEvent event;
    location_icon_view->ShowBubble(event);
    views::BubbleDialogDelegateView* page_info =
        PageInfoBubbleView::GetPageInfoBubbleForTesting();
    EXPECT_NE(nullptr, page_info);
    page_info->set_close_on_deactivate(false);
  }

  views::View* GetViewWithinPageInfo(PageInfoBubbleView* page_info_bubble,
                                     int view_id) {
    views::View* view = page_info_bubble->GetViewByID(view_id);
    EXPECT_TRUE(view);
    return view;
  }

  void PerformMouseClickOnView(views::Button* button) {
    views::test::ButtonTestApi(button).NotifyClick(
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  GURL url_;
};

IN_PROC_BROWSER_TEST_F(PageInfoChangedWithin1mUmaTest,
                       VerifyResetFromAllowedUmaMetric) {
  InitAndRequestNotification();
  base::HistogramTester histograms;

  test_api_->manager()->Accept();

  OpenPageInfoAndClickReset();

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.PageInfo.ChangedWithin1m.Notifications",
      static_cast<int>(permissions::PermissionChangeAction::RESET_FROM_ALLOWED),
      1);
}

IN_PROC_BROWSER_TEST_F(PageInfoChangedWithin1mUmaTest,
                       VerifyResetFromDeniedUmaMetric) {
  InitAndRequestNotification();
  base::HistogramTester histograms;

  test_api_->manager()->Deny();

  OpenPageInfoAndClickReset();

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.PageInfo.ChangedWithin1m.Notifications",
      static_cast<int>(permissions::PermissionChangeAction::RESET_FROM_DENIED),
      1);
}

IN_PROC_BROWSER_TEST_F(PageInfoChangedWithin1mUmaTest, VerifyRevokedUmaMetric) {
  InitAndRequestNotification();
  base::HistogramTester histograms;

  test_api_->manager()->Accept();

  OpenPageInfoAndTogglePermission();

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.PageInfo.ChangedWithin1m.Notifications",
      static_cast<int>(permissions::PermissionChangeAction::REVOKED), 1);
}

IN_PROC_BROWSER_TEST_F(PageInfoChangedWithin1mUmaTest, VerifyReallowUmaMetric) {
  InitAndRequestNotification();
  base::HistogramTester histograms;

  test_api_->manager()->Deny();

  OpenPageInfoAndTogglePermission();

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.PageInfo.ChangedWithin1m.Notifications",
      static_cast<int>(permissions::PermissionChangeAction::REALLOWED), 1);
}

IN_PROC_BROWSER_TEST_F(PageInfoChangedWithin1mUmaTest,
                       VerifyNoReallowFromDenyRecordedAfter2mUmaMetric) {
  InitAndRequestNotification();
  base::HistogramTester histograms;

  test_api_->manager()->Deny();

  content::WebContents* web_contents = GetLocationBarView()->GetWebContents();
  const GURL& origin = permissions::PermissionUtil::GetLastCommittedOriginAsURL(
      web_contents->GetPrimaryMainFrame());
  permissions::OriginKeyedPermissionActionService* permission_action_service =
      permissions::PermissionsClient::Get()
          ->GetOriginKeyedPermissionActionService(
              GetLocationBarView()->GetWebContents()->GetBrowserContext());

  // Get recorded entry and manually change its time to 2 minutes ago.
  absl::optional<permissions::PermissionActionTime> record =
      permission_action_service->GetLastActionEntry(
          origin, ContentSettingsType::NOTIFICATIONS);
  EXPECT_TRUE(record.has_value());
  permission_action_service->RecordActionWithTimeForTesting(
      origin, ContentSettingsType::NOTIFICATIONS, record->first,
      record->second - base::Minutes(2));

  OpenPageInfoAndTogglePermission();

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.PageInfo.ChangedWithin1m.Notifications",
      static_cast<int>(permissions::PermissionChangeAction::REALLOWED), 0);
}

class ChipGestureSensitiveEnabledInteractiveTest
    : public PermissionChipInteractiveTest {
 public:
  ChipGestureSensitiveEnabledInteractiveTest() {}
};
IN_PROC_BROWSER_TEST_F(ChipGestureSensitiveEnabledInteractiveTest,
                       ChipAutoPopupBubbleEnabled) {
  RequestPermission(permissions::RequestType::kGeolocation);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kNotifications);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kMidiSysex);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);
}

class ChipRequestTypeSensitiveEnabledInteractiveTest
    : public PermissionChipInteractiveTest {
 public:
  ChipRequestTypeSensitiveEnabledInteractiveTest() {}
};

class ChipDisabledInteractiveTest : public PermissionChipInteractiveTest {
 public:
  ChipDisabledInteractiveTest() {
    scoped_feature_list_.InitWithFeatures(
        {}, {permissions::features::kPermissionChip});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChipDisabledInteractiveTest,
                       ChipAutoPopupBubbleEnabled) {
  ASSERT_FALSE(
      base::FeatureList::IsEnabled(permissions::features::kPermissionChip));

  RequestPermission(permissions::RequestType::kGeolocation);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kNotifications);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kMidiSysex);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);
}

class QuietChipAutoPopupBubbleInteractiveTest
    : public PermissionChipInteractiveTest {
 public:
  QuietChipAutoPopupBubbleInteractiveTest() {
    scoped_feature_list_.InitWithFeatures(
        {permissions::features::kPermissionChip,
         features::kQuietNotificationPrompts,
         permissions::features::kPermissionQuietChip},
        {});
  }

 protected:
  using QuietUiReason = permissions::PermissionUiSelector::QuietUiReason;
  using WarningReason = permissions::PermissionUiSelector::WarningReason;

  void SetCannedUiDecision(absl::optional<QuietUiReason> quiet_ui_reason,
                           absl::optional<WarningReason> warning_reason) {
    test_api_->manager()->set_permission_ui_selector_for_testing(
        std::make_unique<TestQuietNotificationPermissionUiSelector>(
            permissions::PermissionUiSelector::Decision(quiet_ui_reason,
                                                        warning_reason)));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(QuietChipAutoPopupBubbleInteractiveTest,
                       IgnoreChipHistogramsTest) {
  base::HistogramTester histograms;

  RequestPermission(permissions::RequestType::kGeolocation);

  ASSERT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

  test_api_->manager()->Ignore();

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.Prompt.Geolocation.LocationBarLeftChipAutoBubble.Action",
      static_cast<int>(permissions::PermissionAction::IGNORED), 1);
}

IN_PROC_BROWSER_TEST_F(QuietChipAutoPopupBubbleInteractiveTest,
                       GrantedChipHistogramsTest) {
  base::HistogramTester histograms;

  RequestPermission(permissions::RequestType::kGeolocation);

  ASSERT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

  test_api_->manager()->Accept();

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.Prompt.Geolocation.LocationBarLeftChipAutoBubble.Action",
      static_cast<int>(permissions::PermissionAction::GRANTED), 1);
}

IN_PROC_BROWSER_TEST_F(QuietChipAutoPopupBubbleInteractiveTest,
                       DeniedChipHistogramsTest) {
  base::HistogramTester histograms;

  RequestPermission(permissions::RequestType::kGeolocation);

  ASSERT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

  test_api_->manager()->Deny();

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.Prompt.Geolocation.LocationBarLeftChipAutoBubble.Action",
      static_cast<int>(permissions::PermissionAction::DENIED), 1);
}

IN_PROC_BROWSER_TEST_F(QuietChipAutoPopupBubbleInteractiveTest,
                       DismissedChipHistogramsTest) {
  base::HistogramTester histograms;

  RequestPermission(permissions::RequestType::kGeolocation);

  ASSERT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);
  base::TimeDelta duration = base::Milliseconds(42);
  test_api_->manager()->set_time_to_decision_for_test(duration);

  test_api_->manager()->Dismiss();

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.Prompt.Geolocation.LocationBarLeftChipAutoBubble.Action",
      static_cast<int>(permissions::PermissionAction::DISMISSED), 1);

  histograms.ExpectTimeBucketCount(
      "Permissions.Prompt.Geolocation.LocationBarLeftChipAutoBubble.Dismissed."
      "TimeToAction",
      duration, 1);
}

IN_PROC_BROWSER_TEST_F(QuietChipAutoPopupBubbleInteractiveTest,
                       QuietChipNonAbusiveUmaTest) {
  base::HistogramTester histograms;

  for (QuietUiReason reason :
       {QuietUiReason::kEnabledInPrefs,
        QuietUiReason::kServicePredictedVeryUnlikelyGrant}) {
    SetCannedUiDecision(reason, absl::nullopt);

    RequestPermission(permissions::RequestType::kNotifications);

    ASSERT_EQ(
        test_api_->manager()->current_request_prompt_disposition_for_testing(),
        permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_CHIP);

    ClickOnChip(GetChip());

    test_api_->manager()->Ignore();
    base::RunLoop().RunUntilIdle();
  }

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.Prompt.Notifications.LocationBarLeftQuietChip.Ignored."
      "DidShowBubble",
      static_cast<int>(true), 2);

  histograms.ExpectBucketCount(
      "Permissions.Prompt.Notifications.LocationBarLeftQuietChip.Ignored."
      "DidClickManage",
      static_cast<int>(false), 2);
}

IN_PROC_BROWSER_TEST_F(QuietChipAutoPopupBubbleInteractiveTest,
                       QuietChipNonAbusiveClickManageUmaTest) {
  base::HistogramTester histograms;

  for (QuietUiReason reason :
       {QuietUiReason::kEnabledInPrefs,
        QuietUiReason::kServicePredictedVeryUnlikelyGrant}) {
    SetCannedUiDecision(reason, absl::nullopt);

    RequestPermission(permissions::RequestType::kNotifications);

    ASSERT_EQ(
        test_api_->manager()->current_request_prompt_disposition_for_testing(),
        permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_CHIP);

    ClickOnChip(GetChip());

    views::View* bubble_view =
        GetChipController()->get_prompt_bubble_view_for_testing();
    ContentSettingBubbleContents* permission_prompt_bubble =
        static_cast<ContentSettingBubbleContents*>(bubble_view);

    ASSERT_TRUE(permission_prompt_bubble != nullptr);

    permission_prompt_bubble->managed_button_clicked_for_test();

    test_api_->manager()->Ignore();
    base::RunLoop().RunUntilIdle();
  }

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.Prompt.Notifications.LocationBarLeftQuietChip.Ignored."
      "DidShowBubble",
      static_cast<int>(true), 2);

  histograms.ExpectBucketCount(
      "Permissions.Prompt.Notifications.LocationBarLeftQuietChip.Ignored."
      "DidClickManage",
      static_cast<int>(true), 2);
}

IN_PROC_BROWSER_TEST_F(QuietChipAutoPopupBubbleInteractiveTest,
                       PermissionIgnoredQuietChipAbusiveUmaTest) {
  base::HistogramTester histograms;

  for (QuietUiReason reason : {QuietUiReason::kTriggeredByCrowdDeny,
                               QuietUiReason::kTriggeredDueToAbusiveRequests,
                               QuietUiReason::kTriggeredDueToAbusiveContent}) {
    SetCannedUiDecision(reason, absl::nullopt);

    RequestPermission(permissions::RequestType::kNotifications);

    ASSERT_EQ(
        test_api_->manager()->current_request_prompt_disposition_for_testing(),
        permissions::PermissionPromptDisposition::
            LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP);

    ClickOnChip(GetChip());

    test_api_->manager()->Ignore();
    base::RunLoop().RunUntilIdle();
  }

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.Prompt.Notifications.LocationBarLeftQuietAbusiveChip."
      "Ignored."
      "DidShowBubble",
      static_cast<int>(true), 3);

  histograms.ExpectBucketCount(
      "Permissions.Prompt.Notifications.LocationBarLeftQuietAbusiveChip."
      "Ignored.DidClickLearnMore",
      static_cast<int>(false), 3);
}

IN_PROC_BROWSER_TEST_F(QuietChipAutoPopupBubbleInteractiveTest,
                       PermissionGrantedQuietChipAbusiveUmaTest) {
  base::HistogramTester histograms;

  for (QuietUiReason reason : {QuietUiReason::kTriggeredByCrowdDeny,
                               QuietUiReason::kTriggeredDueToAbusiveRequests,
                               QuietUiReason::kTriggeredDueToAbusiveContent}) {
    SetCannedUiDecision(reason, absl::nullopt);

    RequestPermission(permissions::RequestType::kNotifications);

    ASSERT_EQ(
        test_api_->manager()->current_request_prompt_disposition_for_testing(),
        permissions::PermissionPromptDisposition::
            LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP);

    ClickOnChip(GetChip());

    test_api_->manager()->Accept();
    base::RunLoop().RunUntilIdle();
  }

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.Prompt.Notifications.LocationBarLeftQuietAbusiveChip."
      "Accepted.DidClickLearnMore",
      static_cast<int>(false), 3);
}

IN_PROC_BROWSER_TEST_F(QuietChipAutoPopupBubbleInteractiveTest,
                       PermissionGrantedOnceQuietChipAbusiveUmaTest) {
  base::HistogramTester histograms;

  for (QuietUiReason reason : {QuietUiReason::kTriggeredByCrowdDeny,
                               QuietUiReason::kTriggeredDueToAbusiveRequests,
                               QuietUiReason::kTriggeredDueToAbusiveContent}) {
    SetCannedUiDecision(reason, absl::nullopt);

    RequestPermission(permissions::RequestType::kNotifications);

    ASSERT_EQ(
        test_api_->manager()->current_request_prompt_disposition_for_testing(),
        permissions::PermissionPromptDisposition::
            LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP);

    ClickOnChip(GetChip());

    test_api_->manager()->AcceptThisTime();
    base::RunLoop().RunUntilIdle();
  }

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.Prompt.Notifications.LocationBarLeftQuietAbusiveChip."
      "AcceptedOnce.DidClickLearnMore",
      static_cast<int>(false), 3);
}

IN_PROC_BROWSER_TEST_F(QuietChipAutoPopupBubbleInteractiveTest,
                       PermissionDeniedOnceQuietChipAbusiveUmaTest) {
  base::HistogramTester histograms;

  for (QuietUiReason reason : {QuietUiReason::kTriggeredByCrowdDeny,
                               QuietUiReason::kTriggeredDueToAbusiveRequests,
                               QuietUiReason::kTriggeredDueToAbusiveContent}) {
    SetCannedUiDecision(reason, absl::nullopt);

    RequestPermission(permissions::RequestType::kNotifications);

    ASSERT_EQ(
        test_api_->manager()->current_request_prompt_disposition_for_testing(),
        permissions::PermissionPromptDisposition::
            LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP);

    ClickOnChip(GetChip());

    test_api_->manager()->Deny();
    base::RunLoop().RunUntilIdle();
  }

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.Prompt.Notifications.LocationBarLeftQuietAbusiveChip."
      "Denied.DidClickLearnMore",
      static_cast<int>(false), 3);
}

IN_PROC_BROWSER_TEST_F(QuietChipAutoPopupBubbleInteractiveTest,
                       PermissionDismissedOnceQuietChipAbusiveUmaTest) {
  base::HistogramTester histograms;

  for (QuietUiReason reason : {QuietUiReason::kTriggeredByCrowdDeny,
                               QuietUiReason::kTriggeredDueToAbusiveRequests,
                               QuietUiReason::kTriggeredDueToAbusiveContent}) {
    SetCannedUiDecision(reason, absl::nullopt);

    RequestPermission(permissions::RequestType::kNotifications);

    ASSERT_EQ(
        test_api_->manager()->current_request_prompt_disposition_for_testing(),
        permissions::PermissionPromptDisposition::
            LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP);

    ClickOnChip(GetChip());

    test_api_->manager()->Dismiss();
    base::RunLoop().RunUntilIdle();
  }

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.Prompt.Notifications.LocationBarLeftQuietAbusiveChip."
      "Dismissed.DidClickLearnMore",
      static_cast<int>(false), 3);
}

IN_PROC_BROWSER_TEST_F(QuietChipAutoPopupBubbleInteractiveTest,
                       QuietChipAbusiveClickLearnMoreUmaTest) {
  base::HistogramTester histograms;

  for (QuietUiReason reason : {QuietUiReason::kTriggeredByCrowdDeny,
                               QuietUiReason::kTriggeredDueToAbusiveRequests,
                               QuietUiReason::kTriggeredDueToAbusiveContent}) {
    SetCannedUiDecision(reason, absl::nullopt);

    RequestPermission(permissions::RequestType::kNotifications);

    ASSERT_EQ(
        test_api_->manager()->current_request_prompt_disposition_for_testing(),
        permissions::PermissionPromptDisposition::
            LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP);

    ClickOnChip(GetChip());

    views::View* bubble_view =
        GetChipController()->get_prompt_bubble_view_for_testing();
    ContentSettingBubbleContents* permission_prompt_bubble =
        static_cast<ContentSettingBubbleContents*>(bubble_view);

    ASSERT_TRUE(permission_prompt_bubble != nullptr);

    permission_prompt_bubble->learn_more_button_clicked_for_test();

    test_api_->manager()->Ignore();
    base::RunLoop().RunUntilIdle();
  }

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.Prompt.Notifications.LocationBarLeftQuietAbusiveChip."
      "Ignored."
      "DidShowBubble",
      static_cast<int>(true), 3);

  histograms.ExpectBucketCount(
      "Permissions.Prompt.Notifications.LocationBarLeftQuietAbusiveChip."
      "Ignored.DidClickLearnMore",
      static_cast<int>(true), 3);
}

IN_PROC_BROWSER_TEST_F(QuietChipAutoPopupBubbleInteractiveTest,
                       QuietChipAutoPopupBubbleEnabled) {
  ASSERT_TRUE(
      base::FeatureList::IsEnabled(permissions::features::kPermissionChip));
  ASSERT_TRUE(
      base::FeatureList::IsEnabled(features::kQuietNotificationPrompts));
  ASSERT_TRUE(base::FeatureList::IsEnabled(
      permissions::features::kPermissionQuietChip));

  SetCannedUiDecision(QuietUiReason::kTriggeredDueToAbusiveContent,
                      absl::nullopt);

  RequestPermission(permissions::RequestType::kGeolocation);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kNotifications);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kMidiSysex);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);
}

class QuietUIPromoInteractiveTest : public PermissionChipInteractiveTest {
 public:
  QuietUIPromoInteractiveTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kQuietNotificationPrompts,
          {{QuietNotificationPermissionUiConfig::kEnableAdaptiveActivation,
            "true"}}}},
        {{permissions::features::kPermissionQuietChip}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(QuietUIPromoInteractiveTest, QuietUIPromo) {
  auto* profile = browser()->profile();
  // Promo is not enabled by default.
  EXPECT_FALSE(QuietNotificationPermissionUiState::ShouldShowPromo(profile));

  for (const char* origin_spec :
       {"https://a.com", "https://b.com", "https://c.com"}) {
    GURL requesting_origin(origin_spec);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), requesting_origin));
    permissions::MockPermissionRequest notification_request(
        requesting_origin, permissions::RequestType::kNotifications);
    test_api_->manager()->AddRequest(GetActiveMainFrame(),
                                     &notification_request);
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(test_api_->manager()->ShouldCurrentRequestUseQuietUI());
    EXPECT_FALSE(QuietNotificationPermissionUiState::ShouldShowPromo(profile));
    test_api_->manager()->Deny();
    base::RunLoop().RunUntilIdle();
  }

  ContentSettingImageView& quiet_ui_icon = GetContentSettingImageView(
      ContentSettingImageModel::ImageType::NOTIFICATIONS_QUIET_PROMPT);

  EXPECT_FALSE(quiet_ui_icon.GetVisible());
  // `ContentSettingImageView::AnimationEnded()` was not triggered and IPH is
  // not shown.
  EXPECT_FALSE(quiet_ui_icon.critical_promo_bubble_for_testing());

  GURL notification("http://www.notification1.com/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), notification));
  permissions::MockPermissionRequest notification_request(
      notification, permissions::RequestType::kNotifications);
  test_api_->manager()->AddRequest(GetActiveMainFrame(), &notification_request);
  base::RunLoop().RunUntilIdle();

  // After 3 denied Notifications requests, Adaptive activation enabled quiet
  // permission prompt.
  EXPECT_TRUE(test_api_->manager()->ShouldCurrentRequestUseQuietUI());
  // At the first quiet permission prompt we show IPH.
  ASSERT_TRUE(QuietNotificationPermissionUiState::ShouldShowPromo(profile));

  EXPECT_TRUE(quiet_ui_icon.GetVisible());
  EXPECT_TRUE(quiet_ui_icon.is_animating_label());
  // Animation is reset to trigger `ContentSettingImageView::AnimationEnded()`.
  // `AnimationEnded` contains logic for displaying IPH and marking it as shown.
  quiet_ui_icon.reset_animation_for_testing();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(quiet_ui_icon.is_animating_label());

  // The IPH is showing.
  ASSERT_TRUE(quiet_ui_icon.critical_promo_bubble_for_testing());

  // The critical promo that is currently showing is the one created by a quiet
  // permission prompt.
  EXPECT_TRUE(quiet_ui_icon.critical_promo_bubble_for_testing()->is_open());
  quiet_ui_icon.critical_promo_bubble_for_testing()->Close();

  test_api_->manager()->Deny();
  base::RunLoop().RunUntilIdle();

  // After quiet permission prompt was resolved, the critical promo is reset.
  EXPECT_FALSE(quiet_ui_icon.critical_promo_bubble_for_testing());

  EXPECT_FALSE(quiet_ui_icon.GetVisible());

  // The second Notifications permission request to verify that the IPH is not
  // shown.
  GURL notification2("http://www.notification2.com/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), notification2));
  permissions::MockPermissionRequest notification_request2(
      notification2, permissions::RequestType::kNotifications);
  test_api_->manager()->AddRequest(GetActiveMainFrame(),
                                   &notification_request2);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_api_->manager()->ShouldCurrentRequestUseQuietUI());
  // At the second quiet permission prompt the IPH should be disabled.
  EXPECT_FALSE(QuietNotificationPermissionUiState::ShouldShowPromo(profile));

  EXPECT_TRUE(quiet_ui_icon.GetVisible());
  EXPECT_TRUE(quiet_ui_icon.is_animating_label());
  quiet_ui_icon.reset_animation_for_testing();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(quiet_ui_icon.is_animating_label());

  // The IPH did not show when `ContentSettingImageView::AnimationEnded()`
  // was triggered.
  EXPECT_FALSE(quiet_ui_icon.critical_promo_bubble_for_testing());

  test_api_->manager()->Deny();
  base::RunLoop().RunUntilIdle();
}

class QuietChipPermissionPromptBubbleViewInteractiveTest
    : public PermissionChipInteractiveTest {
 public:
  QuietChipPermissionPromptBubbleViewInteractiveTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kQuietNotificationPrompts,
         permissions::features::kPermissionQuietChip},
        {permissions::features::kConfirmationChip});
  }

 protected:
  using QuietUiReason = permissions::PermissionUiSelector::QuietUiReason;
  using WarningReason = permissions::PermissionUiSelector::WarningReason;

  void SetCannedUiDecision(absl::optional<QuietUiReason> quiet_ui_reason,
                           absl::optional<WarningReason> warning_reason) {
    test_api_->manager()->set_permission_ui_selector_for_testing(
        std::make_unique<TestQuietNotificationPermissionUiSelector>(
            permissions::PermissionUiSelector::Decision(quiet_ui_reason,
                                                        warning_reason)));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(QuietChipPermissionPromptBubbleViewInteractiveTest,
                       LoudChipIsShownForNonAbusiveRequests) {
  SetCannedUiDecision(absl::nullopt, absl::nullopt);

  RequestPermission(permissions::RequestType::kGeolocation);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kNotifications);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);
}

IN_PROC_BROWSER_TEST_F(QuietChipPermissionPromptBubbleViewInteractiveTest,
                       QuietChipIsShownForAbusiveRequests) {
  for (QuietUiReason reason : {QuietUiReason::kTriggeredByCrowdDeny,
                               QuietUiReason::kTriggeredDueToAbusiveRequests,
                               QuietUiReason::kTriggeredDueToAbusiveContent}) {
    SetCannedUiDecision(reason, absl::nullopt);

    RequestPermission(permissions::RequestType::kGeolocation);

    EXPECT_EQ(
        test_api_->manager()->current_request_prompt_disposition_for_testing(),
        permissions::PermissionPromptDisposition::
            LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

    test_api_->manager()->Accept();
    base::RunLoop().RunUntilIdle();

    RequestPermission(permissions::RequestType::kNotifications);

    // Quiet Chip is enabled, that means a quiet chip will be shown even if the
    // Chip experiment is disabled.
    EXPECT_EQ(
        test_api_->manager()->current_request_prompt_disposition_for_testing(),
        permissions::PermissionPromptDisposition::
            LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP);
  }
}

// The quiet UI icon is verified to make sure that the quiet chip is not shown
// when the quiet icon is shown.
IN_PROC_BROWSER_TEST_F(QuietChipPermissionPromptBubbleViewInteractiveTest,
                       QuietChipIsNotShownForNonAbusiveRequests) {
  SetCannedUiDecision(absl::nullopt, absl::nullopt);

  ContentSettingImageView& quiet_ui_icon = GetContentSettingImageView(
      ContentSettingImageModel::ImageType::NOTIFICATIONS_QUIET_PROMPT);
  EXPECT_FALSE(quiet_ui_icon.GetVisible());
  EXPECT_FALSE(GetChip()->GetVisible() &&
               GetChipController()->IsPermissionPromptChipVisible());

  RequestPermission(permissions::RequestType::kGeolocation);

  EXPECT_FALSE(quiet_ui_icon.GetVisible());
  ExpectNormalChip();

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kNotifications);

  EXPECT_FALSE(quiet_ui_icon.GetVisible());
  ExpectNormalChip();

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(QuietChipPermissionPromptBubbleViewInteractiveTest,
                       NotAnimatedQuietChipIsShownForAbusiveRequests) {
  for (QuietUiReason reason : {QuietUiReason::kTriggeredByCrowdDeny,
                               QuietUiReason::kTriggeredDueToAbusiveRequests,
                               QuietUiReason::kTriggeredDueToAbusiveContent}) {
    SetCannedUiDecision(reason, absl::nullopt);

    ContentSettingImageView& quiet_ui_icon = GetContentSettingImageView(
        ContentSettingImageModel::ImageType::NOTIFICATIONS_QUIET_PROMPT);

    EXPECT_FALSE(quiet_ui_icon.GetVisible());
    EXPECT_FALSE(GetChip()->GetVisible() &&
                 GetChipController()->IsPermissionPromptChipVisible());

    RequestPermission(permissions::RequestType::kGeolocation);

    EXPECT_FALSE(quiet_ui_icon.GetVisible());
    ExpectNormalChip();

    test_api_->manager()->Accept();
    base::RunLoop().RunUntilIdle();

    RequestPermission(permissions::RequestType::kNotifications);

    EXPECT_FALSE(quiet_ui_icon.GetVisible());
    ExpectQuietAbusiveChip();

    test_api_->manager()->Accept();
    base::RunLoop().RunUntilIdle();
  }
}

IN_PROC_BROWSER_TEST_F(QuietChipPermissionPromptBubbleViewInteractiveTest,
                       AnimatedQuietChipIsShownForNonAbusiveRequests) {
  for (QuietUiReason reason :
       {QuietUiReason::kEnabledInPrefs,
        QuietUiReason::kServicePredictedVeryUnlikelyGrant}) {
    SetCannedUiDecision(reason, absl::nullopt);

    ContentSettingImageView& quiet_ui_icon = GetContentSettingImageView(
        ContentSettingImageModel::ImageType::NOTIFICATIONS_QUIET_PROMPT);
    EXPECT_FALSE(quiet_ui_icon.GetVisible());
    EXPECT_FALSE(GetChip()->GetVisible() &&
                 GetChipController()->IsPermissionPromptChipVisible());

    RequestPermission(permissions::RequestType::kGeolocation);

    EXPECT_FALSE(quiet_ui_icon.GetVisible());
    ExpectNormalChip();

    test_api_->manager()->Accept();
    base::RunLoop().RunUntilIdle();

    RequestPermission(permissions::RequestType::kNotifications);

    EXPECT_FALSE(quiet_ui_icon.GetVisible());
    ExpectQuietChip();

    test_api_->manager()->Accept();
    base::RunLoop().RunUntilIdle();
  }
}

// Test that the quiet prompt disposition differs when permission is considered
// abusive (currently only applicable for Notifications) vs. when permission is
// not considered abusive.
IN_PROC_BROWSER_TEST_F(QuietChipPermissionPromptBubbleViewInteractiveTest,
                       DispositionAbusiveContentTest) {
  SetCannedUiDecision(QuietUiReason::kTriggeredDueToAbusiveContent,
                      WarningReason::kAbusiveContent);

  RequestPermission(permissions::RequestType::kGeolocation);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kNotifications);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP);
}

IN_PROC_BROWSER_TEST_F(QuietChipPermissionPromptBubbleViewInteractiveTest,
                       DispositionCrowdDenyTest) {
  SetCannedUiDecision(QuietUiReason::kTriggeredByCrowdDeny, absl::nullopt);

  RequestPermission(permissions::RequestType::kGeolocation);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kNotifications);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP);
}

IN_PROC_BROWSER_TEST_F(QuietChipPermissionPromptBubbleViewInteractiveTest,
                       DispositionEnabledInPrefsTest) {
  SetCannedUiDecision(QuietUiReason::kEnabledInPrefs, absl::nullopt);

  RequestPermission(permissions::RequestType::kGeolocation);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kNotifications);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_CHIP);
}

IN_PROC_BROWSER_TEST_F(QuietChipPermissionPromptBubbleViewInteractiveTest,
                       DispositionPredictedVeryUnlikelyGrantTest) {
  SetCannedUiDecision(QuietUiReason::kServicePredictedVeryUnlikelyGrant,
                      absl::nullopt);

  RequestPermission(permissions::RequestType::kGeolocation);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kNotifications);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_CHIP);
}

IN_PROC_BROWSER_TEST_F(QuietChipPermissionPromptBubbleViewInteractiveTest,
                       DispositionAbusiveRequestsTest) {
  SetCannedUiDecision(QuietUiReason::kTriggeredDueToAbusiveRequests,
                      WarningReason::kAbusiveRequests);

  RequestPermission(permissions::RequestType::kGeolocation);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE);

  test_api_->manager()->Accept();
  base::RunLoop().RunUntilIdle();

  RequestPermission(permissions::RequestType::kNotifications);

  EXPECT_EQ(
      test_api_->manager()->current_request_prompt_disposition_for_testing(),
      permissions::PermissionPromptDisposition::
          LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP);
}

class QuietChipFailFastInteractiveTest : public PermissionChipInteractiveTest {
 public:
  QuietChipFailFastInteractiveTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kQuietNotificationPrompts,
         permissions::features::kPermissionQuietChip,
         permissions::features::kPermissionChip,
         permissions::features::kFailFastQuietChip},
        {});
  }

 protected:
  using QuietUiReason = permissions::PermissionUiSelector::QuietUiReason;
  using WarningReason = permissions::PermissionUiSelector::WarningReason;

  void SetCannedUiDecision(absl::optional<QuietUiReason> quiet_ui_reason,
                           absl::optional<WarningReason> warning_reason) {
    test_api_->manager()->set_permission_ui_selector_for_testing(
        std::make_unique<TestQuietNotificationPermissionUiSelector>(
            permissions::PermissionUiSelector::Decision(quiet_ui_reason,
                                                        warning_reason)));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(QuietChipFailFastInteractiveTest,
                       NormalChipNoPreignoreTest) {
  base::HistogramTester histograms;

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/title1.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  ASSERT_TRUE(main_rfh);

  // Keep it above `IsSubscribedToPermissionChangeEvent` to make sure it does
  // not influence it.
  EXPECT_FALSE(content::EvalJs(main_rfh, kCheckNotifications).value.GetBool());

  bool IsPermissionStatusSubscribed =
      web_contents->GetBrowserContext()
          ->GetPermissionController()
          ->IsSubscribedToPermissionChangeEvent(
              blink::PermissionType::NOTIFICATIONS, main_rfh);

  EXPECT_FALSE(IsPermissionStatusSubscribed);

  base::RunLoop run_loop;

  content::AddNotifyListenerObserver(
      main_rfh->GetBrowserContext()->GetPermissionController(),
      run_loop.QuitClosure());

  EXPECT_TRUE(content::EvalJs(main_rfh, kAddNotificationsEventListener)
                  .value.GetBool());

  // `kAddNotificationsEventListener` execution is async. To informing that an
  // event listener has been added for a permission we should wait otherwise
  // `IsPermissionStatusSubscribed` is flaky.
  run_loop.Run();

  IsPermissionStatusSubscribed =
      web_contents->GetBrowserContext()
          ->GetPermissionController()
          ->IsSubscribedToPermissionChangeEvent(
              blink::PermissionType::NOTIFICATIONS, main_rfh);

  EXPECT_TRUE(IsPermissionStatusSubscribed);

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);

  EXPECT_FALSE(manager->IsRequestInProgress());

  {
    permissions::PermissionRequestObserver observer(web_contents);

    // Request permission in foreground tab, prompt should be shown.
    EXPECT_TRUE(content::ExecJs(
        main_rfh, kRequestNotifications,
        content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

    observer.Wait();
    EXPECT_TRUE(observer.request_shown());
  }

  EXPECT_TRUE(manager->IsRequestInProgress());

  manager->Accept();

  EXPECT_TRUE(content::EvalJs(main_rfh, kCheckNotifications).value.GetBool());
  EXPECT_FALSE(manager->IsRequestInProgress());

  IsPermissionStatusSubscribed =
      web_contents->GetBrowserContext()
          ->GetPermissionController()
          ->IsSubscribedToPermissionChangeEvent(
              blink::PermissionType::NOTIFICATIONS, main_rfh);

  EXPECT_TRUE(IsPermissionStatusSubscribed);

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.QuietPrompt.Preignore",
      static_cast<int>(blink::PermissionType::NOTIFICATIONS), 0);
}

IN_PROC_BROWSER_TEST_F(QuietChipFailFastInteractiveTest,
                       EventListenerAddedTest) {
  base::HistogramTester histograms;

  SetCannedUiDecision(QuietUiReason::kTriggeredDueToAbusiveRequests,
                      WarningReason::kAbusiveRequests);

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/title1.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  ASSERT_TRUE(main_rfh);

  // Keep it above `IsSubscribedToPermissionChangeEvent` to make sure it does
  // not influence it.
  EXPECT_FALSE(content::EvalJs(main_rfh, kCheckNotifications).value.GetBool());

  bool IsPermissionStatusSubscribed =
      web_contents->GetBrowserContext()
          ->GetPermissionController()
          ->IsSubscribedToPermissionChangeEvent(
              blink::PermissionType::NOTIFICATIONS, main_rfh);

  EXPECT_FALSE(IsPermissionStatusSubscribed);

  base::RunLoop run_loop;

  content::AddNotifyListenerObserver(
      main_rfh->GetBrowserContext()->GetPermissionController(),
      run_loop.QuitClosure());

  EXPECT_TRUE(content::EvalJs(main_rfh, kAddNotificationsEventListener)
                  .value.GetBool());

  // `kAddNotificationsEventListener` execution is async. To informing that an
  // event listener has been added for a permission we should wait otherwise
  // `IsPermissionStatusSubscribed` is flaky.
  run_loop.Run();

  IsPermissionStatusSubscribed =
      web_contents->GetBrowserContext()
          ->GetPermissionController()
          ->IsSubscribedToPermissionChangeEvent(
              blink::PermissionType::NOTIFICATIONS, main_rfh);

  EXPECT_TRUE(IsPermissionStatusSubscribed);

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);

  EXPECT_FALSE(manager->IsRequestInProgress());

  EXPECT_EQ("default", content::EvalJs(main_rfh, kRequestNotifications));

  EXPECT_TRUE(manager->IsRequestInProgress());
  manager->Accept();

  EXPECT_TRUE(content::EvalJs(main_rfh, kCheckNotifications).value.GetBool());
  EXPECT_FALSE(manager->IsRequestInProgress());

  IsPermissionStatusSubscribed =
      web_contents->GetBrowserContext()
          ->GetPermissionController()
          ->IsSubscribedToPermissionChangeEvent(
              blink::PermissionType::NOTIFICATIONS, main_rfh);

  EXPECT_TRUE(IsPermissionStatusSubscribed);

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectBucketCount(
      "Permissions.QuietPrompt.Preignore",
      static_cast<int>(blink::PermissionType::NOTIFICATIONS), 1);
}

// There are two ways of setting `change` event listener:
// `PermissionStatus.onchange` and `PermissionStatus.addEventListener`. There
// are two ways of removing the listener: `PermissionStatus.onchange = null`,
// `PermissionStatus.removeEventListener`. Any of the listeners should
// initialize internal subscribtion map. We should remove the internal
// subscribtion only if there is no `change` event listener left.
IN_PROC_BROWSER_TEST_F(QuietChipFailFastInteractiveTest,
                       EventListenerRemovedTest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/title1.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  ASSERT_TRUE(main_rfh);

  // Init global `PermissionStatus` variable and add API for assigning and
  // removing event listeners.
  ASSERT_EQ("", content::EvalJs(main_rfh, R"(
    var PermissionStatus;

    function onChangeListener(event) {}

    function addOnChange(){
      PermissionStatus.onchange = () => {};
    }

    function addEventListener(){
      PermissionStatus.addEventListener('change', onChangeListener);
    }

    function removeOnchange(){
      PermissionStatus.onchange = null;
    }

    function removeEventListener(){
      PermissionStatus.removeEventListener("change", onChangeListener);
    }
    )")
                    .error);

  // Initialize global JS variable `PermissionStatus`.
  EXPECT_TRUE(content::EvalJs(main_rfh, R"(
    new Promise(async resolve => {
      PermissionStatus =
        await navigator.permissions.query({name: 'notifications'});
      resolve(true);
    })
    )")
                  .value.GetBool());

  bool IsPermissionStatusSubscribed =
      web_contents->GetBrowserContext()
          ->GetPermissionController()
          ->IsSubscribedToPermissionChangeEvent(
              blink::PermissionType::NOTIFICATIONS, main_rfh);

  EXPECT_FALSE(IsPermissionStatusSubscribed);

  {
    // For each event create a new RunLoop otherwise crashes with
    // `run_loop.cc(335)] Check failed: run_allowed_`.
    base::RunLoop run_loop;
    content::AddNotifyListenerObserver(
        main_rfh->GetBrowserContext()->GetPermissionController(),
        run_loop.QuitClosure());

    // Set PermissionState.onchange listener.
    ASSERT_EQ("", content::EvalJs(main_rfh, "addOnChange()").error);

    // `kAddNotificationsEventListener` execution is async. To informing that an
    // event listener has been added for a permission we should wait otherwise
    // `IsPermissionStatusSubscribed` is flaky.
    run_loop.Run();

    IsPermissionStatusSubscribed =
        web_contents->GetBrowserContext()
            ->GetPermissionController()
            ->IsSubscribedToPermissionChangeEvent(
                blink::PermissionType::NOTIFICATIONS, main_rfh);

    EXPECT_TRUE(IsPermissionStatusSubscribed);
  }

  {
    base::RunLoop run_loop;
    content::AddNotifyListenerObserver(
        main_rfh->GetBrowserContext()->GetPermissionController(),
        run_loop.QuitClosure());

    ASSERT_EQ("", content::EvalJs(main_rfh, "removeOnchange()").error);

    run_loop.Run();

    IsPermissionStatusSubscribed =
        web_contents->GetBrowserContext()
            ->GetPermissionController()
            ->IsSubscribedToPermissionChangeEvent(
                blink::PermissionType::NOTIFICATIONS, main_rfh);

    EXPECT_FALSE(IsPermissionStatusSubscribed);
  }

  {
    base::RunLoop run_loop;
    content::AddNotifyListenerObserver(
        main_rfh->GetBrowserContext()->GetPermissionController(),
        run_loop.QuitClosure());

    // Add `change` event listener.
    ASSERT_EQ("", content::EvalJs(main_rfh, "addEventListener()").error);

    run_loop.Run();

    IsPermissionStatusSubscribed =
        web_contents->GetBrowserContext()
            ->GetPermissionController()
            ->IsSubscribedToPermissionChangeEvent(
                blink::PermissionType::NOTIFICATIONS, main_rfh);

    EXPECT_TRUE(IsPermissionStatusSubscribed);
  }

  {
    base::RunLoop run_loop;
    content::AddNotifyListenerObserver(
        main_rfh->GetBrowserContext()->GetPermissionController(),
        run_loop.QuitClosure());
    // Add the second lisener.
    ASSERT_EQ("", content::EvalJs(main_rfh, "addOnChange()").error);
    run_loop.Run();
  }

  {
    // Removing the first listener should not endup in disablign
    // `IsPermissionStatusSubscribed`. Do not need to call `run_loop.Run()` as
    // that event will not be processed.
    ASSERT_EQ("", content::EvalJs(main_rfh, "removeEventListener()").error);
    // run_loop.Run();

    IsPermissionStatusSubscribed =
        web_contents->GetBrowserContext()
            ->GetPermissionController()
            ->IsSubscribedToPermissionChangeEvent(
                blink::PermissionType::NOTIFICATIONS, main_rfh);

    EXPECT_TRUE(IsPermissionStatusSubscribed);

    base::RunLoop run_loop;
    content::AddNotifyListenerObserver(
        main_rfh->GetBrowserContext()->GetPermissionController(),
        run_loop.QuitClosure());
    // This will remove the internal listener.
    ASSERT_EQ("", content::EvalJs(main_rfh, "removeOnchange()").error);
    run_loop.Run();

    IsPermissionStatusSubscribed =
        web_contents->GetBrowserContext()
            ->GetPermissionController()
            ->IsSubscribedToPermissionChangeEvent(
                blink::PermissionType::NOTIFICATIONS, main_rfh);

    EXPECT_FALSE(IsPermissionStatusSubscribed);
  }
}

IN_PROC_BROWSER_TEST_F(PermissionChipInteractiveTest,
                       PermissionChipWithAndWithoutUserGesture) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/title1.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url,
                                                                1);
  ASSERT_TRUE(main_rfh);

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);

  EXPECT_FALSE(manager->IsRequestInProgress());

  // Request permission without user gesture
  {
    permissions::PermissionRequestObserver observer(web_contents);

    EXPECT_TRUE(content::ExecJs(
        main_rfh, kRequestNotifications,
        content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES |
            content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));

    observer.Wait();
    EXPECT_TRUE(observer.request_shown());

    EXPECT_TRUE(manager->IsRequestInProgress());
    EXPECT_FALSE(permissions::PermissionUtil::HasUserGesture(manager));
    absl::optional<permissions::PermissionPromptDisposition> disposition =
        manager->current_request_prompt_disposition_for_testing();

    ASSERT_TRUE(disposition.has_value());
    EXPECT_EQ(permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
              disposition.value());
    manager->Dismiss();
  }

  // Request permission with user gesture
  {
    permissions::PermissionRequestObserver observer(web_contents);

    EXPECT_TRUE(content::ExecJs(
        main_rfh, kRequestNotifications,
        content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

    observer.Wait();
    EXPECT_TRUE(observer.request_shown());

    EXPECT_TRUE(manager->IsRequestInProgress());
    EXPECT_TRUE(permissions::PermissionUtil::HasUserGesture(manager));
    absl::optional<permissions::PermissionPromptDisposition> disposition =
        manager->current_request_prompt_disposition_for_testing();

    ASSERT_TRUE(disposition.has_value());
    EXPECT_EQ(permissions::PermissionPromptDisposition::
                  LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE,
              disposition.value());
  }
}

IN_PROC_BROWSER_TEST_F(PermissionChipInteractiveTest,
                       PermissionRequestWithSameDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL top_url = embedded_test_server()->GetURL("/empty.html");
  GURL embedded_url = embedded_test_server()->GetURL("/empty.html");
  content::RenderFrameHost* main_rfh =
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                                top_url, 1);
  ASSERT_TRUE(main_rfh);

  content::RenderFrameHost* subframe = CreateIframe(main_rfh, embedded_url);
  ASSERT_TRUE(subframe);

  EXPECT_FALSE(content::EvalJs(main_rfh, kCheckNotifications).value.GetBool());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  auto* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  permissions::PermissionRequestObserver observer(web_contents);

  LocationBarView* location_bar =
      BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBarView();
  ASSERT_TRUE(location_bar);
  ChipController* chip_controller = location_bar->chip_controller();
  ChipExpansionObserver chip_expansion_observer(chip_controller->chip());

  EXPECT_FALSE(manager->IsRequestInProgress());

  EXPECT_TRUE(content::ExecJs(
      main_rfh, kRequestNotifications,
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Wait until a permission request is shown.
  observer.Wait();

  // Wait until chip finished expanding
  chip_expansion_observer.WaitForChipToExpand();

  EXPECT_TRUE(manager->IsRequestInProgress());
  EXPECT_TRUE(observer.request_shown());
  EXPECT_TRUE(manager->view_for_testing());
  EXPECT_TRUE(chip_controller->IsPermissionPromptChipVisible());

  // At first, we verify that the same document navigation on both, top level
  // and embedded frames will not end up resolving or hiding the current active
  // permission request chip.

  // Same document navigation on the main frame.
  content::TestNavigationObserver main_frame_navigation_observer(web_contents,
                                                                 1);
  ASSERT_TRUE(content::ExecJs(main_rfh, "window.location = '#navigateHere'"));
  main_frame_navigation_observer.Wait();

  EXPECT_TRUE(manager->IsRequestInProgress());
  EXPECT_TRUE(chip_controller->IsPermissionPromptChipVisible());
  EXPECT_FALSE(chip_controller->IsAnimating());

  // Same document navigation on the child frame.
  content::TestNavigationObserver embedded_frame_navigation_observer(
      web_contents, 1);
  ASSERT_TRUE(content::ExecJs(subframe, "window.location = '#navigateHere'"));
  embedded_frame_navigation_observer.Wait();

  EXPECT_TRUE(manager->IsRequestInProgress());
  EXPECT_TRUE(chip_controller->IsPermissionPromptChipVisible());

  // Second, we verify that cross-origin navigation of the embedded iframe will
  // not end up resolving or hiding the current active permission request chip.
  ASSERT_TRUE(NavigateToURLFromRenderer(subframe, embedded_url));
  EXPECT_TRUE(manager->IsRequestInProgress());
  EXPECT_TRUE(chip_controller->IsPermissionPromptChipVisible());

  manager->Accept();

  EXPECT_TRUE(content::EvalJs(main_rfh, kCheckNotifications).value.GetBool());
}
