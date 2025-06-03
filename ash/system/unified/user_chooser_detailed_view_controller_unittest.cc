// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/user_chooser_detailed_view_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/unified/quick_settings_footer.h"
#include "ash/system/unified/quick_settings_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "base/test/scoped_feature_list.h"
#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

AccountId GetActiveUser() {
  return Shell::Get()
      ->session_controller()
      ->GetUserSession(/*user_index=*/0)
      ->user_info.account_id;
}

class UserChooserDetailedViewControllerTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  UserChooserDetailedViewControllerTest() {
    if (IsQsRevampEnabled()) {
      feature_list_.InitAndEnableFeature(features::kQsRevamp);
    } else {
      feature_list_.InitAndDisableFeature(features::kQsRevamp);
    }
  }

  UserChooserDetailedViewControllerTest(
      const UserChooserDetailedViewControllerTest&) = delete;
  UserChooserDetailedViewControllerTest& operator=(
      const UserChooserDetailedViewControllerTest&) = delete;

  ~UserChooserDetailedViewControllerTest() override = default;

  // TODO(b/305075031) clean up after the flag is removed.
  bool IsQsRevampEnabled() const { return true; }

  // AshTestBase
  void SetUp() override {
    AshTestBase::SetUp();
    tray_test_api_ = std::make_unique<SystemTrayTestApi>();
    disable_animations_ =
        std::make_unique<ui::ScopedAnimationDurationScaleMode>(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  }

  bool IsBubbleViewVisible(ViewID view_id) const {
    return tray_test_api_->IsBubbleViewVisible(view_id, /*open_tray=*/false);
  }

  void ShowUserChooserView() {
    if (!IsQsRevampEnabled()) {
      // Click the user avatar button.
      ASSERT_TRUE(IsBubbleViewVisible(VIEW_ID_QS_USER_AVATAR_BUTTON));
      tray_test_api()->ClickBubbleView(VIEW_ID_QS_USER_AVATAR_BUTTON);
      return;
    }

    // Click the power button to show the menu.
    ASSERT_TRUE(IsBubbleViewVisible(VIEW_ID_QS_POWER_BUTTON));
    tray_test_api()->ClickBubbleView(VIEW_ID_QS_POWER_BUTTON);
    PowerButton* power_button = GetPrimaryUnifiedSystemTray()
                                    ->bubble()
                                    ->quick_settings_view()
                                    ->footer_for_testing()
                                    ->power_button_for_testing();
    ASSERT_TRUE(power_button->IsMenuShowing());

    // Click the user email address.
    views::View* email_item =
        power_button->GetMenuViewForTesting()->GetMenuItemByID(
            VIEW_ID_QS_POWER_EMAIL_MENU_BUTTON);
    ASSERT_TRUE(email_item);
    LeftClickOn(email_item);
  }

  SystemTrayTestApi* tray_test_api() { return tray_test_api_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> disable_animations_;
  std::unique_ptr<SystemTrayTestApi> tray_test_api_;
};

INSTANTIATE_TEST_SUITE_P(QsRevamp,
                         UserChooserDetailedViewControllerTest,
                         testing::Bool());

TEST_P(UserChooserDetailedViewControllerTest,
       ShowMultiProfileLoginWithOverview) {
  // Enter overview mode.
  EnterOverview();
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Show system tray.
  tray_test_api()->ShowBubble();
  ASSERT_TRUE(tray_test_api()->IsTrayBubbleOpen());

  // Show the user chooser view.
  ShowUserChooserView();

  // Click on add user button to show multi profile login window.
  ASSERT_TRUE(tray_test_api()->IsBubbleViewVisible(VIEW_ID_ADD_USER_BUTTON,
                                                   /*open_tray=*/false));
  tray_test_api()->ClickBubbleView(VIEW_ID_ADD_USER_BUTTON);
}

TEST_P(UserChooserDetailedViewControllerTest, SwitchUserWithOverview) {
  // Add a secondary user.
  const AccountId secondary_user =
      AccountId::FromUserEmail("secondary@gmail.com");
  GetSessionControllerClient()->AddUserSession(secondary_user.GetUserEmail());
  ASSERT_NE(GetActiveUser(), secondary_user);

  // Create an activatable widget.
  std::unique_ptr<views::Widget> widget = CreateTestWidget();

  // Enter overview mode.
  EnterOverview();
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Show system tray.
  tray_test_api()->ShowBubble();
  ASSERT_TRUE(tray_test_api()->IsTrayBubbleOpen());

  // Show the user chooser view.
  ShowUserChooserView();

  const int secondary_user_button_id = VIEW_ID_USER_ITEM_BUTTON_START + 1;
  ASSERT_TRUE(tray_test_api()->IsBubbleViewVisible(secondary_user_button_id,
                                                   /*open_tray=*/false));
  tray_test_api()->ClickBubbleView(secondary_user_button_id);

  // Active user is switched.
  EXPECT_EQ(GetActiveUser(), secondary_user);
}

TEST_P(UserChooserDetailedViewControllerTest,
       MultiProfileLoginDisabledForFamilyLinkUsers) {
  EXPECT_TRUE(UserChooserDetailedViewController::IsUserChooserEnabled());

  GetSessionControllerClient()->Reset();

  // Log in as a child user.
  SimulateUserLogin("child@gmail.com", user_manager::USER_TYPE_CHILD);

  EXPECT_FALSE(UserChooserDetailedViewController::IsUserChooserEnabled());
}

}  // namespace
}  // namespace ash
