// Copyright 2023 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/ui_navigation/scroll_engine/free_scrolling_nav_item.h"

#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace cobalt {
namespace ui_navigation {
namespace scroll_engine {

namespace {

constexpr base::TimeDelta kTestDuration = base::TimeDelta::FromSeconds(5);

class FreeScrollingNavItemTest : public ::testing::Test {
 protected:
  void SetUp() override {
    free_scrolling_nav_item.set_clock_for_testing(&test_clock);
    free_scrolling_nav_item.set_initial_change_for_testing(test_clock.Now());
  }

  scoped_refptr<NavItem> mock_nav_item_;
  math::Vector2dF initial_offset_{0.f, 0.f};
  math::Vector2dF target_offset_{10.f, 10.f};
  base::TimeDelta animation_duration_{kTestDuration};
  float animation_slope_{0.5f};
  base::SimpleTestClock test_clock;
  FreeScrollingNavItem free_scrolling_nav_item{
      mock_nav_item_, initial_offset_, target_offset_, animation_duration_,
      animation_slope_};
};

}  // namespace

TEST_F(FreeScrollingNavItemTest, GetFractionOfCurrentProgressTest) {
  EXPECT_EQ(free_scrolling_nav_item.GetFractionOfCurrentProgress(), 0.f);
  test_clock.Advance(kTestDuration / 2);
  EXPECT_EQ(free_scrolling_nav_item.GetFractionOfCurrentProgress(), 0.5f);
  test_clock.Advance(kTestDuration / 2);
  EXPECT_EQ(free_scrolling_nav_item.GetFractionOfCurrentProgress(), 1.f);
  test_clock.Advance(kTestDuration / 2);
  EXPECT_EQ(free_scrolling_nav_item.GetFractionOfCurrentProgress(), 1.f);
}

TEST_F(FreeScrollingNavItemTest, AnimationIsCompleteTest) {
  EXPECT_FALSE(free_scrolling_nav_item.AnimationIsComplete());
  test_clock.Advance(kTestDuration / 2);
  EXPECT_FALSE(free_scrolling_nav_item.AnimationIsComplete());
  test_clock.Advance(kTestDuration / 2);
  EXPECT_TRUE(free_scrolling_nav_item.AnimationIsComplete());
}

TEST_F(FreeScrollingNavItemTest, GetCurrentOffsetTest) {
  EXPECT_EQ(free_scrolling_nav_item.GetCurrentOffset(), initial_offset_);
  test_clock.Advance(kTestDuration);
  EXPECT_EQ(free_scrolling_nav_item.GetCurrentOffset(), target_offset_);
  test_clock.Advance(kTestDuration);
  EXPECT_EQ(free_scrolling_nav_item.GetCurrentOffset(), target_offset_);
}

}  // namespace scroll_engine
}  // namespace ui_navigation
}  // namespace cobalt
