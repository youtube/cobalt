// Copyright 2016 Google Inc. All Rights Reserved.
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

TEST(SbBlitterIsPixelFormatSupportedByDownloadSurfacePixels,
     SunnyDayRGBARenderTargetSurface) {
  SbBlitterDevice device = SbBlitterCreateDefaultDevice();
  ASSERT_TRUE(SbBlitterIsDeviceValid(device));

  const int kWidth = 128;
  const int kHeight = 128;

  // Test that every kind of surface can be downloaded in at least one format.
  PixelAndAlphaFormats supported_formats =
      GetAllSupportedPixelAndAlphaFormatsForPixelData(device);
  for (PixelAndAlphaFormats::const_iterator iter = supported_formats.begin();
       iter != supported_formats.end(); ++iter) {
    SbBlitterPixelData pixel_data = SbBlitterCreatePixelData(
        device, kWidth, kHeight, iter->pixel, iter->alpha);
    ASSERT_TRUE(SbBlitterIsPixelDataValid(pixel_data));

    SbBlitterSurface surface =
        SbBlitterCreateSurfaceFromPixelData(device, pixel_data);
    EXPECT_TRUE(SbBlitterIsSurfaceValid(surface));

    // Check every color and alpha format and verify that at least one of them
    // returns true.
    bool any_format_supported = false;
    for (int i = 0; i < kSbBlitterNumPixelDataFormats; ++i) {
      for (int j = 0; j < kSbBlitterNumAlphaFormats; ++j) {
        SbBlitterPixelDataFormat download_pixel_format =
            static_cast<SbBlitterPixelDataFormat>(i);
        SbBlitterAlphaFormat download_alpha_format =
            static_cast<SbBlitterAlphaFormat>(j);

        any_format_supported |=
            SbBlitterIsPixelFormatSupportedByDownloadSurfacePixels(
                surface, download_pixel_format, download_alpha_format);
      }
    }
    EXPECT_TRUE(any_format_supported);

    EXPECT_TRUE(SbBlitterDestroySurface(surface));
  }

  EXPECT_TRUE(SbBlitterDestroyDevice(device));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_HAS(BLITTER)
