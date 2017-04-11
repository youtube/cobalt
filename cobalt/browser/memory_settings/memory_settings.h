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

#ifndef COBALT_BROWSER_MEMORY_SETTINGS_MEMORY_SETTINGS_H_
#define COBALT_BROWSER_MEMORY_SETTINGS_MEMORY_SETTINGS_H_

#include "base/optional.h"
#include "cobalt/math/size.h"
#include "starboard/types.h"

namespace cobalt {
namespace browser {
namespace memory_settings {

////////////////////////////// Get Functions //////////////////////////////////
// These Get*() functions are sensitive to build setting-overrides. In this
// case they are side-effect sensitive. If build setting-overrides are not
// in effect, then Get*() functions will call Calculate*() functions.

// Gets the ImageCacheSize in bytes from the given dimensions. If
// COBALT_IMAGE_CACHE_SIZE_IN_BYTES is defined, then this is the value
// that is returned, otherwise the value is generated via a call to
// CalculateImageCacheSize().
size_t GetImageCacheSize(const math::Size& dimensions);

// Gets the width and height of the skia atlas texture. Optionally applies
// overrides if they have been defined.
math::Size GetSkiaAtlasTextureSize(const math::Size& ui_resolution,
                                   const base::optional<math::Size> override);

// Gets the software surface cache. Applies overrides if they have been
// defined.
size_t GetSoftwareSurfaceCacheSizeInBytes(const math::Size& ui_resolution);

// Get's the SkiaCachSize. Optionally applies overrides if they have been
// defined.
size_t GetSkiaCacheSizeInBytes(const math::Size& ui_resolution);

uint32_t GetJsEngineGarbageCollectionThresholdInBytes();

////////////////////////// Calculate Functions ////////////////////////////////
// These functions are exposed here for testing purposes and should not be used
// directly.

// Calculates the ImageCacheSize in bytes.
// The return ranges from [kMinImageCacheSize, kMaxImageCacheSize].
size_t CalculateImageCacheSize(const math::Size& dimensions);

// Calculates the SkiaAtlasTextureSize.
// When the ui resolution is 1920x1080, then the returned atlas texture size
// will be 8192x4096. The texture will scale up and down, by powers of two,
// in relation to the input ui_resolution. The returned value will be clamped
// such that
// kMinSkiaTextureAtlasWidth <= output.width() <= kMaxSkiaTextureAtlasWidth
// and
// kMinSkiaTextureAtlasHeight <= output.height() <= kMaxSkiaTextureAtlasHeight
// will be true.
math::Size CalculateSkiaAtlasTextureSize(const math::Size& ui_resolution);

// Calculates the SoftwareSurfaceCacheSize given the ui_resolution.
size_t CalculateSoftwareSurfaceCacheSizeInBytes(
    const math::Size& ui_resolution);

// These internal values are exposed for testing.
enum MemorySizes {
  kMinImageCacheSize = 20 * 1024 * 1024,  // 20mb.
  kMaxImageCacheSize = 64 * 1024 * 1024,  // 64mb

  kMinSkiaTextureAtlasWidth = 2048,
  kMinSkiaTextureAtlasHeight = 2048,

  kMinSkiaCacheSize = 4 * 1024 * 1024,  // 4mb.
};

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_SETTINGS_MEMORY_SETTINGS_H_
