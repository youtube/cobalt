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

TEST(SbBlitterCreateSwapChainFromWindowTest, SunnyDay) {
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
  EXPECT_TRUE(SbBlitterIsSwapChainValid(swap_chain));

  EXPECT_TRUE(SbBlitterDestroySwapChain(swap_chain));
  EXPECT_TRUE(SbBlitterDestroyDevice(device));
  EXPECT_TRUE(SbWindowDestroy(window));
}

TEST(SbBlitterCreateSwapChainFromWindowTest, SunnyDayMultipleTimes) {
  SbWindowOptions options;
  SbWindowSetDefaultOptions(&options);
  options.size.width = 1920;
  options.size.height = 1080;
  SbWindow window = SbWindowCreate(NULL);
  ASSERT_TRUE(SbWindowIsValid(window));

  SbBlitterDevice device = SbBlitterCreateDefaultDevice();
  ASSERT_TRUE(SbBlitterIsDeviceValid(device));

  for (int i = 0; i < kALot; ++i) {
    SbBlitterSwapChain swap_chain =
        SbBlitterCreateSwapChainFromWindow(device, window);
    EXPECT_TRUE(SbBlitterIsSwapChainValid(swap_chain));
    EXPECT_TRUE(SbBlitterDestroySwapChain(swap_chain));
  }

  EXPECT_TRUE(SbBlitterDestroyDevice(device));
  EXPECT_TRUE(SbWindowDestroy(window));
}

TEST(SbBlitterCreateSwapChainFromWindowTest, RainyDayBadWindow) {
  SbBlitterDevice device = SbBlitterCreateDefaultDevice();
  ASSERT_TRUE(SbBlitterIsDeviceValid(device));

  SbBlitterSwapChain swap_chain =
      SbBlitterCreateSwapChainFromWindow(device, kSbWindowInvalid);
  EXPECT_FALSE(SbBlitterIsSwapChainValid(swap_chain));

  EXPECT_TRUE(SbBlitterDestroyDevice(device));
}

TEST(SbBlitterCreateSwapChainFromWindowTest, RainyDayBadDevice) {
  SbWindowOptions options;
  SbWindowSetDefaultOptions(&options);
  options.size.width = 1920;
  options.size.height = 1080;
  SbWindow window = SbWindowCreate(NULL);
  ASSERT_TRUE(SbWindowIsValid(window));

  SbBlitterSwapChain swap_chain =
      SbBlitterCreateSwapChainFromWindow(kSbBlitterInvalidDevice, window);
  EXPECT_FALSE(SbBlitterIsSwapChainValid(swap_chain));

  EXPECT_TRUE(SbWindowDestroy(window));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_HAS(BLITTER)
