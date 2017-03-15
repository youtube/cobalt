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
// 2K:        2,048 x 1,080
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

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
