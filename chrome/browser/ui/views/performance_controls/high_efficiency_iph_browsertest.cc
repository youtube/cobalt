// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/user_education/test/feature_promo_test_util.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "url/gurl.h"

class HighEfficiencyHelpPromoTest : public InProcessBrowserTest {
 public:
  HighEfficiencyHelpPromoTest() = default;
  ~HighEfficiencyHelpPromoTest() override = default;

  void SetUp() override {
    iph_features_.InitAndEnableFeaturesWithParameters(
        {{feature_engagement::kIPHHighEfficiencyModeFeature, {}}});

    InProcessBrowserTest::SetUp();
  }

  void TearDown() override { InProcessBrowserTest::TearDown(); }

  BrowserFeaturePromoController* GetFeaturePromoController() {
    auto* promo_controller = static_cast<BrowserFeaturePromoController*>(
        browser()->window()->GetFeaturePromoController());
    return promo_controller;
  }

  void PressButton(views::Button* button) {
    views::test::InteractionTestUtilSimulatorViews::PressButton(
        button, ui::test::InteractionTestUtil::InputType::kMouse);
  }

  void TriggerHighEfficiencyPromo() {
    auto lock =
        BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();

    views::NamedWidgetShownWaiter waiter(
        views::test::AnyWidgetTestPasskey{},
        user_education::HelpBubbleView::kViewClassName);

    int tab_count_threshold = 10;  // The threshold count is a constant.
    for (int i = 0; i < tab_count_threshold; i++)
      chrome::AddTabAt(browser(), GURL(), i, true);

    waiter.WaitIfNeededAndGet();

    auto* const promo_controller = GetFeaturePromoController();
    bool promo_active = user_education::test::WaitForStartupPromo(
        promo_controller, feature_engagement::kIPHHighEfficiencyModeFeature);
    EXPECT_TRUE(promo_active);
  }

 private:
  feature_engagement::test::ScopedIphFeatureList iph_features_;
};

// Check that the high efficiency mode in-product help promo is shown when
// a tab threshold is reached and dismisses correctly when the app menu
// button is pushed
IN_PROC_BROWSER_TEST_F(HighEfficiencyHelpPromoTest, ShowPromoOnTabThreshold) {
  TriggerHighEfficiencyPromo();
  auto* app_menu_button_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kToolbarAppMenuButtonElementId,
          browser()->window()->GetElementContext());
  PressButton(views::AsViewClass<views::Button>(app_menu_button_view));

  auto* const promo_controller = GetFeaturePromoController();
  bool promo_active = promo_controller->IsPromoActive(
      feature_engagement::kIPHHighEfficiencyModeFeature);
  EXPECT_FALSE(promo_active);
}

// Confirm that High Efficiency mode is enabled when the custom action
// button for high efficiency mode is clicked
IN_PROC_BROWSER_TEST_F(HighEfficiencyHelpPromoTest, PromoCustomActionClicked) {
  EXPECT_TRUE(performance_manager::user_tuning::UserPerformanceTuningManager::
                  GetInstance()
                      ->IsHighEfficiencyModeDefault());
  EXPECT_FALSE(performance_manager::user_tuning::UserPerformanceTuningManager::
                   GetInstance()
                       ->IsHighEfficiencyModeActive());

  TriggerHighEfficiencyPromo();

  auto* const promo_controller = GetFeaturePromoController();
  auto* promo_bubble = promo_controller->promo_bubble_for_testing()
                           ->AsA<user_education::HelpBubbleViews>()
                           ->bubble_view();
  auto* custom_action_button = promo_bubble->GetDefaultButtonForTesting();
  PressButton(custom_action_button);

  EXPECT_FALSE(performance_manager::user_tuning::UserPerformanceTuningManager::
                   GetInstance()
                       ->IsHighEfficiencyModeDefault());
  EXPECT_TRUE(performance_manager::user_tuning::UserPerformanceTuningManager::
                  GetInstance()
                      ->IsHighEfficiencyModeActive());
}

// Check that the performance menu item is alerted when the high efficiency
// promo is shown and the app menu button is clicked
IN_PROC_BROWSER_TEST_F(HighEfficiencyHelpPromoTest,
                       AlertMenuItemWhenPromoShown) {
  TriggerHighEfficiencyPromo();

  auto* app_menu_button_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kToolbarAppMenuButtonElementId,
          browser()->window()->GetElementContext());
  PressButton(views::AsViewClass<views::Button>(app_menu_button_view));

  AppMenuModel* app_menu_model =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->app_menu_button()
          ->app_menu_model();
  const size_t more_tools_index =
      app_menu_model->GetIndexOfCommandId(IDC_MORE_TOOLS_MENU).value();
  EXPECT_TRUE(app_menu_model->IsAlertedAt(more_tools_index));

  ToolsMenuModel toolModel(app_menu_model, browser());
  const size_t performance_index =
      toolModel.GetIndexOfCommandId(IDC_PERFORMANCE).value();
  EXPECT_TRUE(toolModel.IsAlertedAt(performance_index));
}
