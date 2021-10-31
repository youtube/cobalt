// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include <limits>

#include "cobalt/web_animations/animation_effect_timing_read_only.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace web_animations {

namespace {
AnimationEffectTimingReadOnly::Data CreateCanonicalTimingData() {
  return AnimationEffectTimingReadOnly::Data(
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromSeconds(1),
      AnimationEffectTimingReadOnly::kBoth, 0.2, 1.0,
      base::TimeDelta::FromSeconds(2), AnimationEffectTimingReadOnly::kNormal,
      cssom::TimingFunction::GetLinear());
}
}  // namespace

TEST(AnimationEffectTimingReadOnlyDataTests,
     ActiveTimeComputesToUnresolvedIfLocalTimeIsUnresolved) {
  AnimationEffectTimingReadOnly::Data timing = CreateCanonicalTimingData();

  base::Optional<base::TimeDelta> active_time =
      timing.ComputeActiveTimeFromLocalTime(base::nullopt);

  EXPECT_FALSE(active_time);
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     ActiveTimeSubtractsDelayDuringActivePhase) {
  AnimationEffectTimingReadOnly::Data timing = CreateCanonicalTimingData();

  base::Optional<base::TimeDelta> active_time =
      timing.ComputeActiveTimeFromLocalTime(base::TimeDelta::FromSecondsD(2.0));

  ASSERT_TRUE(active_time);
  EXPECT_DOUBLE_EQ(1.0, active_time->InSecondsF());
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     ActiveTimeComputesToZeroBeforeDelayWithFillModeBoth) {
  AnimationEffectTimingReadOnly::Data timing = CreateCanonicalTimingData();

  base::Optional<base::TimeDelta> active_time =
      timing.ComputeActiveTimeFromLocalTime(base::TimeDelta::FromSecondsD(0.5));

  ASSERT_TRUE(active_time);
  EXPECT_DOUBLE_EQ(0.0, active_time->InSecondsF());
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     ActiveTimeCapsAtDurationWithFillModeBoth) {
  AnimationEffectTimingReadOnly::Data timing = CreateCanonicalTimingData();

  base::Optional<base::TimeDelta> active_time =
      timing.ComputeActiveTimeFromLocalTime(base::TimeDelta::FromSecondsD(3.5));

  ASSERT_TRUE(active_time);
  EXPECT_DOUBLE_EQ(2.0, active_time->InSecondsF());
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     ScaledActiveTimeComputesToUnresolvedIfActiveTimeIsUnresolved) {
  AnimationEffectTimingReadOnly::Data timing = CreateCanonicalTimingData();

  base::Optional<base::TimeDelta> scaled_active_time =
      timing.ComputeScaledActiveTimeFromActiveTime(base::nullopt);

  EXPECT_FALSE(scaled_active_time);
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     ScaledActiveTimeAddsStartOffsetToActiveTime) {
  AnimationEffectTimingReadOnly::Data timing = CreateCanonicalTimingData();

  base::Optional<base::TimeDelta> scaled_active_time =
      timing.ComputeScaledActiveTimeFromActiveTime(
          base::TimeDelta::FromSecondsD(0.5));

  ASSERT_TRUE(scaled_active_time);
  EXPECT_DOUBLE_EQ(0.9, scaled_active_time->InSecondsF());
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     IterationTimeComputesToUnresolvedIfScaledActiveTimeIsUnresolved) {
  AnimationEffectTimingReadOnly::Data timing = CreateCanonicalTimingData();

  base::Optional<base::TimeDelta> iteration_time =
      timing.ComputeIterationTimeFromScaledActiveTime(base::nullopt);

  EXPECT_FALSE(iteration_time);
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     IterationTimeIsZeroIfDurationIsZero) {
  AnimationEffectTimingReadOnly::Data timing(
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromSeconds(1),
      AnimationEffectTimingReadOnly::kBoth, 0.2, 1.0,
      base::TimeDelta::FromSeconds(0), AnimationEffectTimingReadOnly::kNormal,
      cssom::TimingFunction::GetLinear());

  base::Optional<base::TimeDelta> iteration_time =
      timing.ComputeIterationTimeFromScaledActiveTime(
          base::TimeDelta::FromSecondsD(0.5));

  ASSERT_TRUE(iteration_time);
  EXPECT_DOUBLE_EQ(0.0, iteration_time->InSecondsF());
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     IterationTimeIsDurationIfAtEndOfIterations) {
  AnimationEffectTimingReadOnly::Data timing(
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromSeconds(1),
      AnimationEffectTimingReadOnly::kBoth, 0.2, 0.8,
      base::TimeDelta::FromSeconds(2), AnimationEffectTimingReadOnly::kNormal,
      cssom::TimingFunction::GetLinear());

  base::Optional<base::TimeDelta> iteration_time =
      timing.ComputeIterationTimeFromScaledActiveTime(
          base::TimeDelta::FromSecondsD(2.0));

  ASSERT_TRUE(iteration_time);
  EXPECT_DOUBLE_EQ(2.0, iteration_time->InSecondsF());
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     IterationTimeIsScaledActiveTimeModulosIterationDuration) {
  AnimationEffectTimingReadOnly::Data timing = CreateCanonicalTimingData();

  base::Optional<base::TimeDelta> iteration_time =
      timing.ComputeIterationTimeFromScaledActiveTime(
          base::TimeDelta::FromSecondsD(2.5));

  ASSERT_TRUE(iteration_time);
  EXPECT_DOUBLE_EQ(0.5, iteration_time->InSecondsF());
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     CurrentIterationIsUnresolvedIfActiveTimeIsUnresolved) {
  AnimationEffectTimingReadOnly::Data timing = CreateCanonicalTimingData();

  base::Optional<double> current_iteration = timing.ComputeCurrentIteration(
      base::nullopt, base::nullopt, base::nullopt);

  EXPECT_FALSE(current_iteration);
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     CurrentIterationIsFloorOfIterationStartIfActiveTimeIsZero) {
  AnimationEffectTimingReadOnly::Data timing(
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromSeconds(1),
      AnimationEffectTimingReadOnly::kBoth, 3.5, 0.8,
      base::TimeDelta::FromSeconds(2), AnimationEffectTimingReadOnly::kNormal,
      cssom::TimingFunction::GetLinear());

  base::TimeDelta active_time = base::TimeDelta();
  base::TimeDelta scaled_active_time =
      *timing.ComputeScaledActiveTimeFromActiveTime(active_time);
  base::TimeDelta iteration_time =
      *timing.ComputeIterationTimeFromScaledActiveTime(scaled_active_time);

  base::Optional<double> current_iteration = timing.ComputeCurrentIteration(
      active_time, scaled_active_time, iteration_time);

  ASSERT_TRUE(current_iteration);
  EXPECT_EQ(3.0, *current_iteration);
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     CurrentIterationIsInfinityIfDurationIsZeroAndIterationCountIsInfinity) {
  AnimationEffectTimingReadOnly::Data timing(
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromSeconds(1),
      AnimationEffectTimingReadOnly::kBoth, 3.5,
      std::numeric_limits<double>::infinity(), base::TimeDelta(),
      AnimationEffectTimingReadOnly::kNormal,
      cssom::TimingFunction::GetLinear());

  base::TimeDelta active_time(base::TimeDelta::FromSecondsD(1.0));
  base::TimeDelta scaled_active_time =
      *timing.ComputeScaledActiveTimeFromActiveTime(active_time);
  base::TimeDelta iteration_time =
      *timing.ComputeIterationTimeFromScaledActiveTime(scaled_active_time);

  base::Optional<double> current_iteration = timing.ComputeCurrentIteration(
      active_time, scaled_active_time, iteration_time);

  ASSERT_TRUE(current_iteration);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), *current_iteration);
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     CurrentIterationIsCeilMinusOneOfCountPlusStartIfDurationIsZero) {
  AnimationEffectTimingReadOnly::Data timing(
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromSeconds(1),
      AnimationEffectTimingReadOnly::kBoth, 3.5, 0.8, base::TimeDelta(),
      AnimationEffectTimingReadOnly::kNormal,
      cssom::TimingFunction::GetLinear());

  base::TimeDelta active_time(base::TimeDelta::FromSecondsD(1.0));
  base::TimeDelta scaled_active_time =
      *timing.ComputeScaledActiveTimeFromActiveTime(active_time);
  base::TimeDelta iteration_time =
      *timing.ComputeIterationTimeFromScaledActiveTime(scaled_active_time);

  base::Optional<double> current_iteration = timing.ComputeCurrentIteration(
      active_time, scaled_active_time, iteration_time);

  ASSERT_TRUE(current_iteration);
  EXPECT_EQ(4.0, *current_iteration);
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     CurrentIterationIsCountPlusStartMinusOneIfIterationTimeIsDuration) {
  AnimationEffectTimingReadOnly::Data timing(
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromSeconds(1),
      AnimationEffectTimingReadOnly::kBoth, 0.2, 0.8,
      base::TimeDelta::FromSeconds(2), AnimationEffectTimingReadOnly::kNormal,
      cssom::TimingFunction::GetLinear());

  // This active time is chosen such that the resulting iteration_time will
  // be equal to the effect timing's duration.
  base::TimeDelta active_time(base::TimeDelta::FromSecondsD(1.6));
  base::TimeDelta scaled_active_time =
      *timing.ComputeScaledActiveTimeFromActiveTime(active_time);
  base::TimeDelta iteration_time =
      *timing.ComputeIterationTimeFromScaledActiveTime(scaled_active_time);

  ASSERT_DOUBLE_EQ(timing.duration().InSecondsF(), iteration_time.InSecondsF());

  base::Optional<double> current_iteration = timing.ComputeCurrentIteration(
      active_time, scaled_active_time, iteration_time);

  ASSERT_TRUE(current_iteration);
  EXPECT_EQ(0.0, *current_iteration);
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     CurrentIterationIsFloorOfScaledActiveTimeDividedByDuration) {
  AnimationEffectTimingReadOnly::Data timing(
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromSeconds(1),
      AnimationEffectTimingReadOnly::kBoth, 0.0, 3.0,
      base::TimeDelta::FromSeconds(2), AnimationEffectTimingReadOnly::kNormal,
      cssom::TimingFunction::GetLinear());

  base::TimeDelta active_time(base::TimeDelta::FromSecondsD(4.5));
  base::TimeDelta scaled_active_time =
      *timing.ComputeScaledActiveTimeFromActiveTime(active_time);
  base::TimeDelta iteration_time =
      *timing.ComputeIterationTimeFromScaledActiveTime(scaled_active_time);

  base::Optional<double> current_iteration = timing.ComputeCurrentIteration(
      active_time, scaled_active_time, iteration_time);

  ASSERT_TRUE(current_iteration);
  EXPECT_EQ(2.0, *current_iteration);
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     DirectedTimeIsUnresolvedIfIterationTimeIsUnresolved) {
  AnimationEffectTimingReadOnly::Data timing = CreateCanonicalTimingData();

  base::Optional<base::TimeDelta> directed_time =
      timing.ComputeDirectedTimeFromIterationTime(base::nullopt, base::nullopt);

  EXPECT_FALSE(directed_time);
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     DirectedTimeIsIterationTimeForForwardsDirection) {
  AnimationEffectTimingReadOnly::Data timing(
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromSeconds(1),
      AnimationEffectTimingReadOnly::kBoth, 0.0, 4.0,
      base::TimeDelta::FromSeconds(1), AnimationEffectTimingReadOnly::kNormal,
      cssom::TimingFunction::GetLinear());

  base::Optional<base::TimeDelta> directed_time =
      timing.ComputeDirectedTimeFromIterationTime(
          base::TimeDelta::FromSecondsD(0.25), 0.0);

  ASSERT_TRUE(directed_time);
  EXPECT_EQ(0.25, directed_time->InSecondsF());
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     DirectedTimeIsDurationMinusIterationTimeForReverseDirection) {
  AnimationEffectTimingReadOnly::Data timing(
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromSeconds(1),
      AnimationEffectTimingReadOnly::kBoth, 0.0, 4.0,
      base::TimeDelta::FromSeconds(1), AnimationEffectTimingReadOnly::kReverse,
      cssom::TimingFunction::GetLinear());

  base::Optional<base::TimeDelta> directed_time =
      timing.ComputeDirectedTimeFromIterationTime(
          base::TimeDelta::FromSecondsD(0.25), 0.0);

  ASSERT_TRUE(directed_time);
  EXPECT_EQ(0.75, directed_time->InSecondsF());
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     DirectedTimeAlternatesForAlternateDirection) {
  AnimationEffectTimingReadOnly::Data timing(
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromSeconds(1),
      AnimationEffectTimingReadOnly::kBoth, 0.0, 4.0,
      base::TimeDelta::FromSeconds(1),
      AnimationEffectTimingReadOnly::kAlternate,
      cssom::TimingFunction::GetLinear());

  base::Optional<base::TimeDelta> directed_time =
      timing.ComputeDirectedTimeFromIterationTime(
          base::TimeDelta::FromSecondsD(0.25), 0.0);

  ASSERT_TRUE(directed_time);
  EXPECT_EQ(0.25, directed_time->InSecondsF());

  directed_time = timing.ComputeDirectedTimeFromIterationTime(
      base::TimeDelta::FromSecondsD(0.25), 1.0);

  ASSERT_TRUE(directed_time);
  EXPECT_EQ(0.75, directed_time->InSecondsF());
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     DirectedTimeAlternatesForAlternateReverseDirection) {
  AnimationEffectTimingReadOnly::Data timing(
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromSeconds(1),
      AnimationEffectTimingReadOnly::kBoth, 0.0, 4.0,
      base::TimeDelta::FromSeconds(1),
      AnimationEffectTimingReadOnly::kAlternateReverse,
      cssom::TimingFunction::GetLinear());

  base::Optional<base::TimeDelta> directed_time =
      timing.ComputeDirectedTimeFromIterationTime(
          base::TimeDelta::FromSecondsD(0.25), 0.0);

  ASSERT_TRUE(directed_time);
  EXPECT_EQ(0.75, directed_time->InSecondsF());

  directed_time = timing.ComputeDirectedTimeFromIterationTime(
      base::TimeDelta::FromSecondsD(0.25), 1.0);

  ASSERT_TRUE(directed_time);
  EXPECT_EQ(0.25, directed_time->InSecondsF());
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     DirectedTimeIsIterationTimeForCurrentIterationOfInfinity) {
  AnimationEffectTimingReadOnly::Data timing(
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromSeconds(1),
      AnimationEffectTimingReadOnly::kBoth, 0.0,
      std::numeric_limits<double>::infinity(), base::TimeDelta::FromSeconds(1),
      AnimationEffectTimingReadOnly::kAlternate,
      cssom::TimingFunction::GetLinear());

  base::Optional<base::TimeDelta> directed_time =
      timing.ComputeDirectedTimeFromIterationTime(
          base::TimeDelta::FromSecondsD(0.25),
          std::numeric_limits<double>::infinity());

  ASSERT_TRUE(directed_time);
  EXPECT_EQ(0.25, directed_time->InSecondsF());
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     TransformedTimeIsUnresolvedIfDirectedTimeIsUnresolved) {
  AnimationEffectTimingReadOnly::Data timing = CreateCanonicalTimingData();

  base::Optional<base::TimeDelta> transformed_time =
      timing.ComputeTransformedTimeFromDirectedTime(base::nullopt);

  EXPECT_FALSE(transformed_time);
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     TransformedTimeIsDirectedTimeIfDurationIsInfinity) {
  AnimationEffectTimingReadOnly::Data timing(
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromSeconds(1),
      AnimationEffectTimingReadOnly::kBoth, 0.0, 3.0, base::TimeDelta::Max(),
      AnimationEffectTimingReadOnly::kNormal,
      cssom::TimingFunction::GetLinear());

  base::Optional<base::TimeDelta> transformed_time =
      timing.ComputeTransformedTimeFromDirectedTime(
          base::TimeDelta::FromSecondsD(1.5));

  ASSERT_TRUE(transformed_time);
  EXPECT_DOUBLE_EQ(1.5, transformed_time->InSecondsF());
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     TransformedTimeIsZeroIfDurationIsZero) {
  AnimationEffectTimingReadOnly::Data timing(
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromSeconds(1),
      AnimationEffectTimingReadOnly::kBoth, 0.0, 3.0, base::TimeDelta(),
      AnimationEffectTimingReadOnly::kNormal,
      cssom::TimingFunction::GetLinear());

  base::Optional<base::TimeDelta> transformed_time =
      timing.ComputeTransformedTimeFromDirectedTime(
          base::TimeDelta::FromSecondsD(1.5));

  ASSERT_TRUE(transformed_time);
  EXPECT_DOUBLE_EQ(0.0, transformed_time->InSecondsF());
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     TransformedTimeIsEqualToDirectedTimeForLinearEasing) {
  AnimationEffectTimingReadOnly::Data timing = CreateCanonicalTimingData();

  base::Optional<base::TimeDelta> transformed_time =
      timing.ComputeTransformedTimeFromDirectedTime(
          base::TimeDelta::FromSecondsD(1.5));

  ASSERT_TRUE(transformed_time);
  EXPECT_NEAR(1.5, transformed_time->InSecondsF(), 0.0001);
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     TransformedTimeIsEqualToTimingFunctionResultTimesDuration) {
  AnimationEffectTimingReadOnly::Data timing(
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromSeconds(1),
      AnimationEffectTimingReadOnly::kBoth, 0.2, 1.0,
      base::TimeDelta::FromSeconds(2), AnimationEffectTimingReadOnly::kNormal,
      cssom::TimingFunction::GetEase());

  base::Optional<base::TimeDelta> transformed_time =
      timing.ComputeTransformedTimeFromDirectedTime(
          base::TimeDelta::FromSecondsD(1.5));

  ASSERT_TRUE(transformed_time);
  EXPECT_NEAR(cssom::TimingFunction::GetEase()->Evaluate(0.75) *
                  timing.duration().InSecondsF(),
              transformed_time->InSecondsF(), 0.0001);
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     IterationProgressIsUnresolvedIfTransformedTimeIsUnresolved) {
  AnimationEffectTimingReadOnly::Data timing = CreateCanonicalTimingData();

  base::Optional<double> iteration_progress =
      timing.ComputeIterationProgressFromTransformedTime(base::nullopt);

  EXPECT_FALSE(iteration_progress);
}

TEST(AnimationEffectTimingReadOnlyDataTests,
     IterationProgressIsTransformedTimeDividedByDuration) {
  AnimationEffectTimingReadOnly::Data timing = CreateCanonicalTimingData();

  base::Optional<double> iteration_progress =
      timing.ComputeIterationProgressFromTransformedTime(
          base::TimeDelta::FromSecondsD(1.5));

  ASSERT_TRUE(iteration_progress);
  EXPECT_DOUBLE_EQ(0.75, *iteration_progress);
}

}  // namespace web_animations
}  // namespace cobalt
