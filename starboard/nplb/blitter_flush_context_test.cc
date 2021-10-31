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

TEST(SbBlitterFlushContextTest, SunnyDayTrivial) {
  SbBlitterDevice device = SbBlitterCreateDefaultDevice();
  ASSERT_TRUE(SbBlitterIsDeviceValid(device));

  SbBlitterContext context = SbBlitterCreateContext(device);
  ASSERT_TRUE(SbBlitterIsContextValid(context));

  // Check that flush succeeds despite there being no commands issued on the
  // context.
  EXPECT_TRUE(SbBlitterFlushContext(context));

  EXPECT_TRUE(SbBlitterDestroyContext(context));
  EXPECT_TRUE(SbBlitterDestroyDevice(device));
}

// Test that we can flush the context okay after issuing some draw commands.
TEST(SbBlitterFlushContextTest, SunnyDayWithDrawCalls) {
  SbBlitterDevice device = SbBlitterCreateDefaultDevice();
  ASSERT_TRUE(SbBlitterIsDeviceValid(device));

  SbBlitterContext context = SbBlitterCreateContext(device);
  ASSERT_TRUE(SbBlitterIsContextValid(context));

  // We need a render target in order to issue draw commands.
  const int kRenderTargetWidth = 128;
  const int kRenderTargetHeight = 128;
  SurfaceFormats formats =
      GetAllSupportedSurfaceFormatsForRenderTargetSurfaces(device);
  SbBlitterSurface surface = SbBlitterCreateRenderTargetSurface(
      device, kRenderTargetWidth, kRenderTargetHeight, formats[0]);
  ASSERT_TRUE(SbBlitterIsSurfaceValid(surface));

  SbBlitterRenderTarget render_target =
      SbBlitterGetRenderTargetFromSurface(surface);
  ASSERT_TRUE(SbBlitterIsRenderTargetValid(render_target));

  EXPECT_TRUE(SbBlitterSetRenderTarget(context, render_target));
  EXPECT_TRUE(SbBlitterFillRect(
      context,
      SbBlitterMakeRect(0, 0, kRenderTargetWidth, kRenderTargetHeight)));

  // Check that flush succeeds after some commands have been submitted.
  EXPECT_TRUE(SbBlitterFlushContext(context));

  EXPECT_TRUE(SbBlitterDestroySurface(surface));
  EXPECT_TRUE(SbBlitterDestroyContext(context));
  EXPECT_TRUE(SbBlitterDestroyDevice(device));
}

TEST(SbBlitterFlushContextTest, RainyDayInvalidContext) {
  EXPECT_FALSE(SbBlitterFlushContext(kSbBlitterInvalidContext));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_HAS(BLITTER)
