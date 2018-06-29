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

TEST(SbBlitterGetRenderTargetFromSurfaceTest, SunnyDay) {
  SbBlitterDevice device = SbBlitterCreateDefaultDevice();
  ASSERT_TRUE(SbBlitterIsDeviceValid(device));

  SbBlitterContext context = SbBlitterCreateContext(device);
  ASSERT_TRUE(SbBlitterIsContextValid(context));

  const int kWidth = 128;
  const int kHeight = 128;

  // Test that we can get a render target from all supported pixel formats.
  SurfaceFormats supported_formats =
      GetAllSupportedSurfaceFormatsForRenderTargetSurfaces(device);

  for (SurfaceFormats::const_iterator iter = supported_formats.begin();
       iter != supported_formats.end(); ++iter) {
    SbBlitterSurface surface =
        SbBlitterCreateRenderTargetSurface(device, kWidth, kHeight, *iter);
    ASSERT_TRUE(SbBlitterIsSurfaceValid(surface));

    SbBlitterRenderTarget render_target =
        SbBlitterGetRenderTargetFromSurface(surface);
    ASSERT_TRUE(SbBlitterIsRenderTargetValid(render_target));

    // Issue a few rendering calls to the surface to ensure its validity.
    EXPECT_TRUE(SbBlitterSetRenderTarget(context, render_target));
    EXPECT_TRUE(
        SbBlitterFillRect(context, SbBlitterMakeRect(0, 0, kWidth, kHeight)));
    EXPECT_TRUE(SbBlitterFlushContext(context));

    EXPECT_TRUE(SbBlitterDestroySurface(surface));
  }

  EXPECT_TRUE(SbBlitterDestroyContext(context));
  EXPECT_TRUE(SbBlitterDestroyDevice(device));
}

// Surfaces created from SbBlitterPixelData objects are not able to provide
// SbBlitterRenderTargets, and this test verifies that.
TEST(SbBlitterGetRenderTargetFromSurfaceTest,
     RainyDayCannotGetRenderTargetFromPixelDataSurfaces) {
  SbBlitterDevice device = SbBlitterCreateDefaultDevice();
  ASSERT_TRUE(SbBlitterIsDeviceValid(device));

  const int kWidth = 128;
  const int kHeight = 128;

  std::vector<SbBlitterPixelDataFormat> supported_formats =
      GetAllSupportedPixelFormatsForPixelData(device);
  for (std::vector<SbBlitterPixelDataFormat>::const_iterator iter =
           supported_formats.begin();
       iter != supported_formats.end(); ++iter) {
    SbBlitterPixelData pixel_data =
        SbBlitterCreatePixelData(device, kWidth, kHeight, *iter);
    ASSERT_TRUE(SbBlitterIsPixelDataValid(pixel_data));

    SbBlitterSurface surface =
        SbBlitterCreateSurfaceFromPixelData(device, pixel_data);
    ASSERT_TRUE(SbBlitterIsSurfaceValid(surface));

    SbBlitterRenderTarget render_target =
        SbBlitterGetRenderTargetFromSurface(surface);
    EXPECT_FALSE(SbBlitterIsRenderTargetValid(render_target));

    EXPECT_TRUE(SbBlitterDestroySurface(surface));
  }

  EXPECT_TRUE(SbBlitterDestroyDevice(device));
}

TEST(SbBlitterGetRenderTargetFromSurfaceTest, RainyDayInvalidSurface) {
  SbBlitterRenderTarget render_target =
      SbBlitterGetRenderTargetFromSurface(kSbBlitterInvalidSurface);
  EXPECT_FALSE(SbBlitterIsRenderTargetValid(render_target));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_HAS(BLITTER)
