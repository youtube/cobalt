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

#ifndef COBALT_BROWSER_MEMORY_SETTINGS_CONSTANTS_H_
#define COBALT_BROWSER_MEMORY_SETTINGS_CONSTANTS_H_

namespace cobalt {
namespace browser {
namespace memory_settings {

// These internal values are exposed for testing.
enum MemorySizes {
  // Size of the engine + unaccounted caches.
  // This was experimentally selected.
  kMiscCobaltCpuSizeInBytes = 119 * 1024 * 1024,

  // 1mb of encoded data is enough to avoid downloading new images during
  // suspend/resume.
  kDefaultEncodedImageCacheSize = 1024 * 1024,  // 1mb

  // Decoding image to multi-plane allows the decoded images to use 3/8 of
  // memory compare to their RGBA counter part due to the more compact nature of
  // the YUV images versus RGBA.  As most images in the image cache are jpegs,
  // this effectively reduces the image cache size requirements by 2.  So we
  // reduce the image cache size at 1080p and the minimum requirement of image
  // cache by half when decoding to multi-plane is enabled.

  // The image cache size when the output resolution is 1080p.  When calculating
  // the image cache size for other output resolutions, we scale the image cache
  // size at 1080p proportionally to the number of pixels of other resolutions.
  // So when the pixels are doubled, the image cache size is also doubled,
  // subject to clamping between minimum size and maximum size listed below.
  kImageCacheSize1080pWithDecodingToMultiPlane = 16 * 1024 * 1024,     // 16mb
  kImageCacheSize1080pWithoutDecodingToMultiPlane = 32 * 1024 * 1024,  // 32mb

  kMinImageCacheSizeWithDecodingToMultiPlane = 10 * 1024 * 1024,     // 10mb
  kMinImageCacheSizeWithoutDecodingToMultiPlane = 20 * 1024 * 1024,  // 20mb

  kMaxImageCacheSize = 64 * 1024 * 1024,  // 64mb

  kMinSkiaGlyphTextureAtlasWidth = 2048,
  kMinSkiaGlyphTextureAtlasHeight = 2048,
  kSkiaGlyphAtlasTextureBytesPerPixel = 2,
  kDefaultRemoteTypeFaceCacheSize = 4 * 1024 * 1024,           // 4mb
  kDefaultJsGarbageCollectionThresholdSize = 8 * 1024 * 1024,  // 8mb

  kMinSkiaCacheSize = 4 * 1024 * 1024,  // 4mb
};

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_SETTINGS_CONSTANTS_H_
