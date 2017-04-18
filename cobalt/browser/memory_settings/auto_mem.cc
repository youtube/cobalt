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

#include <string>
#include <vector>

#include "base/optional.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/stringprintf.h"

#include "cobalt/browser/memory_settings/build_settings.h"
#include "cobalt/browser/memory_settings/calculations.h"
#include "cobalt/browser/memory_settings/pretty_print.h"
#include "cobalt/browser/switches.h"

namespace cobalt {
namespace browser {
namespace memory_settings {
namespace {

// Creates the specified memory setting type and binds it to (1) command line
// or else (2) build setting or else (3) an auto_set value.
template <typename MemorySettingType, typename ValueType>
scoped_ptr<MemorySettingType> CreateMemorySetting(
    const char* setting_name, const CommandLine& cmd_line,
    const base::optional<ValueType>& build_setting,
    const ValueType& autoset_value) {
  scoped_ptr<MemorySettingType> output(new MemorySettingType(setting_name));

  // The value is set according to importance:
  // 1) Command line switches are the most important, so set those if they
  //    exist.
  if (cmd_line.HasSwitch(setting_name)) {
    std::string value = cmd_line.GetSwitchValueNative(setting_name);
    if (output->TryParseValue(MemorySetting::kCmdLine, value)) {
      return output.Pass();
    }
  }

  // 2) Is there a build setting? Then set to build_setting.
  if (build_setting) {
    output->set_value(MemorySetting::kBuildSetting, *build_setting);
  } else {
    // 3) Otherwise bind to the autoset_value.
    output->set_value(MemorySetting::kAutoSet, autoset_value);
  }
  return output.Pass();
}

void EnsureValuePositive(IntSetting* setting) {
  if (setting->value() < 0) {
    setting->set_value(setting->source_type(), 0);
  }
}

void EnsureValuePositive(DimensionSetting* setting) {
  if (setting->value().GetArea() < 0) {
    setting->set_value(setting->source_type(), math::Size(0, 0));
  }
}

}  // namespace

AutoMem::AutoMem(const math::Size& ui_resolution,
                 const CommandLine& command_line,
                 const BuildSettings& build_settings) {
  // Set the ImageCache
  image_cache_size_in_bytes_ = CreateMemorySetting<IntSetting, int64_t>(
      switches::kImageCacheSizeInBytes, command_line,
      build_settings.cobalt_image_cache_size_in_bytes,
      CalculateImageCacheSize(ui_resolution));
  EnsureValuePositive(image_cache_size_in_bytes_.get());

  // Set javascript gc threshold
  javascript_gc_threshold_in_bytes_ = CreateMemorySetting<IntSetting, int64_t>(
      switches::kJavaScriptGcThresholdInBytes, command_line,
      build_settings.javascript_garbage_collection_threshold_in_bytes,
      kDefaultJsGarbageCollectionThresholdSize);
  EnsureValuePositive(javascript_gc_threshold_in_bytes_.get());

  // Set skia_atlas_texture_dimensions
  skia_atlas_texture_dimensions_ =
      CreateMemorySetting<DimensionSetting, math::Size>(
          switches::kSkiaTextureAtlasDimensions, command_line,
          build_settings.skia_texture_atlas_dimensions,
          CalculateSkiaGlyphAtlasTextureSize(ui_resolution));
  // Not available for non-blitter platforms.
  if (build_settings.has_blitter) {
    skia_atlas_texture_dimensions_->set_value(MemorySetting::kNotApplicable,
                                              math::Size(0, 0));
  }
  EnsureValuePositive(skia_atlas_texture_dimensions_.get());

  // Set skia_cache_size_in_bytes
  skia_cache_size_in_bytes_ = CreateMemorySetting<IntSetting, int64_t>(
      switches::kSkiaCacheSizeInBytes, command_line,
      build_settings.skia_cache_size_in_bytes,
      CalculateSkiaCacheSize(ui_resolution));
  // Not available for blitter platforms.
  if (build_settings.has_blitter) {
    skia_cache_size_in_bytes_->set_value(MemorySetting::kNotApplicable, 0);
  }
  EnsureValuePositive(skia_cache_size_in_bytes_.get());

  // Set software_surface_cache_size_in_bytes
  software_surface_cache_size_in_bytes_ =
      CreateMemorySetting<IntSetting, int64_t>(
          switches::kSoftwareSurfaceCacheSizeInBytes, command_line,
          build_settings.software_surface_cache_size_in_bytes,
          CalculateSoftwareSurfaceCacheSizeInBytes(ui_resolution));
  // Blitter only feature.
  if (!build_settings.has_blitter) {
    software_surface_cache_size_in_bytes_->set_value(
        MemorySetting::kNotApplicable, 0);
  }
  EnsureValuePositive(software_surface_cache_size_in_bytes_.get());
}

AutoMem::~AutoMem() {}

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

std::vector<const MemorySetting*> AutoMem::AllMemorySetttings() const {
  std::vector<const MemorySetting*> all_settings;
  // Keep these in alphabetical order.
  all_settings.push_back(image_cache_size_in_bytes_.get());
  all_settings.push_back(javascript_gc_threshold_in_bytes_.get());
  all_settings.push_back(skia_atlas_texture_dimensions_.get());
  all_settings.push_back(skia_cache_size_in_bytes_.get());
  all_settings.push_back(software_surface_cache_size_in_bytes_.get());
  return all_settings;
}

std::string AutoMem::ToPrettyPrintString() const {
  std::vector<const MemorySetting*> all_settings = AllMemorySetttings();
  return GeneratePrettyPrintTable(all_settings);
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
