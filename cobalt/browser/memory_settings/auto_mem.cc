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

#include "cobalt/browser/memory_settings/auto_mem.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/optional.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "cobalt/browser/memory_settings/build_settings.h"
#include "cobalt/browser/memory_settings/calculations.h"
#include "cobalt/browser/memory_settings/constants.h"
#include "cobalt/browser/memory_settings/memory_settings.h"
#include "cobalt/browser/memory_settings/pretty_print.h"
#include "cobalt/browser/switches.h"
#include "nb/lexical_cast.h"

namespace cobalt {
namespace browser {
namespace memory_settings {
namespace {

// Determines if the string value signals "autoset".
bool StringValueSignalsAutoset(const std::string& value) {
  std::string value_lower_case = value;
  std::transform(value_lower_case.begin(), value_lower_case.end(),
                 value_lower_case.begin(), ::tolower);
  bool is_autoset = (value_lower_case == "auto") ||
                    (value_lower_case == "autoset") ||
                    (value_lower_case == "-1");
  return is_autoset;
}

// Creates the specified memory setting type and binds it to (1) command line
// or else (2) build setting or else (3) an auto_set value.
template <typename MemorySettingType, typename ValueType>
scoped_ptr<MemorySettingType> CreateMemorySetting(
    const char* setting_name,
    const CommandLine& cmd_line,  // Optional.
    const base::optional<ValueType>& build_setting,
    const ValueType& autoset_value) {
  scoped_ptr<MemorySettingType> output(new MemorySettingType(setting_name));

  // True when the command line explicitly requests the variable to be autoset.
  bool force_autoset = false;

  // The value is set according to importance:
  // 1) Command line switches are the most important, so set those if they
  //    exist.
  if (cmd_line.HasSwitch(setting_name)) {
    std::string value = cmd_line.GetSwitchValueNative(setting_name);
    if (StringValueSignalsAutoset(value)) {
      force_autoset = true;
    } else if (output->TryParseValue(MemorySetting::kCmdLine, value)) {
      return output.Pass();
    }
  }

  // 2) Is there a build setting? Then set to build_setting, unless the command
  //    line specifies that it should be autoset.
  if (build_setting && !force_autoset) {
    output->set_value(MemorySetting::kBuildSetting, *build_setting);
  } else {
    // 3) Otherwise bind to the autoset_value.
    output->set_value(MemorySetting::kAutoSet, autoset_value);
  }
  return output.Pass();
}

scoped_ptr<IntSetting> CreateSystemMemorySetting(
    const char* setting_name,
    MemorySetting::MemoryType memory_type,
    const CommandLine& command_line,
    const base::optional<int64_t>& build_setting,
    const base::optional<int64_t>& starboard_value) {
  scoped_ptr<IntSetting> setting(new IntSetting(setting_name));
  setting->set_memory_type(memory_type);
  if (command_line.HasSwitch(setting_name)) {
    const std::string value = command_line.GetSwitchValueNative(setting_name);
    if (setting->TryParseValue(MemorySetting::kCmdLine, value)) {
      return setting.Pass();
    }
  }

  if (build_setting) {
    setting->set_value(MemorySetting::kBuildSetting, *build_setting);
    return setting.Pass();
  }

  if (starboard_value) {
    setting->set_value(MemorySetting::kStarboardAPI, *starboard_value);
    return setting.Pass();
  }

  setting->set_value(MemorySetting::kStarboardAPI, -1);
  return setting.Pass();
}

void EnsureValuePositive(IntSetting* setting) {
  if (setting->value() < 0) {
    setting->set_value(setting->source_type(), 0);
  }
}

void EnsureValuePositive(DimensionSetting* setting) {
  const TextureDimensions value = setting->value();
  if (value.width() < 0 || value.height() < 0 || value.bytes_per_pixel() < 0) {
    setting->set_value(setting->source_type(), TextureDimensions());
  }
}

void EnsureTwoBytesPerPixel(DimensionSetting* setting) {
  TextureDimensions value = setting->value();
  if (value.bytes_per_pixel() != 2) {
    LOG(ERROR) << "Only two bytes per pixel are allowed for setting: "
               << setting->name();
    value.set_bytes_per_pixel(2);
    setting->set_value(setting->source_type(), value);
  }
}

// Sums up the memory consumption of all memory settings with of the input
// memory type.
int64_t SumMemoryConsumption(
    MemorySetting::MemoryType memory_type_filter,
    const std::vector<const MemorySetting*>& memory_settings) {
  int64_t sum = 0;
  for (size_t i = 0; i < memory_settings.size(); ++i) {
    const MemorySetting* setting = memory_settings[i];
    if (memory_type_filter == setting->memory_type()) {
      sum += setting->MemoryConsumption();
    }
  }
  return sum;
}


// Creates the GPU setting.
// This setting is unique because it may not be defined by command line, or
// build. In this was, it can be unset.
scoped_ptr<IntSetting> CreateGpuSetting(const CommandLine& command_line,
                                        const BuildSettings& build_settings) {
  // Bind to the starboard api, if applicable.
  base::optional<int64_t> starboard_setting;
  if (SbSystemHasCapability(kSbSystemCapabilityCanQueryGPUMemoryStats)) {
    starboard_setting = SbSystemGetTotalGPUMemory();
  }

  scoped_ptr<IntSetting> gpu_setting =
      CreateSystemMemorySetting(
          switches::kMaxCobaltGpuUsage,
          MemorySetting::kGPU,
          command_line,
          build_settings.max_gpu_in_bytes,
          starboard_setting);

  EnsureValuePositive(gpu_setting.get());
  return gpu_setting.Pass();
}

scoped_ptr<IntSetting> CreateCpuSetting(const CommandLine& command_line,
                                        const BuildSettings& build_settings) {
  scoped_ptr<IntSetting> cpu_setting =
      CreateSystemMemorySetting(
          switches::kMaxCobaltGpuUsage,
          MemorySetting::kCPU,
          command_line,
          build_settings.max_cpu_in_bytes,
          SbSystemGetTotalCPUMemory());

  EnsureValuePositive(cpu_setting.get());
  return cpu_setting.Pass();
}

}  // namespace

AutoMem::AutoMem(const math::Size& ui_resolution,
                 const CommandLine& command_line,
                 const BuildSettings& build_settings) {
  max_cpu_bytes_ = CreateCpuSetting(command_line, build_settings);
  max_gpu_bytes_ = CreateGpuSetting(command_line, build_settings);

  // Set the ImageCache
  image_cache_size_in_bytes_ = CreateMemorySetting<IntSetting, int64_t>(
      switches::kImageCacheSizeInBytes,
      command_line,
      build_settings.cobalt_image_cache_size_in_bytes,
      CalculateImageCacheSize(ui_resolution));
  EnsureValuePositive(image_cache_size_in_bytes_.get());
  image_cache_size_in_bytes_->set_memory_type(MemorySetting::kGPU);

  // Set javascript gc threshold
  javascript_gc_threshold_in_bytes_ = CreateMemorySetting<IntSetting, int64_t>(
      switches::kJavaScriptGcThresholdInBytes,
      command_line,
      build_settings.javascript_garbage_collection_threshold_in_bytes,
      kDefaultJsGarbageCollectionThresholdSize);
  EnsureValuePositive(javascript_gc_threshold_in_bytes_.get());

  // Set the misc cobalt size to a specific size.
  misc_cobalt_cpu_size_in_bytes_.reset(
      new IntSetting("misc_cobalt_cpu_size_in_bytes"));
  misc_cobalt_cpu_size_in_bytes_->set_value(
      MemorySetting::kAutoSet, kMiscCobaltSizeInBytes);

  // Set remote_type_face_cache size.
  remote_typeface_cache_size_in_bytes_ =
      CreateMemorySetting<IntSetting, int64_t>(
        switches::kRemoteTypefaceCacheSizeInBytes,
        command_line,
        build_settings.remote_typeface_cache_capacity_in_bytes,
        kDefaultRemoteTypeFaceCacheSize);
  EnsureValuePositive(remote_typeface_cache_size_in_bytes_.get());

  // Set skia_atlas_texture_dimensions
  skia_atlas_texture_dimensions_ =
      CreateMemorySetting<DimensionSetting, TextureDimensions>(
          switches::kSkiaTextureAtlasDimensions,
          command_line,
          build_settings.skia_texture_atlas_dimensions,
          CalculateSkiaGlyphAtlasTextureSize(ui_resolution));
  // Not available for non-blitter platforms.
  if (build_settings.has_blitter) {
    skia_atlas_texture_dimensions_->set_memory_type(
        MemorySetting::kNotApplicable);
  } else {
    // Skia always uses gpu memory, when enabled.
    skia_atlas_texture_dimensions_->set_memory_type(MemorySetting::kGPU);
  }
  EnsureValuePositive(skia_atlas_texture_dimensions_.get());
  EnsureTwoBytesPerPixel(skia_atlas_texture_dimensions_.get());

  // Set skia_cache_size_in_bytes
  skia_cache_size_in_bytes_ = CreateMemorySetting<IntSetting, int64_t>(
      switches::kSkiaCacheSizeInBytes,
      command_line,
      build_settings.skia_cache_size_in_bytes,
      CalculateSkiaCacheSize(ui_resolution));
  // Not available for blitter platforms.
  if (build_settings.has_blitter) {
    skia_cache_size_in_bytes_->set_memory_type(MemorySetting::kNotApplicable);
  } else {
    // Skia always uses gpu memory, when enabled.
    skia_cache_size_in_bytes_->set_memory_type(MemorySetting::kGPU);
  }
  EnsureValuePositive(skia_cache_size_in_bytes_.get());

  // Set software_surface_cache_size_in_bytes
  software_surface_cache_size_in_bytes_ =
      CreateMemorySetting<IntSetting, int64_t>(
          switches::kSoftwareSurfaceCacheSizeInBytes,
          command_line,
          build_settings.software_surface_cache_size_in_bytes,
          CalculateSoftwareSurfaceCacheSizeInBytes(ui_resolution));
  // Blitter only feature.
  if (!build_settings.has_blitter) {
    software_surface_cache_size_in_bytes_->set_memory_type(
        MemorySetting::kNotApplicable);
  }
  EnsureValuePositive(software_surface_cache_size_in_bytes_.get());
}

AutoMem::~AutoMem() {}

const IntSetting* AutoMem::misc_engine_cpu_size_in_bytes() const {
  return misc_cobalt_cpu_size_in_bytes_.get();
}

const IntSetting* AutoMem::remote_typeface_cache_size_in_bytes() const {
  return remote_typeface_cache_size_in_bytes_.get();
}

const IntSetting* AutoMem::image_cache_size_in_bytes() const {
  return image_cache_size_in_bytes_.get();
}

const IntSetting* AutoMem::javascript_gc_threshold_in_bytes() const {
  return javascript_gc_threshold_in_bytes_.get();
}

const DimensionSetting* AutoMem::skia_atlas_texture_dimensions() const {
  return skia_atlas_texture_dimensions_.get();
}

const IntSetting* AutoMem::skia_cache_size_in_bytes() const {
  return skia_cache_size_in_bytes_.get();
}

const IntSetting* AutoMem::software_surface_cache_size_in_bytes() const {
  return software_surface_cache_size_in_bytes_.get();
}

std::vector<const MemorySetting*> AutoMem::AllMemorySettings() const {
  AutoMem* this_unconst = const_cast<AutoMem*>(this);
  std::vector<MemorySetting*> all_settings_mutable =
      this_unconst->AllMemorySettingsMutable();

  std::vector<const MemorySetting*> all_settings;
  all_settings.assign(all_settings_mutable.begin(),
                      all_settings_mutable.end());
  return all_settings;
}

// Make sure that this is the same as AllMemorySettings().
std::vector<MemorySetting*> AutoMem::AllMemorySettingsMutable() {
  std::vector<MemorySetting*> all_settings;
  // Keep these in alphabetical order.
  all_settings.push_back(image_cache_size_in_bytes_.get());
  all_settings.push_back(javascript_gc_threshold_in_bytes_.get());
  all_settings.push_back(misc_cobalt_cpu_size_in_bytes_.get());
  all_settings.push_back(remote_typeface_cache_size_in_bytes_.get());
  all_settings.push_back(skia_atlas_texture_dimensions_.get());
  all_settings.push_back(skia_cache_size_in_bytes_.get());
  all_settings.push_back(software_surface_cache_size_in_bytes_.get());
  return all_settings;
}

std::string AutoMem::ToPrettyPrintString() const {
  std::stringstream ss;
  std::vector<const MemorySetting*> all_settings = AllMemorySettings();
  ss << GeneratePrettyPrintTable(all_settings) << "\n";

  int64_t cpu_consumption =
      SumMemoryConsumption(MemorySetting::kCPU, all_settings);
  int64_t gpu_consumption =
      SumMemoryConsumption(MemorySetting::kGPU, all_settings);

  ss << GenerateMemoryTable(*max_cpu_bytes_, *max_gpu_bytes_,
                            cpu_consumption, gpu_consumption);

  std::string output_str = ss.str();
  return output_str;
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
