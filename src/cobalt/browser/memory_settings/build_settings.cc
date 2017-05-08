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

#include "cobalt/browser/memory_settings/build_settings.h"

#include "base/optional.h"
#include "cobalt/browser/memory_settings/constants.h"

namespace cobalt {
namespace browser {
namespace memory_settings {
namespace {
bool HasBlitter() {
#if SB_HAS(BLITTER)
  const bool has_blitter = true;
#else
  const bool has_blitter = false;
#endif
  return has_blitter;
}

base::optional<int64_t> MakeValidIfGreaterThanOrEqualToZero(int64_t value) {
  base::optional<int64_t> output;
  if (value >= 0) {
    output = value;
  }
  return output;
}

base::optional<TextureDimensions> MakeDimensionsIfValid(TextureDimensions td) {
  base::optional<TextureDimensions> output;
  if ((td.width() > 0) && (td.height() > 0) && td.bytes_per_pixel() > 0) {
    output = td;
  }
  return output;
}

}  // namespace

BuildSettings GetDefaultBuildSettings() {
  BuildSettings settings;
  settings.has_blitter = HasBlitter();

  settings.cobalt_image_cache_size_in_bytes =
      MakeValidIfGreaterThanOrEqualToZero(COBALT_IMAGE_CACHE_SIZE_IN_BYTES);
  settings.javascript_garbage_collection_threshold_in_bytes =
      MakeValidIfGreaterThanOrEqualToZero(
          COBALT_JS_GARBAGE_COLLECTION_THRESHOLD_IN_BYTES);
  settings.remote_typeface_cache_capacity_in_bytes =
      MakeValidIfGreaterThanOrEqualToZero(
          COBALT_REMOTE_TYPEFACE_CACHE_SIZE_IN_BYTES);
  settings.skia_cache_size_in_bytes =
      MakeValidIfGreaterThanOrEqualToZero(COBALT_SKIA_CACHE_SIZE_IN_BYTES);
  settings.skia_texture_atlas_dimensions =
      MakeDimensionsIfValid(
          TextureDimensions(COBALT_SKIA_GLYPH_ATLAS_WIDTH,
                            COBALT_SKIA_GLYPH_ATLAS_HEIGHT,
                            kSkiaGlyphAtlasTextureBytesPerPixel));
  settings.software_surface_cache_size_in_bytes =
      MakeValidIfGreaterThanOrEqualToZero(
          COBALT_SOFTWARE_SURFACE_CACHE_SIZE_IN_BYTES);

  settings.max_cpu_in_bytes =
      MakeValidIfGreaterThanOrEqualToZero(COBALT_MAX_CPU_USAGE_IN_BYTES);
  settings.max_gpu_in_bytes =
      MakeValidIfGreaterThanOrEqualToZero(COBALT_MAX_GPU_USAGE_IN_BYTES);
  return settings;
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
