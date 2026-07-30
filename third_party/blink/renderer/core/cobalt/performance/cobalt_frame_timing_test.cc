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

#include <memory>

#include "base/time/time.h"
#include "cc/metrics/begin_main_frame_metrics.h"
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

TEST(CobaltFrameInputTimingTest, MainFrameSnapshot) {
  CobaltMainFrameSnapshot snapshot;
  snapshot.bmf_start = AtMs(0);
  snapshot.main_frame_run_time = AtMs(6.5);
  snapshot.metrics = std::make_unique<cc::BeginMainFrameMetrics>();
  snapshot.metrics->handle_input_events = base::Milliseconds(1.25);

  CobaltFrameInputTiming t =
      CobaltFrameInputTiming::FromMainFrameSnapshot(&snapshot);

  ASSERT_TRUE(t.main_frame_queue_duration.has_value());
  EXPECT_DOUBLE_EQ(*t.main_frame_queue_duration, 6.5);
  ASSERT_TRUE(t.handle_input_events_duration.has_value());
  EXPECT_DOUBLE_EQ(*t.handle_input_events_duration, 1.25);
}

TEST(CobaltFrameInputTimingTest, CompositorOnlyFrameIsNull) {
  CobaltFrameInputTiming t =
      CobaltFrameInputTiming::FromMainFrameSnapshot(nullptr);
  EXPECT_FALSE(t.main_frame_queue_duration.has_value());
  EXPECT_FALSE(t.handle_input_events_duration.has_value());

  // A snapshot without metrics is treated the same way.
  CobaltMainFrameSnapshot snapshot;
  snapshot.bmf_start = AtMs(0);
  snapshot.main_frame_run_time = AtMs(1);
  t = CobaltFrameInputTiming::FromMainFrameSnapshot(&snapshot);
  EXPECT_FALSE(t.main_frame_queue_duration.has_value());
  EXPECT_FALSE(t.handle_input_events_duration.has_value());
}

TEST(CobaltFrameInputTimingTest, MissingRunTimeGivesNullQueue) {
  CobaltMainFrameSnapshot snapshot;
  snapshot.bmf_start = AtMs(0);
  snapshot.metrics = std::make_unique<cc::BeginMainFrameMetrics>();

  CobaltFrameInputTiming t =
      CobaltFrameInputTiming::FromMainFrameSnapshot(&snapshot);

  EXPECT_FALSE(t.main_frame_queue_duration.has_value());
  ASSERT_TRUE(t.handle_input_events_duration.has_value());
  EXPECT_DOUBLE_EQ(*t.handle_input_events_duration, 0.0);
}

}  // namespace blink
