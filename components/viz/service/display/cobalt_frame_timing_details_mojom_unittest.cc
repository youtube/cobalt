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

#include "base/time/time.h"
#include "components/viz/common/frame_timing_details.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/viz/public/cpp/compositing/begin_frame_args_mojom_traits.h"
#include "services/viz/public/cpp/compositing/frame_timing_details_mojom_traits.h"
#include "services/viz/public/mojom/compositing/frame_timing_details.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/mojom/presentation_feedback_mojom_traits.h"
#include "ui/gfx/mojom/swap_timings_mojom_traits.h"

namespace viz {
namespace {

base::TimeTicks AtMs(int ms) {
  return base::TimeTicks() + base::Seconds(1) + base::Milliseconds(ms);
}

// The cobalt-frame PerformanceObserver splits drawDuration using the viz/GPU
// draw-stage timestamps in gfx::SwapTimings, which are only serialized under
// [EnableIf=is_cobalt] in ui/gfx/mojom/swap_timings.mojom. This round-trips
// FrameTimingDetails (the struct viz sends to the renderer) so that a
// Chromium rebase cannot silently drop those fields.
TEST(CobaltFrameTimingDetailsMojomTest, SwapTimingsDrawStagesRoundTrip) {
  FrameTimingDetails input;
  input.received_compositor_frame_timestamp = AtMs(0);
  input.draw_start_timestamp = AtMs(2);
  input.swap_timings.viz_scheduled_draw = AtMs(3);
  input.swap_timings.gpu_task_ready = AtMs(8);
  input.swap_timings.gpu_started_draw = AtMs(9);
  input.swap_timings.swap_start = AtMs(12);
  input.swap_timings.swap_end = AtMs(14);
  input.presentation_feedback.timestamp = AtMs(16);

  FrameTimingDetails output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::FrameTimingDetails>(
      input, output));

  EXPECT_EQ(input.received_compositor_frame_timestamp,
            output.received_compositor_frame_timestamp);
  EXPECT_EQ(input.draw_start_timestamp, output.draw_start_timestamp);
  EXPECT_EQ(input.swap_timings.swap_start, output.swap_timings.swap_start);
  EXPECT_EQ(input.swap_timings.swap_end, output.swap_timings.swap_end);
  EXPECT_EQ(input.swap_timings.viz_scheduled_draw,
            output.swap_timings.viz_scheduled_draw);
  EXPECT_EQ(input.swap_timings.gpu_task_ready,
            output.swap_timings.gpu_task_ready);
  EXPECT_EQ(input.swap_timings.gpu_started_draw,
            output.swap_timings.gpu_started_draw);
  EXPECT_EQ(input.presentation_feedback.timestamp,
            output.presentation_feedback.timestamp);
}

}  // namespace
}  // namespace viz
