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

#ifndef COBALT_BROWSER_MEMORY_SETTINGS_AUTO_MEM_H_
#define COBALT_BROWSER_MEMORY_SETTINGS_AUTO_MEM_H_

#include <map>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/browser/memory_settings/build_settings.h"
#include "cobalt/browser/memory_settings/memory_settings.h"
#include "cobalt/math/size.h"

namespace cobalt {
namespace browser {
namespace memory_settings {

// AutoMem is contains system memory settings. The command line is required to
// instantiate this object and after construction, all memory settings are
// calculated.
class AutoMem {
 public:
  explicit AutoMem(const math::Size& ui_resolution,
                   const CommandLine& command_line,
                   const BuildSettings& build_settings);
  ~AutoMem();

  const IntSetting* image_cache_size_in_bytes() const;
  const IntSetting* javascript_gc_threshold_in_bytes() const;
  const IntSetting* misc_engine_cpu_size_in_bytes() const;
  const IntSetting* remote_typeface_cache_size_in_bytes() const;
  const DimensionSetting* skia_atlas_texture_dimensions() const;
  const IntSetting* skia_cache_size_in_bytes() const;
  const IntSetting* software_surface_cache_size_in_bytes() const;

  std::vector<const MemorySetting*> AllMemorySettings() const;
  std::vector<MemorySetting*> AllMemorySettingsMutable();

  // Generates a string table of the memory settings and available memory for
  // cpu and gpu. This is used during startup to display memory configuration
  // information.
  std::string ToPrettyPrintString() const;

 private:
  scoped_ptr<IntSetting> image_cache_size_in_bytes_;
  scoped_ptr<IntSetting> javascript_gc_threshold_in_bytes_;
  scoped_ptr<IntSetting> misc_cobalt_cpu_size_in_bytes_;
  scoped_ptr<IntSetting> remote_typeface_cache_size_in_bytes_;
  scoped_ptr<DimensionSetting> skia_atlas_texture_dimensions_;
  scoped_ptr<IntSetting> skia_cache_size_in_bytes_;
  scoped_ptr<IntSetting> software_surface_cache_size_in_bytes_;
};

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_SETTINGS_AUTO_MEM_H_
