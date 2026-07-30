// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_event_activation_protector.h"

#include <memory>
#include <utility>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/views/input_protection/input_protector_delegate.h"
#include "ui/views/metrics.h"
#include "ui/views/test/views_test_base.h"

namespace views {

class TestInputProtectorDelegate : public InputProtectorDelegate {
 public:
  TestInputProtectorDelegate() = default;
  ~TestInputProtectorDelegate() override = default;

  bool IsPossiblyUnintendedInteraction(
      const ui::Event& event,
      InputEventActivationProtector* protector) override {
    return is_unintended_interaction_;
  }

  void set_is_unintended_interaction(bool is_unintended_interaction) {
    is_unintended_interaction_ = is_unintended_interaction;
  }

 private:
  bool is_unintended_interaction_ = false;
};

class InputEventActivationProtectorTest : public ViewsTestBase {
 public:
  InputEventActivationProtectorTest()
      : ViewsTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ui::MouseEvent CreateClickEvent() {
    return ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                          gfx::Point(), base::TimeTicks::Now(),
                          ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  }
};

TEST_F(InputEventActivationProtectorTest, DefaultBehaviorBlocksRapidClicks) {
  InputEventActivationProtector protector;

  // Initially not protected before visibility changes are simulated.
  EXPECT_FALSE(
      protector.IsPossiblyUnintendedInteraction(CreateClickEvent(), false));

  // Simulate the view becoming visible.
  protector.VisibilityChanged(true);

  // Clicks immediately after visibility changes should be blocked.
  EXPECT_TRUE(
      protector.IsPossiblyUnintendedInteraction(CreateClickEvent(), false));

  // Clicks after the protection period should be allowed.
  task_environment()->FastForwardBy(GetDoubleClickInterval() +
                                    base::Milliseconds(1));
  EXPECT_FALSE(
      protector.IsPossiblyUnintendedInteraction(CreateClickEvent(), false));
}

TEST_F(InputEventActivationProtectorTest, DelegateIsCalled) {
  auto delegate = std::make_unique<TestInputProtectorDelegate>();
  TestInputProtectorDelegate* delegate_ptr = delegate.get();
  InputEventActivationProtector protector(std::move(delegate));

  // Simulate the view being shown, which starts the protection period in normal
  // usage.
  protector.VisibilityChanged(true);

  delegate_ptr->set_is_unintended_interaction(false);
  EXPECT_FALSE(
      protector.IsPossiblyUnintendedInteraction(CreateClickEvent(), false));

  delegate_ptr->set_is_unintended_interaction(true);
  EXPECT_TRUE(
      protector.IsPossiblyUnintendedInteraction(CreateClickEvent(), false));
}

TEST_F(InputEventActivationProtectorTest,
       CustomConstructorReplacesDefaultDelegate) {
  auto delegate = std::make_unique<TestInputProtectorDelegate>();
  TestInputProtectorDelegate* delegate_ptr = delegate.get();
  InputEventActivationProtector protector(std::move(delegate));

  // Simulate the view being shown.
  protector.VisibilityChanged(true);

  // If the default delegate were active, this click would be blocked
  // by the show cooldown. Since we set the custom delegate to allow it,
  // it should succeed, proving the default delegate was replaced.
  delegate_ptr->set_is_unintended_interaction(false);
  EXPECT_FALSE(
      protector.IsPossiblyUnintendedInteraction(CreateClickEvent(), false));
}

TEST_F(InputEventActivationProtectorTest,
       WindowStationarityChangeRestartsProtectionPeriod) {
  InputEventActivationProtector protector;

  // Initially not protected before visibility changes are simulated.
  EXPECT_FALSE(
      protector.IsPossiblyUnintendedInteraction(CreateClickEvent(), false));

  // Stationarity changes before the view is visible should not restart
  // protection.
  protector.OnWindowStationaryStateChanged();
  EXPECT_FALSE(
      protector.IsPossiblyUnintendedInteraction(CreateClickEvent(), false));

  // Simulate the view becoming visible.
  protector.VisibilityChanged(true);
  EXPECT_TRUE(
      protector.IsPossiblyUnintendedInteraction(CreateClickEvent(), false));

  // Wait for the protection period to expire.
  task_environment()->FastForwardBy(GetDoubleClickInterval() +
                                    base::Milliseconds(1));
  EXPECT_FALSE(
      protector.IsPossiblyUnintendedInteraction(CreateClickEvent(), false));

  // Simulate a stationarity change, which should restart the protection period.
  protector.OnWindowStationaryStateChanged();

  // Clicks immediately after a stationarity change should be blocked.
  EXPECT_TRUE(
      protector.IsPossiblyUnintendedInteraction(CreateClickEvent(), false));
}

TEST_F(InputEventActivationProtectorTest, MultipleDelegates) {
  auto delegate1 = std::make_unique<TestInputProtectorDelegate>();
  TestInputProtectorDelegate* delegate1_ptr = delegate1.get();
  auto delegate2 = std::make_unique<TestInputProtectorDelegate>();
  TestInputProtectorDelegate* delegate2_ptr = delegate2.get();

  InputEventActivationProtector protector(std::move(delegate1));
  protector.AddDelegate(std::move(delegate2));

  // Simulate the view being shown.
  protector.VisibilityChanged(true);

  // Verify that the protector allows the interaction, since both delegates
  // allow it.
  delegate1_ptr->set_is_unintended_interaction(false);
  delegate2_ptr->set_is_unintended_interaction(false);
  EXPECT_FALSE(
      protector.IsPossiblyUnintendedInteraction(CreateClickEvent(), false));

  // Verify that the protector blocks the interaction, since the first delegate
  // blocks it.
  delegate1_ptr->set_is_unintended_interaction(true);
  delegate2_ptr->set_is_unintended_interaction(false);
  EXPECT_TRUE(
      protector.IsPossiblyUnintendedInteraction(CreateClickEvent(), false));

  // Verify that the protector blocks the interaction, since the second delegate
  // blocks it.
  delegate1_ptr->set_is_unintended_interaction(false);
  delegate2_ptr->set_is_unintended_interaction(true);
  EXPECT_TRUE(
      protector.IsPossiblyUnintendedInteraction(CreateClickEvent(), false));

  // Verify that the protector blocks the interaction, since both delegates
  // block it.
  delegate1_ptr->set_is_unintended_interaction(true);
  delegate2_ptr->set_is_unintended_interaction(true);
  EXPECT_TRUE(
      protector.IsPossiblyUnintendedInteraction(CreateClickEvent(), false));
}

}  // namespace views
