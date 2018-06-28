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

TEST(SbBlitterCreateRenderTargetSurfaceTest, SunnyDay) {
  SbBlitterDevice device = SbBlitterCreateDefaultDevice();
  ASSERT_TRUE(SbBlitterIsDeviceValid(device));

  const int kWidth = 128;
  const int kHeight = 128;

  // Test that we can create a render target surface for all supported pixel
  // formats.
  SurfaceFormats supported_formats =
      GetAllSupportedSurfaceFormatsForRenderTargetSurfaces(device);

  for (SurfaceFormats::const_iterator iter = supported_formats.begin();
       iter != supported_formats.end(); ++iter) {
    SbBlitterSurface surface =
        SbBlitterCreateRenderTargetSurface(device, kWidth, kHeight, *iter);
    EXPECT_TRUE(SbBlitterIsSurfaceValid(surface));

    EXPECT_TRUE(SbBlitterDestroySurface(surface));
  }

  EXPECT_TRUE(SbBlitterDestroyDevice(device));
}

TEST(SbBlitterCreateRenderTargetSurfaceTest, RainyDayUnsupportedFormat) {
  SbBlitterDevice device = SbBlitterCreateDefaultDevice();
  ASSERT_TRUE(SbBlitterIsDeviceValid(device));

  const int kWidth = 128;
  const int kHeight = 128;

  // Test that we cannot create a render target surface for all unsupported
  // pixel formats.
  SurfaceFormats unsupported_formats =
      GetAllUnsupportedSurfaceFormatsForRenderTargetSurfaces(device);

  for (SurfaceFormats::const_iterator iter = unsupported_formats.begin();
       iter != unsupported_formats.end(); ++iter) {
    SbBlitterSurface surface =
        SbBlitterCreateRenderTargetSurface(device, kWidth, kHeight, *iter);
    EXPECT_FALSE(SbBlitterIsSurfaceValid(surface));
  }

  EXPECT_TRUE(SbBlitterDestroyDevice(device));
}

TEST(SbBlitterCreateRenderTargetSurfaceTest, RainyDayInvalidDevice) {
  SbBlitterDevice device = SbBlitterCreateDefaultDevice();
  ASSERT_TRUE(SbBlitterIsDeviceValid(device));

  const int kWidth = 128;
  const int kHeight = 128;

  // Test that we can create a render target surface for all supported pixel
  // formats.
  SurfaceFormats supported_formats =
      GetAllSupportedSurfaceFormatsForRenderTargetSurfaces(device);

  for (SurfaceFormats::const_iterator iter = supported_formats.begin();
       iter != supported_formats.end(); ++iter) {
    SbBlitterSurface surface = SbBlitterCreateRenderTargetSurface(
        kSbBlitterInvalidDevice, kWidth, kHeight, *iter);
    EXPECT_FALSE(SbBlitterIsSurfaceValid(surface));
  }

  EXPECT_TRUE(SbBlitterDestroyDevice(device));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_HAS(BLITTER)
