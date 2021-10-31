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

#ifndef COBALT_BROWSER_MEMORY_SETTINGS_AUTO_MEM_SETTINGS_H_
#define COBALT_BROWSER_MEMORY_SETTINGS_AUTO_MEM_SETTINGS_H_

#include "base/command_line.h"
#include "base/optional.h"
#include "cobalt/browser/memory_settings/texture_dimensions.h"
#include "cobalt/math/size.h"
#include "starboard/types.h"

namespace cobalt {
namespace browser {
namespace memory_settings {

struct AutoMemSettings {
  enum Type {
    kTypeBuild,
    kTypeCommandLine,
  };

  explicit AutoMemSettings(Type type) : type(type), has_blitter(false) {}

  Type type;
  bool has_blitter;
  base::Optional<int64_t> cobalt_encoded_image_cache_size_in_bytes;
  base::Optional<int64_t> cobalt_image_cache_size_in_bytes;
  base::Optional<int64_t> remote_typeface_cache_capacity_in_bytes;
  base::Optional<int64_t> skia_cache_size_in_bytes;
  base::Optional<TextureDimensions> skia_texture_atlas_dimensions;
  base::Optional<int64_t> software_surface_cache_size_in_bytes;
  base::Optional<int64_t> offscreen_target_cache_size_in_bytes;

  base::Optional<int64_t> max_cpu_in_bytes;
  base::Optional<int64_t> max_gpu_in_bytes;
  base::Optional<int64_t> reduce_cpu_memory_by;
  base::Optional<int64_t> reduce_gpu_memory_by;
};

AutoMemSettings GetDefaultBuildSettings();
AutoMemSettings GetSettings(const base::CommandLine& command_line);

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_SETTINGS_AUTO_MEM_SETTINGS_H_
