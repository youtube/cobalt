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
#include "testing/gtest/include/gtest/gtest.h"

#if SB_HAS(BLITTER)

namespace starboard {
namespace nplb {
namespace {

TEST(SbBlitterFillRectTest, SunnyDay) {
  const int kWidth = 128;
  const int kHeight = 128;
  ContextTestEnvironment env(kWidth, kHeight);

  ASSERT_TRUE(SbBlitterSetRenderTarget(env.context(), env.render_target()));

  // Setting a scissor rectangle with width/height smaller than the current
  // render target dimensions is okay.
  EXPECT_TRUE(
      SbBlitterSetScissor(env.context(), SbBlitterMakeRect(0, 0, 64, 64)));

  // Setting a scissor rectangle with non-zero x/y is okay.
  EXPECT_TRUE(
      SbBlitterSetScissor(env.context(), SbBlitterMakeRect(64, 64, 64, 64)));

  // Setting a scissor rectangle with width/height equal to the current render
  // target is okay.
  EXPECT_TRUE(
      SbBlitterSetScissor(env.context(), SbBlitterMakeRect(0, 0, 128, 128)));

  // Setting a scissor rectangle with width/height larger than the current
  // render target is okay.
  EXPECT_TRUE(
      SbBlitterSetScissor(env.context(), SbBlitterMakeRect(0, 0, 256, 256)));

  // Setting a scissor rectangle with negative x/y values is okay.
  EXPECT_TRUE(SbBlitterSetScissor(env.context(),
                                  SbBlitterMakeRect(-50, -50, 256, 256)));

  // Setting a 0-area scissor rectangle is okay.
  EXPECT_TRUE(
      SbBlitterSetScissor(env.context(), SbBlitterMakeRect(0, 0, 0, 0)));
}

TEST(SbBlitterFillRectTest, RainyDayInvalidContext) {
  const int kWidth = 128;
  const int kHeight = 128;
  ContextTestEnvironment env(kWidth, kHeight);

  ASSERT_TRUE(SbBlitterSetRenderTarget(env.context(), env.render_target()));
  EXPECT_FALSE(SbBlitterSetScissor(kSbBlitterInvalidContext,
                                   SbBlitterMakeRect(0, 0, kWidth, kHeight)));
}

TEST(SbBlitterFillRectTest, RainyDayNegativeWidthHeight) {
  const int kWidth = 128;
  const int kHeight = 128;
  ContextTestEnvironment env(kWidth, kHeight);

  // Setting the scissor will fail if the width or height are negative.
  EXPECT_FALSE(SbBlitterSetScissor(env.context(),
                                   SbBlitterMakeRect(0, 0, -kWidth, kHeight)));
  EXPECT_FALSE(SbBlitterSetScissor(env.context(),
                                   SbBlitterMakeRect(0, 0, kWidth, -kHeight)));
  EXPECT_FALSE(SbBlitterSetScissor(env.context(),
                                   SbBlitterMakeRect(0, 0, -kWidth, -kHeight)));
}

TEST(SbBlitterFillRectTest, RainyDayNoRenderTarget) {
  const int kWidth = 128;
  const int kHeight = 128;
  ContextTestEnvironment env(kWidth, kHeight);

  // Setting the scissor will fail if no render target has been previously set
  // on the current context.
  EXPECT_FALSE(SbBlitterSetScissor(env.context(),
                                   SbBlitterMakeRect(0, 0, kWidth, kHeight)));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_HAS(BLITTER)
