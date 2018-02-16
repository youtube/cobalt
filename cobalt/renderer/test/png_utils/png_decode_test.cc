// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "base/file_path.h"
#include "base/path_service.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/renderer/test/png_utils/png_decode.h"
#include "testing/gtest/include/gtest/gtest.h"

using cobalt::renderer::test::png_utils::DecodePNGToPremultipliedAlphaRGBA;
using cobalt::renderer::test::png_utils::DecodePNGToRGBA;

namespace {
FilePath GetPremultipliedAlphaTestImagePath() {
  FilePath data_directory;
  CHECK(PathService::Get(base::DIR_TEST_DATA, &data_directory));
  return data_directory.Append(FILE_PATH_LITERAL("test"))
      .Append(FILE_PATH_LITERAL("png_utils"))
      .Append(FILE_PATH_LITERAL("png_premultiplied_alpha_test_image.png"));
}

// Counts all pixels who's values differ from the passed in value
int CountDifferingPixels(const uint8_t* pixels, int width, int height,
                         uint32_t test_color) {
  // Number of pixels that differ from their expected value.
  int error_pixels = 0;

  // Iterate through each pixel testing that the value is the expected value.
  for (int row = 0; row < height; ++row) {
    const uint8_t* cur_pixel = pixels + width * row * 4;
    for (int col = 0; col < width; ++col) {
      uint32_t current_color =
          *(reinterpret_cast<const uint32_t*>(cur_pixel) + col);
      if (current_color != test_color) {
        ++error_pixels;
      }
    }
  }

  return error_pixels;
}

}  // namespace

// Test that we can decode the image properly into unpremultiplied alpha format.
TEST(PNGDecodeTests, CanDecodeUnpremultipliedAlpha) {
  int width;
  int height;
  scoped_array<uint8_t> pixels =
      DecodePNGToRGBA(GetPremultipliedAlphaTestImagePath(), &width, &height);

  // All pixels in the PNG image should have non-premultiplied alpha RGBA
  // values of (255, 0, 0, 127).
  uint32_t expected_color = 0;
  uint8_t* expected_color_components =
      reinterpret_cast<uint8_t*>(&expected_color);
  expected_color_components[0] = 255;
  expected_color_components[3] = 127;

  EXPECT_EQ(0,
            CountDifferingPixels(pixels.get(), width, height, expected_color));
}

// Test that we can decode the image properly into premultiplied alpha format.
TEST(PNGDecodeTests, CanDecodePremultipliedAlpha) {
  int width;
  int height;
  scoped_array<uint8_t> pixels = DecodePNGToPremultipliedAlphaRGBA(
      GetPremultipliedAlphaTestImagePath(), &width, &height);

  // All pixels in the PNG image should have premultiplied alpha RGBA
  // values of (127, 0, 0, 127).
  uint32_t expected_color = 0;
  uint8_t* expected_color_components =
      reinterpret_cast<uint8_t*>(&expected_color);
  expected_color_components[0] = 127;
  expected_color_components[3] = 127;

  EXPECT_EQ(0,
            CountDifferingPixels(pixels.get(), width, height, expected_color));
}
