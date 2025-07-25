// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/link_capturing/link_capturing_features.h"
#include "chrome/browser/banners/test_app_banner_manager_desktop.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"
#include "chrome/browser/ui/views/location_bar/intent_chip_button.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/common/buildflags.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/live_caption/caption_util.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#include "ui/base/pointer/touch_ui_controller.h"
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Ref;
using ::testing::Return;

namespace {

// Returns an appropriate set of string replacements; passing the wrong number
// of replacements for the body text of the IPH will cause a DCHECK.
user_education::FeaturePromoSpecification::FormatParameters
GetReplacementsForFeature(const base::Feature& feature) {
  if (&feature == &feature_engagement::kIPHDesktopPwaInstallFeature) {
    return u"Placeholder Text";
  }
  return user_education::FeaturePromoSpecification::NoSubstitution();
}

}  // namespace

class FeaturePromoDialogTest : public DialogBrowserTest {
 public:
  FeaturePromoDialogTest()
      : update_dialog_scope_(web_app::SetIdentityUpdateDialogActionForTesting(
            web_app::AppIdentityUpdate::kSkipped)) {
    feature_ = GetFeatureForTest();
    scoped_feature_list_.InitAndEnableFeatures(
        /* allow_and_enable_features =*/{*feature_},
        /* disable_features =*/
        {media::kLiveCaption, feature_engagement::kIPHLiveCaptionFeature});

    // TODO(crbug.com/1141984): fix cause of bubbles overflowing the
    // screen and remove this.
    set_should_verify_dialog_bounds(false);
  }
  void SetUp() override {
    webapps::TestAppBannerManagerDesktop::SetUp();

    DialogBrowserTest::SetUp();
  }
  void SetUpOnMainThread() override {
    DialogBrowserTest::SetUpOnMainThread();
    browser()->window()->Activate();
    ui_test_utils::BrowserActivationWaiter(browser()).WaitForActivation();
  }

  void TearDownOnMainThread() override {
    Profile* const profile = browser()->profile();
    web_app::WebAppRegistrar& registrar =
        web_app::WebAppProvider::GetForTest(profile)->registrar_unsafe();
    for (const auto& app_id : registrar.GetAppIds()) {
      apps::AppReadinessWaiter app_readiness_waiter(
          profile, app_id, apps::Readiness::kUninstalledByUser);
      web_app::test::UninstallWebApp(profile, app_id);
      app_readiness_waiter.Await();
    }

    DialogBrowserTest::TearDownOnMainThread();
  }

  ~FeaturePromoDialogTest() override = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    auto* mock_tracker = static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForBrowserContext(
            browser()->profile()));
    ASSERT_TRUE(mock_tracker);

    auto* const promo_controller =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->GetFeaturePromoController();
    ASSERT_TRUE(promo_controller);

    // The browser may have already queued a promo for startup. Since the test
    // uses a mock, cancel that and just show it directly.
    const auto status = promo_controller->GetPromoStatus(*feature_);
    if (status == user_education::FeaturePromoStatus::kQueuedForStartup)
      promo_controller->EndPromo(
          *feature_, user_education::FeaturePromoCloseReason::kAbortPromo);

    // Set up mock tracker to allow the IPH, then attempt to show it.
    EXPECT_CALL(*mock_tracker, ShouldTriggerHelpUI(Ref(*feature_)))
        .Times(1)
        .WillOnce(Return(true));
    user_education::FeaturePromoParams params(*feature_);
    params.body_params = GetReplacementsForFeature(*feature_);
    ASSERT_TRUE(promo_controller->MaybeShowPromo(std::move(params)));
  }

 private:
  // Looks up the IPH name from the test name and returns the corresponding
  // base::Feature.
  const base::Feature* GetFeatureForTest() const {
    const std::string full_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();
    const std::string name = full_name.substr(full_name.find('_') + 1);
    std::vector<const base::Feature*> iph_features =
        feature_engagement::GetAllFeatures();
    auto feature_it =
        base::ranges::find(iph_features, name, &base::Feature::name);
    CHECK(feature_it != iph_features.end());
    return *feature_it;
  }

  raw_ptr<const base::Feature> feature_ = nullptr;
  feature_engagement::test::ScopedIphFeatureList scoped_feature_list_;
  base::AutoReset<absl::optional<web_app::AppIdentityUpdate>>
      update_dialog_scope_;

  static void RegisterMockTracker(content::BrowserContext* context) {
    feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(CreateMockTracker));
  }

  static std::unique_ptr<KeyedService> CreateMockTracker(
      content::BrowserContext* context) {
    auto mock_tracker =
        std::make_unique<NiceMock<feature_engagement::test::MockTracker>>();

    // Allow calls for other IPH.
    EXPECT_CALL(*mock_tracker, ShouldTriggerHelpUI(_))
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));

    return mock_tracker;
  }

  base::CallbackListSubscription subscription_{
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(
              base::BindRepeating(RegisterMockTracker))};
};

// Adding new tests for your promo
//
// When you add a new IPH, add a test for your promo. In most cases you
// can follow these steps:
//
// 1. Get the feature name for your IPH. This will be the of the form
//    IPH_<name>. It should be the same as defined in
//    //components/feature_engagement/public/feature_constants.cc.
//
// 2. Define a new test below with the name InvokeUi_IPH_<name>. Place
//    it in alphabetical order.
//
// 3. Call set_baseline("<cl-number>"). This enables Skia Gold pixel
//    tests for your IPH.
//
// 4. Call ShowAndVerifyUi().
//
// For running your test reference the docs in
// //chrome/browser/ui/test/test_browser_dialog.h

IN_PROC_BROWSER_TEST_F(FeaturePromoDialogTest, InvokeUi_IPH_DesktopPwaInstall) {
  set_baseline("2936082");
  // Navigate to an installable site so PWA install icon shows up.
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/banners/manifest_test_page.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* app_banner_manager =
      webapps::TestAppBannerManagerDesktop::FromWebContents(web_contents);
  app_banner_manager->WaitForInstallableCheck();
  EXPECT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())
                  ->toolbar()
                  ->location_bar()
                  ->page_action_icon_controller()
                  ->GetIconView(PageActionIconType::kPwaInstall)
                  ->GetVisible());
  browser()->window()->Activate();
  ui_test_utils::BrowserActivationWaiter(browser()).WaitForActivation();

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FeaturePromoDialogTest,
                       InvokeUi_IPH_DesktopTabGroupsNewGroup) {
  set_baseline("4067389");
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FeaturePromoDialogTest, InvokeUi_IPH_LiveCaption) {
  if (!captions::IsLiveCaptionFeatureSupported())
    return;

  BrowserView::GetBrowserViewForBrowser(browser())
      ->toolbar()
      ->media_button()
      ->Show();
  RunScheduledLayouts();

  set_baseline("2936082");
  ShowAndVerifyUi();
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(FeaturePromoDialogTest, InvokeUi_IPH_ProfileSwitch) {
  set_baseline("3710120");
  ShowAndVerifyUi();
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(FeaturePromoDialogTest, InvokeUi_IPH_TabSearch) {
  set_baseline("2991858");
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FeaturePromoDialogTest,
                       InvokeUi_IPH_DesktopSharedHighlighting) {
  set_baseline("3253618");
  ShowAndVerifyUi();
}

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

// Need a separate fixture to override the feature flag.
class FeaturePromoDialogWebUITabStripTest : public FeaturePromoDialogTest {
 public:
  FeaturePromoDialogWebUITabStripTest() {
    feature_list_.InitAndEnableFeature(features::kWebUITabStrip);
  }

  ~FeaturePromoDialogWebUITabStripTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(FeaturePromoDialogWebUITabStripTest,
                       InvokeUi_IPH_WebUITabStrip) {
  ui::TouchUiController::TouchUiScoperForTesting touch_override(true);
  RunScheduledLayouts();

  set_baseline("2473537");
  ShowAndVerifyUi();
}

#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
