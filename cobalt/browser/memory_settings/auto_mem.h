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

#ifndef COBALT_BROWSER_MEMORY_SETTINGS_AUTO_MEM_H_
#define COBALT_BROWSER_MEMORY_SETTINGS_AUTO_MEM_H_

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "cobalt/browser/memory_settings/auto_mem_settings.h"
#include "cobalt/browser/memory_settings/memory_settings.h"
#include "cobalt/math/size.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace cobalt {
namespace browser {
namespace memory_settings {

// AutoMem is contains system memory settings. The command line is required to
// instantiate this object and after construction, all memory settings are
// calculated.
class AutoMem {
 public:
  explicit AutoMem(const math::Size& ui_resolution,
                   const AutoMemSettings& command_line_settings,
                   const AutoMemSettings& build_settings);
  ~AutoMem();

  const IntSetting* encoded_image_cache_size_in_bytes() const;
  const IntSetting* image_cache_size_in_bytes() const;
  // This setting represents all others cpu-memory consuming systems within
  // the engine. This value has been hard coded.
  const IntSetting* remote_typeface_cache_size_in_bytes() const;
  const DimensionSetting* skia_atlas_texture_dimensions() const;
  const IntSetting* skia_cache_size_in_bytes() const;
  const IntSetting* software_surface_cache_size_in_bytes() const;
  const IntSetting* offscreen_target_cache_size_in_bytes() const;

  // max_cpu/gpu_bytes represents the maximum amount of memory that should
  // be consumed by the engine. These values can be set by the command line
  // or else they are set automatically.
  const IntSetting* max_cpu_bytes() const;
  const IntSetting* max_gpu_bytes() const;

  // Generates a string table of the memory settings and available memory for
  // cpu and gpu. This is used during startup to display memory configuration
  // information.
  std::string ToPrettyPrintString(bool use_color_ascii) const;

  // Reports the total memory that all settings that match memory_type
  // consume.
  int64_t SumAllMemoryOfType(MemorySetting::MemoryType memory_type) const;

 private:
  void ConstructSettings(const math::Size& ui_resolution,
                         const AutoMemSettings& command_line_settings,
                         const AutoMemSettings& build_settings);

  // AllMemorySettings - does not include cpu & gpu max memory.
  std::vector<const MemorySetting*> AllMemorySettings() const;
  std::vector<MemorySetting*> AllMemorySettingsMutable();

  // All of the following are included in AllMemorySettings().
  std::unique_ptr<IntSetting> encoded_image_cache_size_in_bytes_;
  std::unique_ptr<IntSetting> image_cache_size_in_bytes_;
  std::unique_ptr<IntSetting> remote_typeface_cache_size_in_bytes_;
  std::unique_ptr<DimensionSetting> skia_atlas_texture_dimensions_;
  std::unique_ptr<IntSetting> skia_cache_size_in_bytes_;
  std::unique_ptr<IntSetting> software_surface_cache_size_in_bytes_;
  std::unique_ptr<IntSetting> offscreen_target_cache_size_in_bytes_;

  // These settings are used for constraining the memory and are NOT included
  // in AllMemorySettings().
  std::unique_ptr<IntSetting> max_cpu_bytes_;
  std::unique_ptr<IntSetting> max_gpu_bytes_;

  std::vector<std::string> error_msgs_;

  FRIEND_TEST(AutoMem, AllMemorySettingsAreOrderedByName);
  FRIEND_TEST(AutoMem, ConstrainedCPUEnvironment);
  FRIEND_TEST(AutoMem, ConstrainedGPUEnvironment);
  FRIEND_TEST(AutoMem, MaxCpuIsIgnoredDuringExplicitMemoryReduction);
};

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_SETTINGS_AUTO_MEM_H_
