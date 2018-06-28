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

const int kWidth = 32;
const int kHeight = 32;

// Tests that we can download pixel data from surfaces created via
// SbBlitterCreateRenderTargetSurface().
TEST(SbBlitterDownloadSurfacePixelsTest, SunnyDayRGBARenderTargetSurface) {
  SbBlitterDevice device = SbBlitterCreateDefaultDevice();
  ASSERT_TRUE(SbBlitterIsDeviceValid(device));

  if (!SbBlitterIsSurfaceFormatSupportedByRenderTargetSurface(
          device, kSbBlitterSurfaceFormatRGBA8)) {
    // If the device doesn't support RGBA surfaces as render targets, we cannot
    // proceed with this test.
    return;
  }
  SbBlitterSurface surface = SbBlitterCreateRenderTargetSurface(
      device, kWidth, kHeight, kSbBlitterSurfaceFormatRGBA8);
  ASSERT_TRUE(SbBlitterIsSurfaceValid(surface));

  SbBlitterRenderTarget render_target =
      SbBlitterGetRenderTargetFromSurface(surface);
  ASSERT_TRUE(SbBlitterIsRenderTargetValid(render_target));

  SbBlitterContext context = SbBlitterCreateContext(device);
  ASSERT_TRUE(SbBlitterIsContextValid(context));

  ASSERT_TRUE(SbBlitterSetRenderTarget(context, render_target));

  // Fill the top half of the surface to transparent green, and the bottom half
  // to opaque white.
  EXPECT_TRUE(SbBlitterSetBlending(context, false));
  EXPECT_TRUE(SbBlitterSetColor(context, SbBlitterColorFromRGBA(0, 1, 2, 255)));
  EXPECT_TRUE(
      SbBlitterFillRect(context, SbBlitterMakeRect(0, 0, kWidth, kHeight)));
  EXPECT_TRUE(SbBlitterFlushContext(context));

  uint8_t* downloaded_pixels = new uint8_t[kWidth * kHeight * 4];
  EXPECT_TRUE(SbBlitterDownloadSurfacePixels(
      surface, kSbBlitterPixelDataFormatRGBA8, kWidth * 4,
      static_cast<void*>(downloaded_pixels)));

  bool all_correct_color = true;
  for (int pixel = 0; pixel < kWidth * kHeight; ++pixel) {
    uint8_t* downloaded_pixel = downloaded_pixels + pixel * 4;
    all_correct_color &= downloaded_pixel[0] == 0;
    all_correct_color &= downloaded_pixel[1] == 1;
    all_correct_color &= downloaded_pixel[2] == 2;
    all_correct_color &= downloaded_pixel[3] == 255;
  }
  EXPECT_TRUE(all_correct_color);

  delete[] downloaded_pixels;

  EXPECT_TRUE(SbBlitterDestroyContext(context));
  EXPECT_TRUE(SbBlitterDestroySurface(surface));
  EXPECT_TRUE(SbBlitterDestroyDevice(device));
}

// Tests that we can download pixel data from surfaces created via
// SbBlitterCreateSurfaceFromPixelData().
TEST(SbBlitterDownloadSurfacePixelsTest, SunnyDayPixelDataSurface) {
  const int kWidth = 32;
  const int kHeight = 32;
  SbBlitterDevice device = SbBlitterCreateDefaultDevice();

  // A list of formats to test, assuming they are supported by the device.
  // This test only supports permutations of 32-bit RGBA formats.
  SbBlitterPixelDataFormat test_formats[] = {
      kSbBlitterPixelDataFormatARGB8, kSbBlitterPixelDataFormatBGRA8,
      kSbBlitterPixelDataFormatRGBA8,
  };

  for (size_t i = 0; i < SB_ARRAY_SIZE(test_formats); ++i) {
    if (!SbBlitterIsPixelFormatSupportedByPixelData(device, test_formats[i])) {
      continue;
    }

    SbBlitterPixelData pixel_data =
        SbBlitterCreatePixelData(device, kWidth, kHeight, test_formats[i]);
    ASSERT_TRUE(SbBlitterIsPixelDataValid(pixel_data));

    int pitch_in_bytes = SbBlitterGetPixelDataPitchInBytes(pixel_data);
    ASSERT_NE(-1, pitch_in_bytes);

    uint8_t* pixels =
        static_cast<uint8_t*>(SbBlitterGetPixelDataPointer(pixel_data));
    ASSERT_TRUE(pixels);

    // Fill the pixels with dummy data.
    for (int y = 0; y < kHeight; ++y) {
      for (int x = 0; x < kWidth; ++x) {
        pixels[x * 4 + 0] = 0;
        pixels[x * 4 + 1] = 1;
        pixels[x * 4 + 2] = 2;
        pixels[x * 4 + 3] = 3;
      }
      pixels += pitch_in_bytes;
    }

    // Create the surface.
    SbBlitterSurface surface =
        SbBlitterCreateSurfaceFromPixelData(device, pixel_data);
    EXPECT_TRUE(SbBlitterIsSurfaceValid(surface));

    // Now download the data and test that the downloaded data is as we
    // expect it to be.
    uint8_t* downloaded_pixels = new uint8_t[kWidth * kHeight * 4];
    EXPECT_TRUE(SbBlitterDownloadSurfacePixels(surface,
                                               kSbBlitterPixelDataFormatRGBA8,
                                               kWidth * 4, downloaded_pixels));

    bool all_correct_color = true;
    for (int pixel = 0; pixel < kWidth * kHeight; ++pixel) {
      uint8_t* downloaded_pixel = downloaded_pixels + pixel * 4;

      // Take into account the fact that the pixels may have been swizzeled
      // from their input format in order for them to be output as RGBA.
      switch (test_formats[i]) {
        case kSbBlitterPixelDataFormatBGRA8: {
          all_correct_color &= downloaded_pixel[0] == 2;
          all_correct_color &= downloaded_pixel[1] == 1;
          all_correct_color &= downloaded_pixel[2] == 0;
          all_correct_color &= downloaded_pixel[3] == 3;
        } break;
        case kSbBlitterPixelDataFormatARGB8: {
          all_correct_color &= downloaded_pixel[0] == 1;
          all_correct_color &= downloaded_pixel[1] == 2;
          all_correct_color &= downloaded_pixel[2] == 3;
          all_correct_color &= downloaded_pixel[3] == 0;
        } break;
        case kSbBlitterPixelDataFormatRGBA8: {
          all_correct_color &= downloaded_pixel[0] == 0;
          all_correct_color &= downloaded_pixel[1] == 1;
          all_correct_color &= downloaded_pixel[2] == 2;
          all_correct_color &= downloaded_pixel[3] == 3;
        } break;
        default: { SB_NOTREACHED(); }
      }
    }
    EXPECT_TRUE(all_correct_color);

    delete[] downloaded_pixels;

    EXPECT_TRUE(SbBlitterDestroySurface(surface));
  }

  ASSERT_TRUE(SbBlitterIsDeviceValid(device));
  EXPECT_TRUE(SbBlitterDestroyDevice(device));
}

TEST(SbBlitterDownloadSurfacePixelsTest, RainyDayInvalidSurface) {
  const int kWidth = 32;
  const int kHeight = 32;
  uint32_t* pixel_data = new uint32_t[kWidth * kHeight];
  EXPECT_FALSE(SbBlitterDownloadSurfacePixels(kSbBlitterInvalidSurface,
                                              kSbBlitterPixelDataFormatRGBA8,
                                              kWidth * 4, pixel_data));
  delete[] pixel_data;
}

TEST(SbBlitterDownloadSurfacePixelsTest, RainyDayNullDataPointer) {
  const int kWidth = 32;
  const int kHeight = 32;
  ContextTestEnvironment env(kWidth, kHeight);
  EXPECT_FALSE(SbBlitterDownloadSurfacePixels(
      env.surface(), kSbBlitterPixelDataFormatRGBA8, kWidth * 4, NULL));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_HAS(BLITTER)
