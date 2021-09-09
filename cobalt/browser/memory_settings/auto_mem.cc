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

#include "cobalt/browser/memory_settings/auto_mem.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/browser/memory_settings/auto_mem_settings.h"
#include "cobalt/browser/memory_settings/calculations.h"
#include "cobalt/browser/memory_settings/constants.h"
#include "cobalt/browser/memory_settings/memory_settings.h"
#include "cobalt/browser/memory_settings/pretty_print.h"
#include "cobalt/browser/memory_settings/scaling_function.h"
#include "cobalt/browser/switches.h"
#include "cobalt/configuration/configuration.h"
#include "cobalt/loader/image/image_decoder.h"
#include "cobalt/math/clamp.h"

namespace cobalt {
namespace browser {
namespace memory_settings {
namespace {

// Determines if the value signals "autoset".
template <typename ValueType>
bool SignalsAutoset(const ValueType& value) {
  return (value < 0);
}

template <>
bool SignalsAutoset(const TextureDimensions& value) {
  return value.IsAutoset();
}

template <typename MemorySettingType, typename ValueType>
void SetMemorySetting(const base::Optional<ValueType>& command_line_setting,
                      const base::Optional<ValueType>& build_setting,
                      const ValueType& autoset_value,
                      MemorySettingType* setting) {
  const std::string setting_name = setting->name();

  // True when the command line explicitly requests the variable to be autoset.
  bool force_autoset = false;

  // The value is set according to importance:
  // 1) Command line switches are the most important, so set those if they
  //    exist.
  if (command_line_setting) {
    if (!SignalsAutoset(*command_line_setting)) {
      setting->set_value(MemorySetting::kCmdLine, *command_line_setting);
      return;
    }

    force_autoset = true;
  }

  // 2) Is there a build setting? Then set to build_setting, unless the command
  //    line specifies that it should be autoset.
  if (build_setting && !force_autoset) {
    setting->set_value(MemorySetting::kBuildSetting, *build_setting);
  } else {
    // 3) Otherwise bind to the autoset_value.
    setting->set_value(MemorySetting::kAutoSet, autoset_value);
  }
}

// Creates the specified memory setting type and binds it to (1) command line or
// else (2) build setting or else (3) an auto_set value.
template <typename MemorySettingType, typename ValueType>
std::unique_ptr<MemorySettingType> CreateMemorySetting(
    const char* setting_name,
    const base::Optional<ValueType>& command_line_setting,
    const base::Optional<ValueType>& build_setting,
    const ValueType& autoset_value) {
  std::unique_ptr<MemorySettingType> output(
      new MemorySettingType(setting_name));
  SetMemorySetting(command_line_setting, build_setting, autoset_value,
                   output.get());
  return std::move(output);
}

std::unique_ptr<IntSetting> CreateSystemMemorySetting(
    const char* setting_name, MemorySetting::MemoryType memory_type,
    const base::Optional<int64_t>& command_line_setting,
    const base::Optional<int64_t>& build_setting,
    const base::Optional<int64_t>& starboard_value) {
  std::unique_ptr<IntSetting> setting(new IntSetting(setting_name));
  setting->set_memory_type(memory_type);
  if (command_line_setting) {
    setting->set_value(MemorySetting::kCmdLine, *command_line_setting);
    return setting;
  }

  if (build_setting) {
    setting->set_value(MemorySetting::kBuildSetting, *build_setting);
    return setting;
  }

  if (starboard_value) {
    setting->set_value(MemorySetting::kStarboardAPI, *starboard_value);
    return setting;
  }

  // This will mark the value as invalid.
  setting->set_value(MemorySetting::kUnset, -1);
  return setting;
}

void EnsureValuePositive(IntSetting* setting) {
  if (!setting->valid()) {
    return;
  }
  if (setting->value() < 0) {
    setting->set_value(setting->source_type(), 0);
  }
}

void EnsureValuePositive(DimensionSetting* setting) {
  if (!setting->valid()) {
    return;
  }
  const TextureDimensions value = setting->value();
  if (value.width() < 0 || value.height() < 0 || value.bytes_per_pixel() < 0) {
    setting->set_value(setting->source_type(), TextureDimensions());
  }
}

void EnsureTwoBytesPerPixel(DimensionSetting* setting) {
  if (!setting->valid()) {
    return;
  }
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
std::unique_ptr<IntSetting> CreateGpuSetting(
    const AutoMemSettings& command_line_settings,
    const AutoMemSettings& build_settings) {
  // Bind to the starboard api, if applicable.
  base::Optional<int64_t> starboard_setting;
  if (SbSystemHasCapability(kSbSystemCapabilityCanQueryGPUMemoryStats)) {
    starboard_setting = SbSystemGetTotalGPUMemory();
  }

  std::unique_ptr<IntSetting> gpu_setting = CreateSystemMemorySetting(
      switches::kMaxCobaltGpuUsage, MemorySetting::kGPU,
      command_line_settings.max_gpu_in_bytes, build_settings.max_gpu_in_bytes,
      starboard_setting);

  EnsureValuePositive(gpu_setting.get());
  return gpu_setting;
}

std::unique_ptr<IntSetting> CreateCpuSetting(
    const AutoMemSettings& command_line_settings,
    const AutoMemSettings& build_settings) {
  std::unique_ptr<IntSetting> cpu_setting = CreateSystemMemorySetting(
      switches::kMaxCobaltCpuUsage, MemorySetting::kCPU,
      command_line_settings.max_cpu_in_bytes, build_settings.max_cpu_in_bytes,
      SbSystemGetTotalCPUMemory());

  EnsureValuePositive(cpu_setting.get());
  return cpu_setting;
}

void CheckConstrainingValues(const MemorySetting& memory_setting) {
  const size_t kNumTestPoints = 10;

  std::vector<double> values;
  for (size_t i = 0; i < kNumTestPoints; ++i) {
    const double requested_constraining_value =
        static_cast<double>(i) / static_cast<double>(kNumTestPoints - 1);

    const double actual_constraining_value =
        memory_setting.ComputeAbsoluteMemoryScale(requested_constraining_value);

    values.push_back(actual_constraining_value);
  }

  DCHECK(base::STLIsSorted(values))
      << "Constrainer in " << memory_setting.name()
      << " does not produce "
         "monotonically decreasing values as input goes from 1.0 -> 0.0";
}

}  // namespace

AutoMem::AutoMem(const math::Size& ui_resolution,
                 const AutoMemSettings& command_line_settings,
                 const AutoMemSettings& build_settings) {
  TRACE_EVENT0("cobalt::browser", "AutoMem::AutoMem()");
  ConstructSettings(ui_resolution, command_line_settings, build_settings);

  std::vector<MemorySetting*> memory_settings = AllMemorySettingsMutable();
}

AutoMem::~AutoMem() {}

const IntSetting* AutoMem::remote_typeface_cache_size_in_bytes() const {
  return remote_typeface_cache_size_in_bytes_.get();
}

const IntSetting* AutoMem::encoded_image_cache_size_in_bytes() const {
  return encoded_image_cache_size_in_bytes_.get();
}

const IntSetting* AutoMem::image_cache_size_in_bytes() const {
  return image_cache_size_in_bytes_.get();
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

const IntSetting* AutoMem::offscreen_target_cache_size_in_bytes() const {
  return offscreen_target_cache_size_in_bytes_.get();
}

const IntSetting* AutoMem::max_cpu_bytes() const {
  return max_cpu_bytes_.get();
}

const IntSetting* AutoMem::max_gpu_bytes() const {
  return max_gpu_bytes_.get();
}

std::vector<const MemorySetting*> AutoMem::AllMemorySettings() const {
  AutoMem* this_unconst = const_cast<AutoMem*>(this);
  std::vector<MemorySetting*> all_settings_mutable =
      this_unconst->AllMemorySettingsMutable();

  std::vector<const MemorySetting*> all_settings;
  all_settings.assign(all_settings_mutable.begin(), all_settings_mutable.end());
  return all_settings;
}

// Make sure that this is the same as AllMemorySettings().
std::vector<MemorySetting*> AutoMem::AllMemorySettingsMutable() {
  std::vector<MemorySetting*> all_settings;
  // Keep these in alphabetical order.
  all_settings.push_back(encoded_image_cache_size_in_bytes_.get());
  all_settings.push_back(image_cache_size_in_bytes_.get());
  all_settings.push_back(offscreen_target_cache_size_in_bytes_.get());
  all_settings.push_back(remote_typeface_cache_size_in_bytes_.get());
  all_settings.push_back(skia_atlas_texture_dimensions_.get());
  all_settings.push_back(skia_cache_size_in_bytes_.get());
  all_settings.push_back(software_surface_cache_size_in_bytes_.get());
  return all_settings;
}

std::string AutoMem::ToPrettyPrintString(bool use_color_ascii) const {
  std::stringstream ss;

  ss << "AutoMem:\n";
  std::vector<const MemorySetting*> all_settings = AllMemorySettings();
  ss << GeneratePrettyPrintTable(use_color_ascii, all_settings);

  int64_t cpu_consumption =
      SumMemoryConsumption(MemorySetting::kCPU, all_settings);
  int64_t gpu_consumption =
      SumMemoryConsumption(MemorySetting::kGPU, all_settings);

  ss << GenerateMemoryTable(use_color_ascii, *max_cpu_bytes_, *max_gpu_bytes_,
                            cpu_consumption, gpu_consumption);

  // Copy strings and optionally add more.
  std::vector<std::string> error_msgs = error_msgs_;

  if (max_cpu_bytes_->value() <= 0) {
    error_msgs.push_back("ERROR - max_cobalt_cpu_usage WAS 0 BYTES.");
  } else if (cpu_consumption > max_cpu_bytes_->value()) {
    error_msgs.push_back("ERROR - CPU CONSUMED WAS MORE THAN AVAILABLE.");
  }

  const base::Optional<int64_t> max_gpu_value =
      max_gpu_bytes_->optional_value();
  if (max_gpu_value) {
    if (*max_gpu_value <= 0) {
      error_msgs.push_back("ERROR - max_cobalt_gpu_usage WAS 0 BYTES.");
    } else if (gpu_consumption > *max_gpu_value) {
      error_msgs.push_back("ERROR - GPU CONSUMED WAS MORE THAN AVAILABLE.");
    }
  }

  // Stringify error messages.
  if (!error_msgs.empty()) {
    std::stringstream ss_error;
    ss_error << "AutoMem had errors:\n";
    for (size_t i = 0; i < error_msgs.size(); ++i) {
      ss_error << "   " << error_msgs[i] << "\n\n";
    }
    ss_error << "Please see cobalt/docs/memory_tuning.md "
                "for more information.";
    ss << MakeBorder(ss_error.str(), '*');
  }

  std::string output_str = ss.str();
  return output_str;
}

int64_t AutoMem::SumAllMemoryOfType(
    MemorySetting::MemoryType memory_type) const {
  return SumMemoryConsumption(memory_type, AllMemorySettings());
}

void AutoMem::ConstructSettings(const math::Size& ui_resolution,
                                const AutoMemSettings& command_line_settings,
                                const AutoMemSettings& build_settings) {
  max_cpu_bytes_ = CreateCpuSetting(command_line_settings, build_settings);
  max_gpu_bytes_ = CreateGpuSetting(command_line_settings, build_settings);

  // Set the encoded image cache capacity
  encoded_image_cache_size_in_bytes_ = CreateMemorySetting<IntSetting, int64_t>(
      switches::kEncodedImageCacheSizeInBytes,
      command_line_settings.cobalt_encoded_image_cache_size_in_bytes,
      build_settings.cobalt_encoded_image_cache_size_in_bytes,
      kDefaultEncodedImageCacheSize);
  EnsureValuePositive(encoded_image_cache_size_in_bytes_.get());

  // Set the ImageCache
  image_cache_size_in_bytes_ = CreateMemorySetting<IntSetting, int64_t>(
      switches::kImageCacheSizeInBytes,
      command_line_settings.cobalt_image_cache_size_in_bytes,
      build_settings.cobalt_image_cache_size_in_bytes,
      CalculateImageCacheSize(
          ui_resolution,
          loader::image::ImageDecoder::AllowDecodingToMultiPlane()));
  EnsureValuePositive(image_cache_size_in_bytes_.get());
  image_cache_size_in_bytes_->set_memory_type(MemorySetting::kGPU);
  // ImageCache releases memory linearly until a progress value of 75%, then
  // it will not reduce memory any more. It will also clamp at 100% and won't
  // be increased beyond that.
  image_cache_size_in_bytes_->set_memory_scaling_function(
      MakeLinearMemoryScaler(.75, 1.0));

  // Set remote_type_face_cache size.
  remote_typeface_cache_size_in_bytes_ =
      CreateMemorySetting<IntSetting, int64_t>(
          switches::kRemoteTypefaceCacheSizeInBytes,
          command_line_settings.remote_typeface_cache_capacity_in_bytes,
          build_settings.remote_typeface_cache_capacity_in_bytes,
          kDefaultRemoteTypeFaceCacheSize);
  EnsureValuePositive(remote_typeface_cache_size_in_bytes_.get());

  // Skia atlas texture dimensions.
  skia_atlas_texture_dimensions_.reset(new SkiaGlyphAtlasTextureSetting());
  SetMemorySetting(command_line_settings.skia_texture_atlas_dimensions,
                   build_settings.skia_texture_atlas_dimensions,
                   CalculateSkiaGlyphAtlasTextureSize(ui_resolution),
                   skia_atlas_texture_dimensions_.get());
  EnsureValuePositive(skia_atlas_texture_dimensions_.get());

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
      command_line_settings.skia_cache_size_in_bytes,
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
          command_line_settings.software_surface_cache_size_in_bytes,
          build_settings.software_surface_cache_size_in_bytes,
          CalculateSoftwareSurfaceCacheSizeInBytes(ui_resolution));
  // Blitter only feature.
  if (!build_settings.has_blitter) {
    software_surface_cache_size_in_bytes_->set_memory_type(
        MemorySetting::kNotApplicable);
  }
  EnsureValuePositive(software_surface_cache_size_in_bytes_.get());

  // Set offscreen_target_cache_size_in_bytes (relevant to the direct-gles
  // rasterizer).
  offscreen_target_cache_size_in_bytes_ =
      CreateMemorySetting<IntSetting, int64_t>(
          switches::kOffscreenTargetCacheSizeInBytes,
          command_line_settings.offscreen_target_cache_size_in_bytes,
          build_settings.offscreen_target_cache_size_in_bytes,
          CalculateOffscreenTargetCacheSizeInBytes(ui_resolution));
  offscreen_target_cache_size_in_bytes_->set_memory_scaling_function(
      MakeLinearMemoryScaler(0.25, 1.0));
  if (std::string(configuration::Configuration::GetInstance()
                      ->CobaltRasterizerType()) == "direct-gles") {
    offscreen_target_cache_size_in_bytes_->set_memory_type(MemorySetting::kGPU);
  } else {
    offscreen_target_cache_size_in_bytes_->set_memory_type(
        MemorySetting::kNotApplicable);
  }
  EnsureValuePositive(offscreen_target_cache_size_in_bytes_.get());

  // Final stage: Check that all constraining functions are monotonically
  // increasing.
  const std::vector<const MemorySetting*> all_memory_settings =
      AllMemorySettings();
  for (size_t i = 0; i < all_memory_settings.size(); ++i) {
    CheckConstrainingValues(*all_memory_settings[i]);
  }
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
