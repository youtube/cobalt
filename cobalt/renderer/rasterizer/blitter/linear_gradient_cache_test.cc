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

#include "cobalt/renderer/rasterizer/blitter/linear_gradient_cache.h"

#include <vector>

#include "cobalt/math/point_f.h"
#include "cobalt/render_tree/brush.h"
#include "cobalt/render_tree/color_rgba.h"
#include "starboard/blitter.h"
#include "starboard/configuration_constants.h"

#include "testing/gtest/include/gtest/gtest.h"

#if SB_HAS(BLITTER)

using cobalt::math::PointF;
using cobalt::render_tree::ColorRGBA;
using cobalt::render_tree::ColorStop;
using cobalt::render_tree::ColorStopList;
using cobalt::render_tree::LinearGradientBrush;

namespace {
const int kDefaultSurfaceSizeInPixels = 64;

struct TestData {
  TestData(LinearGradientBrush brush_arg, SbBlitterSurface surface_arg)
      : brush(brush_arg), surface(surface_arg) {}

  LinearGradientBrush brush;
  SbBlitterSurface surface;
};

float GenerateRandomFloatBetween0and1() {
  return (std::rand() / static_cast<float>(RAND_MAX));
}

SbBlitterSurface GenerateRandomSurface(SbBlitterDevice device, int width,
                                       int height,
                                       SbBlitterPixelDataFormat pixel_format) {
  SbBlitterPixelData pixel_data =
      SbBlitterCreatePixelData(device, width, height, pixel_format);

  if (!SbBlitterIsPixelDataValid(pixel_data)) return kSbBlitterInvalidSurface;

  SbBlitterSurface surface =
      SbBlitterCreateSurfaceFromPixelData(device, pixel_data);
  return surface;
}

TestData GenerateRandomTestData(const SbBlitterDevice &device, int width,
                                int height,
                                SbBlitterPixelDataFormat pixel_format) {
  bool horizontal_brush = (height == 1);
  if (horizontal_brush) {
    DCHECK_EQ(height, 1);
  } else {
    DCHECK_EQ(width, 1);
  }
  PointF source(20.0f, 20.0f);
  PointF dest(0.0f, 0.0f);
  // Gradient size is randomly chosen between -5.0 and 5.0
  float gradient_size = 2.0 * (GenerateRandomFloatBetween0and1() - 0.5) * 5.0f;
  if (fabs(gradient_size) < 0.1f) {
    // Try not to generate 0s.
    gradient_size = 0.1f;
  }

  if (horizontal_brush) {
    dest.set_x(source.x() + gradient_size);
    dest.set_y(source.y());
  } else {
    dest.set_x(source.x());
    dest.set_y(source.y() + gradient_size);
  }
  ColorStopList stops;
  ColorRGBA color1(GenerateRandomFloatBetween0and1(),
                   GenerateRandomFloatBetween0and1(),
                   GenerateRandomFloatBetween0and1());
  ColorRGBA color2(GenerateRandomFloatBetween0and1(),
                   GenerateRandomFloatBetween0and1(),
                   GenerateRandomFloatBetween0and1());
  stops.push_back(ColorStop(0.0, color1));
  stops.push_back(ColorStop(1.0, color2));
  LinearGradientBrush brush(source, dest, stops);
  SbBlitterSurface random_surface(
      GenerateRandomSurface(device, width, height, pixel_format));
  return TestData(brush, random_surface);
}
}  // namespace

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

class LinearGradientCacheFixture : public ::testing::Test {
 public:
  LinearGradientCacheFixture()
      : default_device(kSbBlitterInvalidDevice), pixel_format() {}

  virtual void SetUp() {
    pixel_format = kSbBlitterInvalidPixelDataFormat;
    switch (kSbPreferredRgbaByteOrder) {
      case SB_PREFERRED_RGBA_BYTE_ORDER_RGBA:
        pixel_format = kSbBlitterPixelDataFormatRGBA8;
        break;
      case SB_PREFERRED_RGBA_BYTE_ORDER_BGRA:
        pixel_format = kSbBlitterPixelDataFormatBGRA8;
        break;
      case SB_PREFERRED_RGBA_BYTE_ORDER_ARGB:
        pixel_format = kSbBlitterPixelDataFormatARGB8;
        break;
      default:
        NOTREACHED() << "Invalid SB_PREFERRED_RGBA_BYTE_ORDER_RGBA"
                     << SB_PREFERRED_RGBA_BYTE_ORDER_RGBA;
    }
    default_device = SbBlitterCreateDefaultDevice();
    ASSERT_TRUE(SbBlitterIsDeviceValid(default_device));
    ASSERT_TRUE(SbBlitterIsPixelFormatSupportedByPixelData(default_device,
                                                           pixel_format));
  }

  virtual void TearDown() {
    if (SbBlitterIsDeviceValid(default_device)) {
      SbBlitterDestroyDevice(default_device);
    }
  }

  TestData GenerateRandomTestData(bool horizontal_brush) {
    int width = 1;
    int height = 1;
    if (horizontal_brush) {
      width = kDefaultSurfaceSizeInPixels;
    } else {
      height = kDefaultSurfaceSizeInPixels;
    }

    return ::GenerateRandomTestData(default_device, width, height,
                                    pixel_format);
  }

  LinearGradientCache cache;
  SbBlitterDevice default_device;
  SbBlitterPixelDataFormat pixel_format;
};

TEST_F(LinearGradientCacheFixture, CheckConstructDestruct) {}

TEST_F(LinearGradientCacheFixture, DoNotCacheInvalidSurface) {
  math::PointF source(0.0f, 0.0f);
  math::PointF dest(15.0f, 20.0f);
  ColorStopList stops;
  ColorRGBA color1(0.8, 1.0, 0.5);
  ColorRGBA color2(0.6, 1.0, 0.5);
  stops.push_back(ColorStop(0.0, color1));
  stops.push_back(ColorStop(1.0, color2));
  LinearGradientBrush brush(source, dest, stops);
  SbBlitterSurface surface = kSbBlitterInvalidSurface;
  bool inserted_in_cache = cache.Put(brush, surface);
  EXPECT_FALSE(inserted_in_cache);
}

TEST_F(LinearGradientCacheFixture, PutSuccessful) {
  TestData test_data(GenerateRandomTestData(true));
  bool inserted_in_cache = cache.Put(test_data.brush, test_data.surface);
  EXPECT_TRUE(inserted_in_cache);
}

TEST_F(LinearGradientCacheFixture, ValidGet) {
  TestData test_data(GenerateRandomTestData(true));
  cache.Put(test_data.brush, test_data.surface);
  SbBlitterSurface surface = cache.Get(test_data.brush);
  EXPECT_TRUE(SbBlitterIsSurfaceValid(surface));
}

TEST_F(LinearGradientCacheFixture, InvalidGet) {
  TestData test_data_1(GenerateRandomTestData(true));
  TestData test_data_2(GenerateRandomTestData(false));
  cache.Put(test_data_1.brush, test_data_1.surface);
  SbBlitterSurface surface = cache.Get(test_data_2.brush);
  SbBlitterDestroySurface(test_data_2.surface);
  EXPECT_FALSE(SbBlitterIsSurfaceValid(surface));
}

TEST_F(LinearGradientCacheFixture, FetchTwoItems) {
  TestData test_data_1(GenerateRandomTestData(true));
  cache.Put(test_data_1.brush, test_data_1.surface);
  TestData test_data_2(GenerateRandomTestData(false));
  cache.Put(test_data_2.brush, test_data_2.surface);

  SbBlitterSurface surface1 = cache.Get(test_data_1.brush);
  EXPECT_TRUE(SbBlitterIsSurfaceValid(surface1));
  SbBlitterSurface surface2 = cache.Get(test_data_2.brush);
  EXPECT_TRUE(SbBlitterIsSurfaceValid(surface2));
}

TEST_F(LinearGradientCacheFixture, GetSameSizeGradientInSameDirection) {
  TestData test_data_1(GenerateRandomTestData(true));
  cache.Put(test_data_1.brush, test_data_1.surface);

  PointF source(test_data_1.brush.source());
  source.set_x(source.x() + 20);
  source.set_y(source.y() + 10);
  PointF dest(test_data_1.brush.dest());
  dest.set_x(dest.x() + 20);
  dest.set_y(dest.y() + 10);
  LinearGradientBrush similar_brush(source, dest,
                                    test_data_1.brush.color_stops());

  SbBlitterSurface surface1 = cache.Get(similar_brush);

  EXPECT_TRUE(SbBlitterIsSurfaceValid(surface1));
  EXPECT_TRUE(surface1 == test_data_1.surface);
}

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_HAS(BLITTER)
