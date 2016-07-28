/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "base/time.h"
#include "cobalt/renderer/smoothed_value.h"
#include "testing/gtest/include/gtest/gtest.h"

using cobalt::renderer::SmoothedValue;

namespace {
base::TimeTicks SecondsToTime(double seconds) {
  return base::TimeTicks() + base::TimeDelta::FromSecondsD(seconds);
}
}  // namespace

TEST(SmoothedValueTest, FirstValueSnaps) {
  SmoothedValue value(base::TimeDelta::FromSeconds(1));

  value.SetTarget(0.5, SecondsToTime(0.0));

  EXPECT_EQ(0.5, value.GetValueAtTime(SecondsToTime(0.0)));
  EXPECT_EQ(0.5, value.GetValueAtTime(SecondsToTime(0.5)));
  EXPECT_EQ(0.5, value.GetValueAtTime(SecondsToTime(1.0)));
  EXPECT_EQ(0.5, value.GetValueAtTime(SecondsToTime(1.5)));
}

TEST(SmoothedValueTest, BezierSplineValues) {
  SmoothedValue value(base::TimeDelta::FromSeconds(1));

  value.SetTarget(0.0, SecondsToTime(0.0));
  value.SetTarget(1.0, SecondsToTime(1.0));

  EXPECT_DOUBLE_EQ(0.0, value.GetValueAtTime(SecondsToTime(1.0)));
  EXPECT_DOUBLE_EQ(0.104, value.GetValueAtTime(SecondsToTime(1.2)));
  EXPECT_DOUBLE_EQ(0.5, value.GetValueAtTime(SecondsToTime(1.5)));
  EXPECT_DOUBLE_EQ(0.784, value.GetValueAtTime(SecondsToTime(1.7)));
  EXPECT_DOUBLE_EQ(1.0, value.GetValueAtTime(SecondsToTime(2.0)));
  EXPECT_DOUBLE_EQ(1.0, value.GetValueAtTime(SecondsToTime(2.5)));
}

TEST(SmoothedValueTest, BezierSplineFromSecondToThirdTarget) {
  // Setup an initial source and target value, and then half-way through that
  // transition, setup a new target value, and ensure that we transition
  // smoothly to it from within the first transition.
  SmoothedValue value(base::TimeDelta::FromSeconds(1));

  value.SetTarget(0.0, SecondsToTime(0.0));
  value.SetTarget(1.0, SecondsToTime(1.0));
  value.SetTarget(2.0, SecondsToTime(2.0));

  EXPECT_DOUBLE_EQ(1.0, value.GetValueAtTime(SecondsToTime(2.0)));
  EXPECT_DOUBLE_EQ(1.104, value.GetValueAtTime(SecondsToTime(2.2)));
  EXPECT_DOUBLE_EQ(1.5, value.GetValueAtTime(SecondsToTime(2.5)));
  EXPECT_DOUBLE_EQ(1.784, value.GetValueAtTime(SecondsToTime(2.7)));
  EXPECT_DOUBLE_EQ(2.0, value.GetValueAtTime(SecondsToTime(3.0)));
  EXPECT_DOUBLE_EQ(2.0, value.GetValueAtTime(SecondsToTime(3.5)));
}

TEST(SmoothedValueTest, SnapToTarget) {
  SmoothedValue value(base::TimeDelta::FromSeconds(1));

  value.SetTarget(0.0, SecondsToTime(0.0));
  value.SetTarget(1.0, SecondsToTime(1.0));
  value.SnapToTarget();

  EXPECT_DOUBLE_EQ(1.0, value.GetValueAtTime(SecondsToTime(1.0)));
  EXPECT_DOUBLE_EQ(1.0, value.GetValueAtTime(SecondsToTime(1.2)));
  EXPECT_DOUBLE_EQ(1.0, value.GetValueAtTime(SecondsToTime(1.5)));
  EXPECT_DOUBLE_EQ(1.0, value.GetValueAtTime(SecondsToTime(1.7)));
  EXPECT_DOUBLE_EQ(1.0, value.GetValueAtTime(SecondsToTime(2.0)));
  EXPECT_DOUBLE_EQ(1.0, value.GetValueAtTime(SecondsToTime(2.5)));
}

TEST(SmoothedValueTest, TransitionFromTransition) {
  // Setup an initial source and target value, and then half-way through that
  // transition, setup a new target value, and ensure that we transition
  // smoothly to it from within the first transition.
  SmoothedValue value(base::TimeDelta::FromSeconds(1));

  value.SetTarget(0.0, SecondsToTime(0.0));
  value.SetTarget(1.0, SecondsToTime(1.0));
  // Retarget halfway through the previous transition.
  value.SetTarget(2.0, SecondsToTime(1.5));

  EXPECT_DOUBLE_EQ(0.5, value.GetValueAtTime(SecondsToTime(1.5)));
  EXPECT_DOUBLE_EQ(0.848, value.GetValueAtTime(SecondsToTime(1.7)));
  EXPECT_DOUBLE_EQ(1.4375, value.GetValueAtTime(SecondsToTime(2.0)));
  EXPECT_DOUBLE_EQ(1.7705, value.GetValueAtTime(SecondsToTime(2.2)));
  EXPECT_DOUBLE_EQ(2.0, value.GetValueAtTime(SecondsToTime(2.5)));
  EXPECT_DOUBLE_EQ(2.0, value.GetValueAtTime(SecondsToTime(3.0)));
}
