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
#include "starboard/window.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_HAS(BLITTER)

namespace starboard {
namespace nplb {
namespace {

const int kALot = 10;

TEST(SbBlitterGetRenderTargetFromSwapChainTest, SunnyDay) {
  SbWindowOptions options;
  SbWindowSetDefaultOptions(&options);
  options.size.width = 1920;
  options.size.height = 1080;
  SbWindow window = SbWindowCreate(NULL);
  ASSERT_TRUE(SbWindowIsValid(window));

  SbBlitterDevice device = SbBlitterCreateDefaultDevice();
  ASSERT_TRUE(SbBlitterIsDeviceValid(device));

  SbBlitterSwapChain swap_chain =
      SbBlitterCreateSwapChainFromWindow(device, window);
  ASSERT_TRUE(SbBlitterIsSwapChainValid(swap_chain));

  SbBlitterRenderTarget render_target =
      SbBlitterGetRenderTargetFromSwapChain(swap_chain);
  EXPECT_TRUE(SbBlitterIsRenderTargetValid(render_target));

  EXPECT_TRUE(SbBlitterDestroySwapChain(swap_chain));
  EXPECT_TRUE(SbBlitterDestroyDevice(device));
  EXPECT_TRUE(SbWindowDestroy(window));
}

TEST(SbBlitterGetRenderTargetFromSwapChainTest, SunnyDayCanDraw) {
  SbWindowOptions options;
  SbWindowSetDefaultOptions(&options);
  options.size.width = 1920;
  options.size.height = 1080;
  SbWindow window = SbWindowCreate(NULL);
  ASSERT_TRUE(SbWindowIsValid(window));

  SbBlitterDevice device = SbBlitterCreateDefaultDevice();
  ASSERT_TRUE(SbBlitterIsDeviceValid(device));

  SbBlitterSwapChain swap_chain =
      SbBlitterCreateSwapChainFromWindow(device, window);
  ASSERT_TRUE(SbBlitterIsSwapChainValid(swap_chain));

  SbBlitterRenderTarget render_target =
      SbBlitterGetRenderTargetFromSwapChain(swap_chain);
  ASSERT_TRUE(SbBlitterIsRenderTargetValid(render_target));

  SbBlitterContext context = SbBlitterCreateContext(device);
  ASSERT_TRUE(SbBlitterIsContextValid(context));

  EXPECT_TRUE(SbBlitterSetRenderTarget(context, render_target));
  EXPECT_TRUE(SbBlitterFillRect(context, SbBlitterMakeRect(0, 0, 1920, 1080)));
  EXPECT_TRUE(SbBlitterFlushContext(context));

  EXPECT_TRUE(SbBlitterDestroyContext(context));
  EXPECT_TRUE(SbBlitterDestroySwapChain(swap_chain));
  EXPECT_TRUE(SbBlitterDestroyDevice(device));
  EXPECT_TRUE(SbWindowDestroy(window));
}

TEST(SbBlitterGetRenderTargetFromSwapChainTest, RainyDayInvalidSwapChain) {
  SbBlitterRenderTarget render_target =
      SbBlitterGetRenderTargetFromSwapChain(kSbBlitterInvalidSwapChain);
  EXPECT_FALSE(SbBlitterIsRenderTargetValid(render_target));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_HAS(BLITTER)
