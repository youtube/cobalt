// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "third_party/blink/renderer/core/cobalt/performance/cobalt_frame_timing.h"

#include "base/time/time.h"
#include "components/viz/common/frame_timing_details.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

// Returns a non-null TimeTicks `ms` milliseconds after an arbitrary origin.
base::TimeTicks AtMs(double ms) {
  return base::TimeTicks() + base::Seconds(1) + base::Milliseconds(ms);
}

}  // namespace

TEST(CobaltFrameDrawBreakdownTest, AllTimestampsPresent) {
  viz::FrameTimingDetails details;
  details.received_compositor_frame_timestamp = AtMs(0);
  details.draw_start_timestamp = AtMs(2);
  details.swap_timings.viz_scheduled_draw = AtMs(3.5);
  details.swap_timings.gpu_task_ready = AtMs(8);
  details.swap_timings.gpu_started_draw = AtMs(9);
  details.swap_timings.swap_start = AtMs(12);

  CobaltFrameDrawBreakdown b =
      CobaltFrameDrawBreakdown::FromFrameTimingDetails(details);

  ASSERT_TRUE(b.receive_to_draw_duration.has_value());
  EXPECT_DOUBLE_EQ(*b.receive_to_draw_duration, 2.0);
  ASSERT_TRUE(b.viz_draw_duration.has_value());
  EXPECT_DOUBLE_EQ(*b.viz_draw_duration, 1.5);
  ASSERT_TRUE(b.gpu_dependency_wait_duration.has_value());
  EXPECT_DOUBLE_EQ(*b.gpu_dependency_wait_duration, 4.5);
  ASSERT_TRUE(b.gpu_queue_duration.has_value());
  EXPECT_DOUBLE_EQ(*b.gpu_queue_duration, 1.0);
  ASSERT_TRUE(b.gpu_draw_duration.has_value());
  EXPECT_DOUBLE_EQ(*b.gpu_draw_duration, 3.0);

  // The four draw stages add up to draw_start -> swap_start.
  EXPECT_DOUBLE_EQ(*b.viz_draw_duration + *b.gpu_dependency_wait_duration +
                       *b.gpu_queue_duration + *b.gpu_draw_duration,
                   10.0);
}

TEST(CobaltFrameDrawBreakdownTest, MissingTaskReadyFoldsIntoQueue) {
  viz::FrameTimingDetails details;
  details.draw_start_timestamp = AtMs(0);
  details.swap_timings.viz_scheduled_draw = AtMs(1);
  details.swap_timings.gpu_started_draw = AtMs(6);
  details.swap_timings.swap_start = AtMs(7);

  CobaltFrameDrawBreakdown b =
      CobaltFrameDrawBreakdown::FromFrameTimingDetails(details);

  EXPECT_FALSE(b.receive_to_draw_duration.has_value());
  EXPECT_FALSE(b.gpu_dependency_wait_duration.has_value());
  ASSERT_TRUE(b.gpu_queue_duration.has_value());
  EXPECT_DOUBLE_EQ(*b.gpu_queue_duration, 5.0);
  ASSERT_TRUE(b.gpu_draw_duration.has_value());
  EXPECT_DOUBLE_EQ(*b.gpu_draw_duration, 1.0);
}

TEST(CobaltFrameDrawBreakdownTest, NoTimestampsGivesAllNull) {
  CobaltFrameDrawBreakdown b = CobaltFrameDrawBreakdown::FromFrameTimingDetails(
      viz::FrameTimingDetails());

  EXPECT_FALSE(b.receive_to_draw_duration.has_value());
  EXPECT_FALSE(b.viz_draw_duration.has_value());
  EXPECT_FALSE(b.gpu_dependency_wait_duration.has_value());
  EXPECT_FALSE(b.gpu_queue_duration.has_value());
  EXPECT_FALSE(b.gpu_draw_duration.has_value());
}

TEST(CobaltFrameDrawBreakdownTest, NegativeIntervalIsNull) {
  viz::FrameTimingDetails details;
  details.draw_start_timestamp = AtMs(5);
  details.swap_timings.viz_scheduled_draw = AtMs(4);
  details.swap_timings.gpu_started_draw = AtMs(6);
  details.swap_timings.swap_start = AtMs(8);

  CobaltFrameDrawBreakdown b =
      CobaltFrameDrawBreakdown::FromFrameTimingDetails(details);

  EXPECT_FALSE(b.viz_draw_duration.has_value());
  ASSERT_TRUE(b.gpu_queue_duration.has_value());
  EXPECT_DOUBLE_EQ(*b.gpu_queue_duration, 2.0);
  ASSERT_TRUE(b.gpu_draw_duration.has_value());
  EXPECT_DOUBLE_EQ(*b.gpu_draw_duration, 2.0);
}

}  // namespace blink
