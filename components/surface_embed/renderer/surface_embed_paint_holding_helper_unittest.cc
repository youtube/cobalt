// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/surface_embed/renderer/surface_embed_paint_holding_helper.h"

#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "cc/layers/surface_layer.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace surface_embed {

namespace {

viz::SurfaceId MakeSurfaceId(const viz::FrameSinkId& frame_sink_id,
                             uint32_t parent_sequence_number,
                             uint32_t child_sequence_number = 1u) {
  return viz::SurfaceId(
      frame_sink_id,
      viz::LocalSurfaceId(parent_sequence_number, child_sequence_number,
                          base::UnguessableToken::CreateForTesting(0, 1u)));
}

}  // namespace

class SurfaceEmbedPaintHoldingHelperTest : public testing::Test {
 public:
  SurfaceEmbedPaintHoldingHelperTest() {
    layer_ = cc::SurfaceLayer::Create();
    layer_->SetIsDrawable(true);
  }

  ~SurfaceEmbedPaintHoldingHelperTest() override = default;

  SurfaceEmbedPaintHoldingHelper& helper() { return helper_; }
  cc::SurfaceLayer& layer() { return *layer_; }
  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  scoped_refptr<cc::SurfaceLayer> layer_;
  SurfaceEmbedPaintHoldingHelper helper_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Verifies that the surface ID is initially invalid.
TEST_F(SurfaceEmbedPaintHoldingHelperTest, InitialState) {
  EXPECT_FALSE(helper().current_surface_id().is_valid());
}

// Verifies that SetSurfaceId updates the layer surface and tracks the current
// surface ID, and that paint holding is not set when allow_paint_holding is
// false.
TEST_F(SurfaceEmbedPaintHoldingHelperTest, SetSurfaceIdUpdatesSurface) {
  const viz::SurfaceId surface_id = MakeSurfaceId(viz::FrameSinkId(1, 1), 1);
  helper().SetSurfaceId(&layer(), surface_id, /*allow_paint_holding=*/false);

  EXPECT_EQ(surface_id, layer().surface_id());
  EXPECT_EQ(surface_id, helper().current_surface_id());
  EXPECT_FALSE(layer().oldest_acceptable_fallback());
}

// Verifies that paint holding sets the old surface as fallback and the timer
// clears it after kNewContentRenderingDelay.
TEST_F(SurfaceEmbedPaintHoldingHelperTest, PaintHoldingTimeout) {
  const viz::SurfaceId surface_id = MakeSurfaceId(viz::FrameSinkId(1, 1), 1);
  helper().SetSurfaceId(&layer(), surface_id, /*allow_paint_holding=*/false);
  EXPECT_EQ(surface_id, layer().surface_id());
  EXPECT_FALSE(layer().oldest_acceptable_fallback());

  const viz::SurfaceId new_surface_id =
      MakeSurfaceId(viz::FrameSinkId(1, 1), 2);
  helper().SetSurfaceId(&layer(), new_surface_id,
                        /*allow_paint_holding=*/true);

  // The new surface should be set on the layer.
  EXPECT_EQ(new_surface_id, layer().surface_id());
  EXPECT_EQ(new_surface_id, helper().current_surface_id());

  // The old surface should be set as the fallback.
  ASSERT_TRUE(layer().oldest_acceptable_fallback());
  EXPECT_EQ(surface_id, layer().oldest_acceptable_fallback().value());

  // After the timeout, the fallback should be cleared.
  task_environment().FastForwardUntilNoTasksRemain();
  EXPECT_EQ(new_surface_id, layer().surface_id());
  EXPECT_FALSE(layer().oldest_acceptable_fallback());
}

// Verifies that paint holding is not set when there is no old surface
// (initial attach).
TEST_F(SurfaceEmbedPaintHoldingHelperTest, NoFallbackOnInitialSurface) {
  const viz::SurfaceId surface_id = MakeSurfaceId(viz::FrameSinkId(1, 1), 1);
  helper().SetSurfaceId(&layer(), surface_id, /*allow_paint_holding=*/true);

  EXPECT_EQ(surface_id, layer().surface_id());
  // No old surface exists, so no fallback even with paint holding allowed.
  EXPECT_FALSE(layer().oldest_acceptable_fallback());
}

// Verifies that a rapid second SetSurfaceId stops the old timer and
// establishes a new fallback.
TEST_F(SurfaceEmbedPaintHoldingHelperTest, RepeatedSetSurfaceIdStopsOldTimer) {
  const viz::SurfaceId surface_1 = MakeSurfaceId(viz::FrameSinkId(1, 1), 1);
  const viz::SurfaceId surface_2 = MakeSurfaceId(viz::FrameSinkId(1, 1), 2);
  const viz::SurfaceId surface_3 = MakeSurfaceId(viz::FrameSinkId(1, 1), 3);

  // First transition: surface_1 -> surface_2 with paint holding.
  helper().SetSurfaceId(&layer(), surface_1, /*allow_paint_holding=*/false);
  helper().SetSurfaceId(&layer(), surface_2, /*allow_paint_holding=*/true);
  ASSERT_TRUE(layer().oldest_acceptable_fallback());
  EXPECT_EQ(surface_1, layer().oldest_acceptable_fallback().value());

  // Second transition: surface_2 -> surface_3 before the timer fires.
  helper().SetSurfaceId(&layer(), surface_3, /*allow_paint_holding=*/true);

  // The fallback should now be surface_2, not surface_1.
  ASSERT_TRUE(layer().oldest_acceptable_fallback());
  EXPECT_EQ(surface_2, layer().oldest_acceptable_fallback().value());

  // Timer fires, clears the fallback.
  task_environment().FastForwardUntilNoTasksRemain();
  EXPECT_EQ(surface_3, layer().surface_id());
  EXPECT_FALSE(layer().oldest_acceptable_fallback());
}

// Verifies that ClearPaintHolding stops the timer and clears the fallback.
TEST_F(SurfaceEmbedPaintHoldingHelperTest, ClearPaintHoldingOnCrash) {
  const viz::SurfaceId surface_1 = MakeSurfaceId(viz::FrameSinkId(1, 1), 1);
  const viz::SurfaceId surface_2 = MakeSurfaceId(viz::FrameSinkId(1, 1), 2);

  helper().SetSurfaceId(&layer(), surface_1, /*allow_paint_holding=*/false);
  helper().SetSurfaceId(&layer(), surface_2, /*allow_paint_holding=*/true);
  ASSERT_TRUE(layer().oldest_acceptable_fallback());

  // Simulate ChildProcessGone cleanup.
  helper().ClearPaintHolding(&layer());

  EXPECT_FALSE(layer().oldest_acceptable_fallback());
  EXPECT_FALSE(helper().current_surface_id().is_valid());

  // Timer should not fire since it was stopped.
  task_environment().FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(layer().oldest_acceptable_fallback());
}

// Verifies that setting the same surface ID is a no-op for paint holding.
TEST_F(SurfaceEmbedPaintHoldingHelperTest, SameSurfaceIdNoOp) {
  const viz::SurfaceId surface_1 = MakeSurfaceId(viz::FrameSinkId(1, 1), 1);
  const viz::SurfaceId surface_2 = MakeSurfaceId(viz::FrameSinkId(1, 1), 2);

  helper().SetSurfaceId(&layer(), surface_1, /*allow_paint_holding=*/false);
  helper().SetSurfaceId(&layer(), surface_2, /*allow_paint_holding=*/true);
  EXPECT_EQ(surface_2, layer().surface_id());
  ASSERT_TRUE(layer().oldest_acceptable_fallback());

  // Setting the same surface ID is a no-op. The early return in
  // SetSurfaceId skips all work when current_surface_id_ == surface_id.
  helper().SetSurfaceId(&layer(), surface_2, /*allow_paint_holding=*/false);
  EXPECT_EQ(surface_2, layer().surface_id());
  EXPECT_TRUE(layer().oldest_acceptable_fallback());
  task_environment().FastForwardUntilNoTasksRemain();
  EXPECT_EQ(surface_2, layer().surface_id());
  EXPECT_FALSE(layer().oldest_acceptable_fallback());
}

}  // namespace surface_embed
