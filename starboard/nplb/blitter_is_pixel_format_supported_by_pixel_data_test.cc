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

TEST(SbBlitterIsPixelFormatSupportedByPixelDataTest, SunnyDay) {
  SbBlitterDevice device = SbBlitterCreateDefaultDevice();
  ASSERT_TRUE(SbBlitterIsDeviceValid(device));

  // Check every color and alpha format and verify that at least one of them
  // returns true.
  bool any_format_supported = false;
  for (int i = 0; i < kSbBlitterNumPixelDataFormats; ++i) {
    any_format_supported |= SbBlitterIsPixelFormatSupportedByPixelData(
        device, static_cast<SbBlitterPixelDataFormat>(i));
  }
  EXPECT_TRUE(any_format_supported);

  EXPECT_TRUE(SbBlitterDestroyDevice(device));
}

TEST(SbBlitterIsPixelFormatSupportedByPixelDataTest, RainyDayInvalidDevice) {
  for (int i = 0; i < kSbBlitterNumPixelDataFormats; ++i) {
    EXPECT_FALSE(SbBlitterIsPixelFormatSupportedByPixelData(
        kSbBlitterInvalidDevice, static_cast<SbBlitterPixelDataFormat>(i)));
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_HAS(BLITTER)
