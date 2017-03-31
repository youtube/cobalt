/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/browser/memory_settings/memory_settings.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "base/logging.h"
#include "starboard/log.h"
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
      return math::Size(2048, 1080);
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

  EXPECT_TRUE(false)
    << "Should not be reached. Unknown enum_value: " << enum_value;
  return GetDimensions(k1080p);
}

// Tests the expectation that CalculateImageCacheSize() is a pure function
// (side effect free) and will produce the expected results.
TEST(MemorySettings, CalculateImageCacheSize) {
  EXPECT_EQ(kMinImageCacheSize, CalculateImageCacheSize(GetDimensions(k720p)));
  EXPECT_EQ(32 * 1024 * 1024,  // 32MB.
            CalculateImageCacheSize(GetDimensions(k1080p)));
  EXPECT_EQ(kMaxImageCacheSize, CalculateImageCacheSize(GetDimensions(kUHD4k)));

  // Expect that the floor is hit for smaller values.
  EXPECT_EQ(kMinImageCacheSize, CalculateImageCacheSize(GetDimensions(k480p)));

  // Expect that the ceiling is hit for larger values.
  EXPECT_EQ(kMaxImageCacheSize,
            CalculateImageCacheSize(GetDimensions(kCinema4k)));
  EXPECT_EQ(kMaxImageCacheSize, CalculateImageCacheSize(GetDimensions(k8k)));
}

// Tests the expectation that GetImageCacheSize() will change it's behavior
// when COBALT_IMAGE_CACHE_SIZE_IN_BYTES is defined to a value that is greater
// than -1, or will otherwise produce values identical values to
// CalculateImageCacheSize().
TEST(MemorySettings, GetImageCacheSize) {
  if (COBALT_IMAGE_CACHE_SIZE_IN_BYTES >= 0) {
    // Expect that regardless of the display size, the value returned will
    // be equal to COBALT_IMAGE_CACHE_SIZE_IN_BYTES
    EXPECT_EQ(COBALT_IMAGE_CACHE_SIZE_IN_BYTES,
              GetImageCacheSize(GetDimensions(k720p)));
    EXPECT_EQ(COBALT_IMAGE_CACHE_SIZE_IN_BYTES,
              GetImageCacheSize(GetDimensions(k1080p)));
    EXPECT_EQ(COBALT_IMAGE_CACHE_SIZE_IN_BYTES,
              GetImageCacheSize(GetDimensions(kUHD4k)));
  } else {
    // ... otherwise expect that the GetImageCacheSize() is equal to
    // CalculateImageCacheSize().
    EXPECT_EQ(CalculateImageCacheSize(GetDimensions(k720p)),
              GetImageCacheSize(GetDimensions(k720p)));
    EXPECT_EQ(CalculateImageCacheSize(GetDimensions(k1080p)),
              GetImageCacheSize(GetDimensions(k1080p)));
    EXPECT_EQ(CalculateImageCacheSize(GetDimensions(kUHD4k)),
              GetImageCacheSize(GetDimensions(kUHD4k)));
  }
}

// Tests the expectation that CalculateSkiaAtlasTextureSize() is a pure
// function (side effect free) and will produce expected results.
TEST(MemorySettings, CalculateSkiaAtlasTextureSize) {
  math::Size ui_dimensions;
  math::Size atlas_texture_size;

  // Test that the small resolution of 480p produces a texture atlas
  // that is minimal.
  ui_dimensions = GetDimensions(k480p);
  atlas_texture_size = CalculateSkiaAtlasTextureSize(ui_dimensions);
  EXPECT_EQ(kMinSkiaTextureAtlasWidth, atlas_texture_size.width());
  EXPECT_EQ(kMinSkiaTextureAtlasHeight, atlas_texture_size.height());

  // Test that expected resolution of 1080p produces a 2048x2048
  // atlas texture.
  ui_dimensions = GetDimensions(k1080p);
  atlas_texture_size = CalculateSkiaAtlasTextureSize(ui_dimensions);
  EXPECT_EQ(2048, atlas_texture_size.width());
  EXPECT_EQ(2048, atlas_texture_size.height());

  // Test that expected resolution of 4k produces a 8192x4096
  // atlas texture.
  ui_dimensions = GetDimensions(kUHD4k);
  atlas_texture_size = CalculateSkiaAtlasTextureSize(ui_dimensions);
  EXPECT_EQ(8192, atlas_texture_size.width());
  EXPECT_EQ(4096, atlas_texture_size.height());

  // Test that ultra-high 8k resolution produces expected results.
  ui_dimensions = GetDimensions(k8k);
  atlas_texture_size = CalculateSkiaAtlasTextureSize(ui_dimensions);
  EXPECT_EQ(16384, atlas_texture_size.width());
  EXPECT_EQ(16384, atlas_texture_size.height());
}

// Tests the expectation that GetImageCacheSize() will change it's behavior
// when COBALT_IMAGE_CACHE_SIZE_IN_BYTES is defined to a value that is greater
// than -1, or will otherwise produce values identical values to
// CalculateImageCacheSize().
//
// Tests the expectation that GetSkiaAtlasTextureSize() will change it's
// behavior when COBALT_SKIA_GLYPH_ATLAS_WIDTH and/or
// COBALT_SKIA_GLYPH_ATLAS_HEIGHT is defined to a value greater than -1, but
// will otherwise produce values identical to values to
// CalculateSkiaAtlasTextureSize().
TEST(MemorySettings, GetSkiaAtlasTextureSize) {
  std::vector<StandardDisplaySettings> display_sizes;
  display_sizes.push_back(k720p);
  display_sizes.push_back(k1080p);
  display_sizes.push_back(kUHD4k);

  for (size_t i = 0; i < display_sizes.size(); ++i) {
    math::Size ui_resolution = GetDimensions(display_sizes[i]);

    // Allows #defined based overrides.
    math::Size texture_atlas = GetSkiaAtlasTextureSize(ui_resolution);

    // Calculates without respect for overrides.
    math::Size reference_texture_atlas =
        CalculateSkiaAtlasTextureSize(ui_resolution);

    // (0) silences compiler warning about "dead code".
    if (COBALT_SKIA_GLYPH_ATLAS_WIDTH >= (0)) {
      // Expect that override is in effect.
      EXPECT_EQ(COBALT_SKIA_GLYPH_ATLAS_WIDTH, texture_atlas.width());
    } else {
      EXPECT_EQ(texture_atlas.width(), reference_texture_atlas.width());
    }

    if (COBALT_SKIA_GLYPH_ATLAS_HEIGHT >= (0)) {
      // Expect that override is in effect.
      EXPECT_EQ(COBALT_SKIA_GLYPH_ATLAS_HEIGHT, texture_atlas.height());
    } else {
      EXPECT_EQ(texture_atlas.height(), reference_texture_atlas.height());
    }
  }
}

TEST(MemorySettings, GetSoftwareSurfaceCacheSizeInBytes) {
  size_t skia_cache_size =
      GetSoftwareSurfaceCacheSizeInBytes(GetDimensions(k1080p));
#if COBALT_SKIA_CACHE_SIZE_IN_BYTES >= 0
  EXPECT_EQ(skia_cache_size, COBALT_SOFTWARE_SURFACE_CACHE_SIZE_IN_BYTES);
#else
  EXPECT_EQ(skia_cache_size, kDefaultSoftwareSurfaceCacheSize);
#endif
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
