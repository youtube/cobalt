// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_chip.h"

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/permissions/chip_controller.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/permission_ui_selector.h"
#include "components/permissions/test/mock_permission_request.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/button_test_api.h"

namespace {

class TestDelegate : public permissions::PermissionPrompt::Delegate {
 public:
  explicit TestDelegate(
      const GURL& origin,
      const std::vector<permissions::RequestType> request_types,
      bool with_gesture,
      content::WebContents* web_contents)
      : TestDelegate(origin,
                     request_types,
                     with_gesture,
                     absl::nullopt,
                     web_contents) {}

  explicit TestDelegate(
      const GURL& origin,
      const std::vector<permissions::RequestType> request_types,
      bool with_gesture,
      absl::optional<permissions::PermissionUiSelector::QuietUiReason>
          quiet_ui_reason,
      content::WebContents* web_contents)
      : quiet_ui_reason_(quiet_ui_reason), web_contents_(web_contents) {
    base::ranges::transform(
        request_types, std::back_inserter(requests_), [&](auto& request_type) {
          return std::make_unique<permissions::MockPermissionRequest>(
              origin, request_type,
              with_gesture
                  ? permissions::PermissionRequestGestureType::GESTURE
                  : permissions::PermissionRequestGestureType::NO_GESTURE);
        });
    base::ranges::transform(
        requests_, std::back_inserter(raw_requests_),
        &std::unique_ptr<permissions::PermissionRequest>::get);
  }

  const std::vector<permissions::PermissionRequest*>& Requests() override {
    return raw_requests_;
  }

  GURL GetRequestingOrigin() const override {
    return raw_requests_.front()->requesting_origin();
  }

  GURL GetEmbeddingOrigin() const override {
    return GURL("https://embedder.example.com");
  }

  void Accept() override { requests_.clear(); }
  void AcceptThisTime() override { requests_.clear(); }
  void Deny() override { requests_.clear(); }
  void Dismiss() override { requests_.clear(); }
  void Ignore() override { requests_.clear(); }
  void PreIgnoreQuietPrompt() override { requests_.clear(); }
  void SetManageClicked() override { requests_.clear(); }
  void SetLearnMoreClicked() override { requests_.clear(); }
  void SetHatsShownCallback(base::OnceCallback<void()> callback) override {}

  bool RecreateView() override { return false; }

  bool WasCurrentRequestAlreadyDisplayed() override {
    return was_current_request_already_displayed_;
  }
  bool ShouldDropCurrentRequestIfCannotShowQuietly() const override {
    return false;
  }
  bool ShouldCurrentRequestUseQuietUI() const override {
    return quiet_ui_reason_.has_value();
  }
  absl::optional<permissions::PermissionUiSelector::QuietUiReason>
  ReasonForUsingQuietUi() const override {
    return quiet_ui_reason_;
  }
  void SetDismissOnTabClose() override {}
  void SetPromptShown() override {}
  void SetDecisionTime() override {}

  base::WeakPtr<permissions::PermissionPrompt::Delegate> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  content::WebContents* GetAssociatedWebContents() override {
    return web_contents_;
  }

  bool IsRequestInProgress() { return !requests_.empty(); }

  void SetAlreadyDisplayed() { was_current_request_already_displayed_ = true; }

 private:
  std::vector<std::unique_ptr<permissions::PermissionRequest>> requests_;
  std::vector<permissions::PermissionRequest*> raw_requests_;
  bool was_current_request_already_displayed_ = false;
  absl::optional<permissions::PermissionUiSelector::QuietUiReason>
      quiet_ui_reason_;
  raw_ptr<content::WebContents> web_contents_;
  base::WeakPtrFactory<TestDelegate> weak_factory_{this};
};
}  // namespace

class PermissionChipUnitTest : public TestWithBrowserView {
 public:
  PermissionChipUnitTest()
      : TestWithBrowserView(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  PermissionChipUnitTest(const PermissionChipUnitTest&) = delete;
  PermissionChipUnitTest& operator=(const PermissionChipUnitTest&) = delete;

  void SetUp() override {
    TestWithBrowserView::SetUp();

    AddTab(browser(), GURL("http://a.com"));
    web_contents_ = browser()->tab_strip_model()->GetWebContentsAt(0);
  }

  void ClickOnChip(OmniboxChipButton& chip) {
    views::test::ButtonTestApi(&chip).NotifyClick(
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    base::RunLoop().RunUntilIdle();
  }

  raw_ptr<content::WebContents> web_contents_;

  base::TimeDelta kChipCollapseDuration = base::Seconds(12);
  base::TimeDelta kNormalChipDismissDuration = base::Seconds(6);
  base::TimeDelta kQuietChipDismissDuration = base::Seconds(18);
  base::TimeDelta kLongerThanAllTimersDuration = base::Seconds(50);
};

TEST_F(PermissionChipUnitTest, AlreadyDisplayedRequestTest) {
  TestDelegate delegate(GURL("https://test.origin"),
                        {permissions::RequestType::kNotifications}, false,
                        web_contents_);
  delegate.SetAlreadyDisplayed();

  EXPECT_TRUE(delegate.WasCurrentRequestAlreadyDisplayed());

  PermissionPromptChip chip_prompt(browser(), web_contents_, &delegate);
  ChipController* chip_controller =
      chip_prompt.get_chip_controller_for_testing();

  EXPECT_FALSE(chip_controller->IsBubbleShowing());

  // The permission request was already displayed, but the dismiss timer will
  // not be triggered directly after the chip is displayed because of the popup
  // bubble.
  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  // The default dismiss timer is 6 seconds. The chip should be still displayed
  // after 5 seconds.
  task_environment()->AdvanceClock(kNormalChipDismissDuration -
                                   base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.IsRequestInProgress());

  // Wait 2 more seconds for the dismiss timer to finish.
  task_environment()->AdvanceClock(base::Seconds(2));
  base::RunLoop().RunUntilIdle();

  // All chips are feature auto popup bubble. They should not resolve a prompt
  // automatically.
  ASSERT_TRUE(delegate.IsRequestInProgress());
}

TEST_F(PermissionChipUnitTest, DisplayChipAutoPopupTest) {
  TestDelegate delegate(GURL("https://test.origin"),
                        {permissions::RequestType::kNotifications}, true,
                        web_contents_);
  PermissionPromptChip chip_prompt(browser(), web_contents_, &delegate);
  ChipController* chip_controller =
      chip_prompt.get_chip_controller_for_testing();

  // Due to animation issue, the collapse timer will not be started.
  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  // Animation does not work. Most probably it is unit tests limitations.
  // `chip.is_fully_collapsed()` will not work as well.
  EXPECT_TRUE(chip_controller->IsAnimating());
  chip_controller->stop_animation_for_test();
  EXPECT_FALSE(chip_controller->IsAnimating());

  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kLongerThanAllTimersDuration);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.IsRequestInProgress());
  // Bubble is showing automatically.
  EXPECT_TRUE(chip_controller->IsBubbleShowing());

  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  // The seconds click on the chip hides the popup bubble.
  ClickOnChip(*chip_controller->chip());
  EXPECT_FALSE(chip_controller->IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  // After the second click, only dismiss timer should be active.
  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip_controller->is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kNormalChipDismissDuration +
                                   base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(delegate.IsRequestInProgress());
}

TEST_F(PermissionChipUnitTest, MultiClickOnChipAutoPopupTest) {
  TestDelegate delegate(GURL("https://test.origin"),
                        {permissions::RequestType::kNotifications}, true,
                        web_contents_);
  PermissionPromptChip chip_prompt(browser(), web_contents_, &delegate);
  ChipController* chip_controller =
      chip_prompt.get_chip_controller_for_testing();

  EXPECT_FALSE(chip_controller->IsBubbleShowing());

  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  // Animation does not work. Most probably it is unit tests limitations.
  // `chip.is_fully_collapsed()` will not work as well.
  EXPECT_TRUE(chip_controller->IsAnimating());
  chip_controller->stop_animation_for_test();
  EXPECT_FALSE(chip_controller->IsAnimating());

  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  // The permission prompt bubble is open automatically
  EXPECT_TRUE(chip_controller->IsBubbleShowing());

  task_environment()->AdvanceClock(kLongerThanAllTimersDuration);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.IsRequestInProgress());
  EXPECT_TRUE(chip_controller->IsBubbleShowing());

  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  ClickOnChip(*chip_controller->chip());

  // The permission prompt bubble is open automatically, hence the first click
  // on the chip should close the bubble.
  EXPECT_FALSE(chip_controller->IsBubbleShowing());
  EXPECT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip_controller->is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kNormalChipDismissDuration -
                                   base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(chip_controller->IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  ClickOnChip(*chip_controller->chip());
  EXPECT_TRUE(chip_controller->IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kLongerThanAllTimersDuration);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(chip_controller->IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  ClickOnChip(*chip_controller->chip());
  EXPECT_FALSE(chip_controller->IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip_controller->is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kNormalChipDismissDuration +
                                   base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(delegate.IsRequestInProgress());
}

TEST_F(PermissionChipUnitTest, DisplayQuietChipNoAbusiveTest) {
  TestDelegate delegate(
      GURL("https://test.origin"), {permissions::RequestType::kNotifications},
      true, permissions::PermissionUiSelector::QuietUiReason::kEnabledInPrefs,
      web_contents_);
  PermissionPromptChip chip_prompt(browser(), web_contents_, &delegate);
  ChipController* chip_controller =
      chip_prompt.get_chip_controller_for_testing();

  EXPECT_FALSE(chip_controller->IsBubbleShowing());

  // Due to animation issue, the collapse timer will not be started.
  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  // Animation does not work. Most probably it is unit tests limitations.
  // `chip.is_fully_collapsed()` will not work as well.
  EXPECT_TRUE(chip_controller->IsAnimating());
  chip_controller->stop_animation_for_test();
  EXPECT_FALSE(chip_controller->IsAnimating());

  EXPECT_TRUE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kChipCollapseDuration - base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_TRUE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  // Wait 2 more seconds for the collapse timer to finish.
  task_environment()->AdvanceClock(base::Seconds(2));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.IsRequestInProgress());

  // The collapse timer is fired and the dismiss timer is started.
  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip_controller->is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kNormalChipDismissDuration +
                                   base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(delegate.IsRequestInProgress());
}

TEST_F(PermissionChipUnitTest, MultiClickOnQuietChipNoAbusiveTest) {
  TestDelegate delegate(
      GURL("https://test.origin"), {permissions::RequestType::kNotifications},
      true, permissions::PermissionUiSelector::QuietUiReason::kEnabledInPrefs,
      web_contents_);
  PermissionPromptChip chip_prompt(browser(), web_contents_, &delegate);
  ChipController* chip_controller =
      chip_prompt.get_chip_controller_for_testing();

  EXPECT_FALSE(chip_controller->IsBubbleShowing());

  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  // Animation does not work. Most probably it is unit tests limitations.
  // `chip.is_fully_collapsed()` will not work as well.
  EXPECT_TRUE(chip_controller->IsAnimating());
  chip_controller->stop_animation_for_test();
  EXPECT_FALSE(chip_controller->IsAnimating());

  EXPECT_TRUE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->IsBubbleShowing());

  // The chip collapse timer is 12 seconds.
  task_environment()->AdvanceClock(kChipCollapseDuration - base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_TRUE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  ClickOnChip(*chip_controller->chip());
  EXPECT_TRUE(chip_controller->IsBubbleShowing());

  // Collapse timer was restarted.
  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  // After 30 seconds the permissin prompt popup bubble should still be
  // visible.
  task_environment()->AdvanceClock(kLongerThanAllTimersDuration);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.IsRequestInProgress());
  EXPECT_TRUE(chip_controller->IsBubbleShowing());

  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  // The seconds click on the chip hides the popup bubble.
  ClickOnChip(*chip_controller->chip());
  EXPECT_FALSE(chip_controller->IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  // After the second click, only dismiss timer should be active.
  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip_controller->is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kNormalChipDismissDuration -
                                   base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(chip_controller->IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  // The third click on the chip opens the popup bubble again.
  ClickOnChip(*chip_controller->chip());
  EXPECT_TRUE(chip_controller->IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kLongerThanAllTimersDuration);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(chip_controller->IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  ClickOnChip(*chip_controller->chip());
  EXPECT_FALSE(chip_controller->IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip_controller->is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kNormalChipDismissDuration +
                                   base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(delegate.IsRequestInProgress());
}

TEST_F(PermissionChipUnitTest, DisplayQuietChipAbusiveTest) {
  TestDelegate delegate(GURL("https://test.origin"),
                        {permissions::RequestType::kNotifications}, true,
                        permissions::PermissionUiSelector::QuietUiReason::
                            kTriggeredDueToAbusiveRequests,
                        web_contents_);

  PermissionPromptChip chip_prompt(browser(), web_contents_, &delegate);
  ChipController* chip_controller =
      chip_prompt.get_chip_controller_for_testing();

  EXPECT_FALSE(chip_controller->IsBubbleShowing());

  // The quiet abusive chip does not have animation and will start the dismiss
  // timer immediately after displaying.
  EXPECT_FALSE(chip_controller->IsAnimating());
  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip_controller->is_dismiss_timer_running_for_testing());

  // The dismiss timer is 18 seconds by default. After 17 seconds, the chip
  // should be there.
  task_environment()->AdvanceClock(kQuietChipDismissDuration -
                                   base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip_controller->is_dismiss_timer_running_for_testing());

  // Wait 2 more seconds for the dismiss timer to finish.
  task_environment()->AdvanceClock(base::Seconds(2));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(delegate.IsRequestInProgress());
}

TEST_F(PermissionChipUnitTest, MultiClickOnQuietChipAbusiveTest) {
  TestDelegate delegate(GURL("https://test.origin"),
                        {permissions::RequestType::kNotifications}, true,
                        permissions::PermissionUiSelector::QuietUiReason::
                            kTriggeredDueToAbusiveRequests,
                        web_contents_);
  PermissionPromptChip chip_prompt(browser(), web_contents_, &delegate);
  ChipController* chip_controller =
      chip_prompt.get_chip_controller_for_testing();

  EXPECT_FALSE(chip_controller->IsBubbleShowing());

  // The quiet abusive chip does not have animation and will start the dismiss
  // timer immediately after displaying.
  EXPECT_FALSE(chip_controller->IsAnimating());
  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip_controller->is_dismiss_timer_running_for_testing());

  // The dismiss timer is 18 seconds by default. After 17 seconds, the chip
  // should be there.
  task_environment()->AdvanceClock(kQuietChipDismissDuration -
                                   base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip_controller->is_dismiss_timer_running_for_testing());

  ClickOnChip(*chip_controller->chip());
  EXPECT_TRUE(chip_controller->IsBubbleShowing());

  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kLongerThanAllTimersDuration);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.IsRequestInProgress());
  EXPECT_TRUE(chip_controller->IsBubbleShowing());

  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  // The seconds click on the chip hides the popup bubble.
  ClickOnChip(*chip_controller->chip());
  EXPECT_FALSE(chip_controller->IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  // After the second click, only dismiss timer should be active.
  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip_controller->is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kQuietChipDismissDuration -
                                   base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(chip_controller->IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  // The third click on the chip opens the popup bubble again.
  ClickOnChip(*chip_controller->chip());
  EXPECT_TRUE(chip_controller->IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kLongerThanAllTimersDuration);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(chip_controller->IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  ClickOnChip(*chip_controller->chip());
  EXPECT_FALSE(chip_controller->IsBubbleShowing());
  ASSERT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip_controller->is_dismiss_timer_running_for_testing());

  task_environment()->AdvanceClock(kQuietChipDismissDuration +
                                   base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(delegate.IsRequestInProgress());
}
