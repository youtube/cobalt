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

TEST(SbBlitterSetColorTest, SunnyDayTrue) {
  const int kWidth = 128;
  const int kHeight = 128;
  ContextTestEnvironment env(kWidth, kHeight);

  ASSERT_TRUE(SbBlitterSetRenderTarget(env.context(), env.render_target()));

  // Set to red and flush a draw command.
  EXPECT_TRUE(
      SbBlitterSetColor(env.context(), SbBlitterColorFromRGBA(255, 0, 0, 255)));
  EXPECT_TRUE(SbBlitterFillRect(env.context(),
                                SbBlitterMakeRect(0, 0, kWidth, kHeight)));
  EXPECT_TRUE(SbBlitterFlushContext(env.context()));
}

TEST(SbBlitterSetColorTest, RainyDayInvalidContext) {
  const int kWidth = 128;
  const int kHeight = 128;
  ContextTestEnvironment env(kWidth, kHeight);

  EXPECT_FALSE(SbBlitterSetColor(kSbBlitterInvalidContext,
                                 SbBlitterColorFromRGBA(255, 0, 0, 255)));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_HAS(BLITTER)
