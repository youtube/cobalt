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

TEST(SbBlitterBlitRectToRectTiledTest, SunnyDay) {
  ContextTestEnvironment env(kWidth, kHeight);

  ASSERT_TRUE(SbBlitterSetRenderTarget(env.context(), env.render_target()));

  SbBlitterSurface surface =
      CreateArbitraryRenderTargetSurface(env.device(), kWidth, kHeight);

  // Issue a blit command and flush a draw command.
  EXPECT_TRUE(SbBlitterBlitRectToRectTiled(
      env.context(), surface, SbBlitterMakeRect(0, 0, kWidth, kHeight),
      SbBlitterMakeRect(0, 0, kWidth, kHeight)));

  EXPECT_TRUE(SbBlitterFlushContext(env.context()));

  EXPECT_TRUE(SbBlitterDestroySurface(surface));
}

TEST(SbBlitterBlitRectToRectTiledTest, RainyDayInvalidContext) {
  ContextTestEnvironment env(kWidth, kHeight);

  ASSERT_TRUE(SbBlitterSetRenderTarget(env.context(), env.render_target()));

  SbBlitterSurface surface =
      CreateArbitraryRenderTargetSurface(env.device(), kWidth, kHeight);

  // Blitter commands will not succeed when the context is invalid.
  EXPECT_FALSE(
      SbBlitterBlitRectToRectTiled(kSbBlitterInvalidContext, surface,
                                   SbBlitterMakeRect(0, 0, kWidth, kHeight),
                                   SbBlitterMakeRect(0, 0, kWidth, kHeight)));

  EXPECT_TRUE(SbBlitterDestroySurface(surface));
}

TEST(SbBlitterBlitRectToRectTiledTest, RainyDayNoRenderTarget) {
  ContextTestEnvironment env(kWidth, kHeight);

  SbBlitterSurface surface =
      CreateArbitraryRenderTargetSurface(env.device(), kWidth, kHeight);

  // Blit commands will not succeed when there is no render target specified
  // on the context.
  EXPECT_FALSE(SbBlitterBlitRectToRectTiled(
      env.context(), surface, SbBlitterMakeRect(0, 0, kWidth, kHeight),
      SbBlitterMakeRect(0, 0, kWidth, kHeight)));

  EXPECT_TRUE(SbBlitterDestroySurface(surface));
}

TEST(SbBlitterBlitRectToRectTiledTest, RainyDayInvalidSourceSurface) {
  ContextTestEnvironment env(kWidth, kHeight);

  ASSERT_TRUE(SbBlitterSetRenderTarget(env.context(), env.render_target()));

  // Blit commands will not succeed when the source surface is invalid.
  EXPECT_FALSE(
      SbBlitterBlitRectToRectTiled(env.context(), kSbBlitterInvalidSurface,
                                   SbBlitterMakeRect(0, 0, kWidth, kHeight),
                                   SbBlitterMakeRect(0, 0, kWidth, kHeight)));
}

TEST(SbBlitterBlitRectToRectTiledTest, RainyDayZeroAreaSourceRect) {
  ContextTestEnvironment env(kWidth, kHeight);

  ASSERT_TRUE(SbBlitterSetRenderTarget(env.context(), env.render_target()));

  SbBlitterSurface surface =
      CreateArbitraryRenderTargetSurface(env.device(), kWidth, kHeight);

  // Blit commands will not succeed when the source rectangle has an area of 0.
  EXPECT_FALSE(SbBlitterBlitRectToRectTiled(
      env.context(), surface, SbBlitterMakeRect(0, 0, 0, 0),
      SbBlitterMakeRect(0, 0, kWidth, kHeight)));

  EXPECT_TRUE(SbBlitterDestroySurface(surface));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_HAS(BLITTER)
