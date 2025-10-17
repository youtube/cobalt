// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_skeleton_loader_view.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/test/views_test_base.h"

namespace ash {
namespace {

class QuickInsertSkeletonLoaderViewTest : public views::ViewsTestBase {
 public:
  QuickInsertSkeletonLoaderViewTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

TEST_F(QuickInsertSkeletonLoaderViewTest, InitialStateHasNoOpacity) {
  QuickInsertSkeletonLoaderView view;

  EXPECT_EQ(view.layer()->opacity(), 0);
}

TEST_F(QuickInsertSkeletonLoaderViewTest,
       StartAnimationDoesNotImmediatelyAnimate) {
  QuickInsertSkeletonLoaderView view;

  view.StartAnimationAfter(base::Seconds(1));

  EXPECT_FALSE(view.layer()->GetAnimator()->is_animating());
  EXPECT_EQ(view.layer()->opacity(), 0);
}

TEST_F(QuickInsertSkeletonLoaderViewTest, AnimationStartsAfterDelay) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  QuickInsertSkeletonLoaderView view;
  view.StartAnimationAfter(base::Seconds(1));
  task_environment()->FastForwardBy(base::Seconds(1));

  EXPECT_TRUE(view.layer()->GetAnimator()->is_animating());
  EXPECT_EQ(view.layer()->opacity(), 1.0f);
}

TEST_F(QuickInsertSkeletonLoaderViewTest,
       StopAnimationStopsUnstartedAnimationAndResetsOpacity) {
  QuickInsertSkeletonLoaderView view;
  view.StartAnimationAfter(base::Seconds(1));

  view.StopAnimation();

  EXPECT_EQ(view.layer()->opacity(), 0.0f);
  EXPECT_FALSE(view.layer()->GetAnimator()->is_animating());
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(view.layer()->GetAnimator()->is_animating());
}

TEST_F(QuickInsertSkeletonLoaderViewTest,
       StopAnimationStopsRunningAnimationAndResetsOpacity) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  QuickInsertSkeletonLoaderView view;
  view.StartAnimationAfter(base::Seconds(1));
  task_environment()->FastForwardBy(base::Seconds(1));

  view.StopAnimation();

  EXPECT_EQ(view.layer()->opacity(), 0.0f);
  EXPECT_FALSE(view.layer()->GetAnimator()->is_animating());
}

}  // namespace
}  // namespace ash
