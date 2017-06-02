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

#ifndef COBALT_BROWSER_MEMORY_SETTINGS_BUILD_SETTINGS_H_
#define COBALT_BROWSER_MEMORY_SETTINGS_BUILD_SETTINGS_H_

#include "base/optional.h"
#include "cobalt/browser/memory_settings/texture_dimensions.h"
#include "cobalt/math/size.h"
#include "starboard/types.h"

namespace cobalt {
namespace browser {
namespace memory_settings {

struct BuildSettings {
  BuildSettings() : has_blitter(false), max_cpu_in_bytes(0) {}
  bool has_blitter;
  base::optional<int64_t> cobalt_image_cache_size_in_bytes;
  base::optional<int64_t> javascript_garbage_collection_threshold_in_bytes;
  base::optional<int64_t> remote_typeface_cache_capacity_in_bytes;
  base::optional<int64_t> skia_cache_size_in_bytes;
  base::optional<TextureDimensions> skia_texture_atlas_dimensions;
  base::optional<int64_t> software_surface_cache_size_in_bytes;

  base::optional<int64_t> max_cpu_in_bytes;
  base::optional<int64_t> max_gpu_in_bytes;
  base::optional<int64_t> reduce_cpu_memory_by;
  base::optional<int64_t> reduce_gpu_memory_by;
};

BuildSettings GetDefaultBuildSettings();

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_SETTINGS_BUILD_SETTINGS_H_
