/*
 * Copyright 2017 The Cobalt Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/browser/memory_settings/calculations.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "cobalt/browser/memory_settings/constants.h"
#include "cobalt/browser/memory_settings/test_common.h"
#include "cobalt/browser/switches.h"
#include "starboard/common/log.h"
#include "starboard/memory.h"
#include "starboard/system.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace browser {
namespace memory_settings {

///////////////////////////////////
//
//            WIDTH | HEIGHT
// -------------------------
// 8K:        7,680 x 4,320
// Cinema 4K: 4,096 x 2,160
// 4k/UHD:    3,840 x 2,160
// 2K:        2,560 x 1,440
// WUXGA:     1,920 x 1,200
// 1080p:     1,920 x 1,080
// 720p:      1,280 x 720
// 480p:        640 x 480
enum StandardDisplaySettings {
  k8k,
  kCinema4k,
  kUHD4k,
  k2k,
  kWUXGA,
  k1080p,
  k720p,
  k480p
};

math::Size GetDimensions(StandardDisplaySettings enum_value) {
  switch (enum_value) {
    case k8k: {
      return math::Size(7680, 4320);
    }
    case kCinema4k: {
      return math::Size(4096, 2160);
    }
    case kUHD4k: {
      return math::Size(3840, 2160);
    }
    case k2k: {
      return math::Size(2560, 1440);
    }
    case kWUXGA: {
      return math::Size(1920, 1200);
    }
    case k1080p: {
      return math::Size(1920, 1080);
    }
    case k720p: {
      return math::Size(1280, 720);
    }
    case k480p: {
      return math::Size(640, 480);
    }
  }

  EXPECT_TRUE(false) << "Should not be reached. Unknown enum_value: "
                     << enum_value;
  return GetDimensions(k1080p);
}

// Tests the expectation that CalculateImageCacheSize() is a pure function
// (side effect free) and will produce the expected results when decoding to
// multi-plane image is enabled.
TEST(MemoryCalculations, CalculateImageCacheSizeWithDecodingToMultiPlane) {
  EXPECT_EQ(kMinImageCacheSizeWithDecodingToMultiPlane,
            CalculateImageCacheSize(GetDimensions(k720p), true));
  EXPECT_EQ(kImageCacheSize1080pWithDecodingToMultiPlane,
            CalculateImageCacheSize(GetDimensions(k1080p), true));
  EXPECT_EQ(kMaxImageCacheSize,
            CalculateImageCacheSize(GetDimensions(kUHD4k), true));

  // Expect that the floor is hit for smaller values.
  EXPECT_EQ(kMinImageCacheSizeWithDecodingToMultiPlane,
            CalculateImageCacheSize(GetDimensions(k480p), true));

  // Expect that the ceiling is hit for larger values.
  EXPECT_EQ(kMaxImageCacheSize,
            CalculateImageCacheSize(GetDimensions(kCinema4k), true));
  EXPECT_EQ(kMaxImageCacheSize,
            CalculateImageCacheSize(GetDimensions(k8k), true));
}

// Tests the expectation that CalculateImageCacheSize() is a pure function
// (side effect free) and will produce the expected results when decoding to
// multi-plane image is disabled.
TEST(MemoryCalculations, CalculateImageCacheSizeWithoutDecodingToMultiPlane) {
  EXPECT_EQ(kMinImageCacheSizeWithoutDecodingToMultiPlane,
            CalculateImageCacheSize(GetDimensions(k720p), false));
  EXPECT_EQ(kImageCacheSize1080pWithoutDecodingToMultiPlane,
            CalculateImageCacheSize(GetDimensions(k1080p), false));
  EXPECT_EQ(kMaxImageCacheSize,
            CalculateImageCacheSize(GetDimensions(kUHD4k), false));

  // Expect that the floor is hit for smaller values.
  EXPECT_EQ(kMinImageCacheSizeWithoutDecodingToMultiPlane,
            CalculateImageCacheSize(GetDimensions(k480p), false));

  // Expect that the ceiling is hit for larger values.
  EXPECT_EQ(kMaxImageCacheSize,
            CalculateImageCacheSize(GetDimensions(kCinema4k), false));
  EXPECT_EQ(kMaxImageCacheSize,
            CalculateImageCacheSize(GetDimensions(k8k), false));
}

// Tests the expectation that CalculateSkiaGlyphAtlasTextureSize() is a pure
// function (side effect free) and will produce expected results.
TEST(MemoryCalculations, CalculateSkiaGlyphAtlasTextureSize) {
  math::Size ui_dimensions;
  TextureDimensions atlas_texture_size;

  // Test that the small resolution of 480p produces a texture atlas
  // that is minimal.
  ui_dimensions = GetDimensions(k480p);
  atlas_texture_size = CalculateSkiaGlyphAtlasTextureSize(ui_dimensions);
  EXPECT_EQ(kMinSkiaGlyphTextureAtlasWidth, atlas_texture_size.width());
  EXPECT_EQ(kMinSkiaGlyphTextureAtlasHeight, atlas_texture_size.height());

  // Test that expected resolution of 1080p produces a 2048x2048
  // atlas texture.
  ui_dimensions = GetDimensions(k1080p);
  atlas_texture_size = CalculateSkiaGlyphAtlasTextureSize(ui_dimensions);
  EXPECT_EQ(2048, atlas_texture_size.width());
  EXPECT_EQ(2048, atlas_texture_size.height());

  // Test that expected resolution of 4k produces a 8192x4096
  // atlas texture.
  ui_dimensions = GetDimensions(kUHD4k);
  atlas_texture_size = CalculateSkiaGlyphAtlasTextureSize(ui_dimensions);
  EXPECT_EQ(8192, atlas_texture_size.width());
  EXPECT_EQ(4096, atlas_texture_size.height());

  // Test that ultra-high 8k resolution produces expected results.
  ui_dimensions = GetDimensions(k8k);
  atlas_texture_size = CalculateSkiaGlyphAtlasTextureSize(ui_dimensions);
  EXPECT_EQ(16384, atlas_texture_size.width());
  EXPECT_EQ(16384, atlas_texture_size.height());
}

TEST(MemoryCalculations, CalculateSoftwareSurfaceCacheSizeInBytes) {
  math::Size ui_resolution = GetDimensions(k720p);
  // Expect that a 720p resolution produces a 4mb SoftwareSurfaceCache.
  EXPECT_EQ(4 * 1024 * 1024,
            CalculateSoftwareSurfaceCacheSizeInBytes(ui_resolution));

  ui_resolution = GetDimensions(k1080p);
  // Expect that a 1080p resolution produces a 9mb SoftwareSurfaceCache.
  EXPECT_EQ(9 * 1024 * 1024,
            CalculateSoftwareSurfaceCacheSizeInBytes(ui_resolution));
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
