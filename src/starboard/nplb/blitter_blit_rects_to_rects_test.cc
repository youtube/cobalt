// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/blitter.h"
#include "starboard/nplb/blitter_helpers.h"
#include "starboard/window.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_HAS(BLITTER)

namespace starboard {
namespace nplb {
namespace {

const int kWidth = 128;
const int kHeight = 128;

const int kNumRects = 2;

struct Rects {
  SbBlitterRect source_rects[kNumRects];
  SbBlitterRect dest_rects[kNumRects];
};

Rects GetArbitraryValidRects() {
  Rects rects;
  rects.source_rects[0] = SbBlitterMakeRect(0, 0, kWidth / 2, kHeight / 2);
  rects.source_rects[1] = SbBlitterMakeRect(0, 0, kWidth, kHeight);
  rects.dest_rects[0] = SbBlitterMakeRect(0, 0, kWidth, kHeight);
  rects.dest_rects[1] = SbBlitterMakeRect(0, 0, kWidth / 2, kHeight / 2);
  return rects;
}

TEST(SbBlitterBlitRectsToRectsTest, SunnyDay) {
  ContextTestEnvironment env(kWidth, kHeight);

  ASSERT_TRUE(SbBlitterSetRenderTarget(env.context(), env.render_target()));

  SbBlitterSurface surface =
      CreateArbitraryRenderTargetSurface(env.device(), kWidth, kHeight);

  // Issue a blit command and flush a draw command.
  Rects rects = GetArbitraryValidRects();

  EXPECT_TRUE(SbBlitterBlitRectsToRects(
      env.context(), surface, rects.source_rects, rects.dest_rects, kNumRects));

  EXPECT_TRUE(SbBlitterFlushContext(env.context()));

  EXPECT_TRUE(SbBlitterDestroySurface(surface));
}

TEST(SbBlitterBlitRectsToRectsTest, RainyDayInvalidContext) {
  ContextTestEnvironment env(kWidth, kHeight);

  ASSERT_TRUE(SbBlitterSetRenderTarget(env.context(), env.render_target()));

  SbBlitterSurface surface =
      CreateArbitraryRenderTargetSurface(env.device(), kWidth, kHeight);

  // Blitter commands will not succeed when the context is invalid.
  Rects rects = GetArbitraryValidRects();

  EXPECT_FALSE(SbBlitterBlitRectsToRects(kSbBlitterInvalidContext, surface,
                                         rects.source_rects, rects.dest_rects,
                                         kNumRects));

  EXPECT_TRUE(SbBlitterDestroySurface(surface));
}

TEST(SbBlitterBlitRectsToRectsTest, RainyDayNoRenderTarget) {
  ContextTestEnvironment env(kWidth, kHeight);

  SbBlitterSurface surface =
      CreateArbitraryRenderTargetSurface(env.device(), kWidth, kHeight);

  // Blit commands will not succeed when there is no render target specified
  // on the context.
  Rects rects = GetArbitraryValidRects();

  EXPECT_FALSE(SbBlitterBlitRectsToRects(
      env.context(), surface, rects.source_rects, rects.dest_rects, kNumRects));

  EXPECT_TRUE(SbBlitterDestroySurface(surface));
}

TEST(SbBlitterBlitRectsToRectsTest, RainyDayInvalidSourceSurface) {
  ContextTestEnvironment env(kWidth, kHeight);

  ASSERT_TRUE(SbBlitterSetRenderTarget(env.context(), env.render_target()));

  // Blit commands will not succeed when the source surface is invalid.
  Rects rects = GetArbitraryValidRects();

  EXPECT_FALSE(SbBlitterBlitRectsToRects(
      env.context(), kSbBlitterInvalidSurface, rects.source_rects,
      rects.dest_rects, kNumRects));
}

TEST(SbBlitterBlitRectsToRectsTest, RainyDayZeroAreaSourceRect) {
  ContextTestEnvironment env(kWidth, kHeight);

  ASSERT_TRUE(SbBlitterSetRenderTarget(env.context(), env.render_target()));

  SbBlitterSurface surface =
      CreateArbitraryRenderTargetSurface(env.device(), kWidth, kHeight);

  // Blit commands will not succeed when any source rectangle has an area of 0.
  SbBlitterRect source_rects[2];
  SbBlitterRect dest_rects[2];
  source_rects[0] = SbBlitterMakeRect(0, 0, 0, 0);
  source_rects[1] = SbBlitterMakeRect(0, 0, kWidth, kHeight);
  dest_rects[0] = SbBlitterMakeRect(0, 0, kWidth, kHeight);
  dest_rects[1] = SbBlitterMakeRect(0, 0, kWidth / 2, kHeight / 2);

  EXPECT_FALSE(SbBlitterBlitRectsToRects(env.context(), surface, source_rects,
                                         dest_rects, 2));

  EXPECT_TRUE(SbBlitterDestroySurface(surface));
}

TEST(SbBlitterBlitRectsToRectsTest, RainyDayOutOfBoundsSourceRect) {
  ContextTestEnvironment env(kWidth, kHeight);

  ASSERT_TRUE(SbBlitterSetRenderTarget(env.context(), env.render_target()));

  SbBlitterSurface surface =
      CreateArbitraryRenderTargetSurface(env.device(), kWidth, kHeight);

  // Blit commands will not succeed when any source rectangle is outside of
  // the source surface's bounds.
  SbBlitterRect source_rects[2];
  SbBlitterRect dest_rects[2];
  source_rects[0] = SbBlitterMakeRect(-1, -1, kWidth / 2, kHeight / 2);
  source_rects[1] = SbBlitterMakeRect(0, 0, kWidth, kHeight);
  dest_rects[0] = SbBlitterMakeRect(0, 0, kWidth, kHeight);
  dest_rects[1] = SbBlitterMakeRect(0, 0, kWidth / 2, kHeight / 2);

  EXPECT_FALSE(SbBlitterBlitRectsToRects(env.context(), surface, source_rects,
                                         dest_rects, 2));

  EXPECT_TRUE(SbBlitterDestroySurface(surface));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_HAS(BLITTER)
