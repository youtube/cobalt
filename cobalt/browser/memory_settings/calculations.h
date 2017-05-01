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

#ifndef COBALT_BROWSER_MEMORY_SETTINGS_CALCULATIONS_H_
#define COBALT_BROWSER_MEMORY_SETTINGS_CALCULATIONS_H_

#include "base/optional.h"
#include "cobalt/browser/memory_settings/texture_dimensions.h"
#include "cobalt/math/size.h"
#include "starboard/types.h"

namespace cobalt {
namespace browser {
namespace memory_settings {

////////////////////////// Calculate Functions ////////////////////////////////
// These functions are exposed here for testing purposes and should not be used
// directly.
// Calculates the ImageCacheSize in bytes.
// The return ranges from [kMinImageCacheSize, kMaxImageCacheSize].
int64_t CalculateImageCacheSize(const math::Size& dimensions);

// Calculates the SkiaAtlasTextureSize.
// When the ui resolution is 1920x1080, then the returned atlas texture size
// will be 8192x4096. The texture will scale up and down, by powers of two,
// in relation to the input ui_resolution. The returned value will be clamped
// such that
// kMinSkiaTextureAtlasWidth <= output.width() <= kMaxSkiaTextureAtlasWidth
// and
// kMinSkiaTextureAtlasHeight <= output.height() <= kMaxSkiaTextureAtlasHeight
// will be true.
TextureDimensions CalculateSkiaGlyphAtlasTextureSize(
    const math::Size& ui_resolution);

// Calculates the SoftwareSurfaceCacheSize given the ui_resolution.
int64_t CalculateSoftwareSurfaceCacheSizeInBytes(
    const math::Size& ui_resolution);

// Calculates the SkiaCachSize from the ui_resolution. This is normalized
// to be 4MB @ 1080p and scales accordingly.
int64_t CalculateSkiaCacheSize(const math::Size& ui_resolution);

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_SETTINGS_CALCULATIONS_H_
