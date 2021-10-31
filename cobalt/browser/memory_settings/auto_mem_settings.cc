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

#include "cobalt/browser/memory_settings/auto_mem_settings.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "cobalt/browser/memory_settings/constants.h"
#include "cobalt/browser/switches.h"
#include "cobalt/configuration/configuration.h"
#include "starboard/blitter.h"

namespace cobalt {
namespace browser {
namespace memory_settings {
namespace {
bool HasBlitter() {
#if SB_API_VERSION < 12 && SB_HAS(BLITTER)
  const bool has_blitter = true;
#else
  const bool has_blitter = false;
#endif
  return has_blitter;
}

base::Optional<int64_t> MakeValidIfGreaterThanOrEqualToZero(int64_t value) {
  base::Optional<int64_t> output;
  if (value >= 0) {
    output = value;
  }
  return output;
}

base::Optional<TextureDimensions> MakeDimensionsIfValid(TextureDimensions td) {
  base::Optional<TextureDimensions> output;
  if ((td.width() > 0) && (td.height() > 0) && td.bytes_per_pixel() > 0) {
    output = td;
  }
  return output;
}

bool StringValueSignalsAutoset(const std::string& value) {
  return base::LowerCaseEqualsASCII(value, "auto") ||
         base::LowerCaseEqualsASCII(value, "autoset") || value == "-1";
}

struct ParsedIntValue {
 public:
  ParsedIntValue() : value_(0), error_(false) {}
  ParsedIntValue(const ParsedIntValue& other)
      : value_(other.value_), error_(other.error_) {}
  int value_;
  bool error_;  // true if there was a parse error.
};

// Parses a string like "1234x5678" to vector of parsed int values.
std::vector<ParsedIntValue> ParseDimensions(const std::string& input) {
  std::string value_str = base::ToLowerASCII(input);
  std::vector<ParsedIntValue> output;

  std::vector<std::string> lengths = base::SplitString(
      value_str, "x", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  for (size_t i = 0; i < lengths.size(); ++i) {
    ParsedIntValue parsed_value;
    parsed_value.error_ = !base::StringToInt(lengths[i], &parsed_value.value_);
    output.push_back(parsed_value);
  }
  return output;
}

// Handles bytes: "12435", "1234B"
// Handles kilobytes: "128KB"
// Handles megabytes: "64MB"
// Handles gigabytes: "1GB"
// Handles fractional units for kilo/mega/gigabytes
int64_t ParseMemoryValue(const std::string& value, bool* parse_ok) {
  // Filter out the decimal portion from any unit designation in the string.
  std::string number_string;
  double units = 1.0;
  if (base::EndsWith(value, "kb", base::CompareCase::INSENSITIVE_ASCII)) {
    units = 1024.0;
    number_string = value.substr(0, value.size() - 2);
  } else if (base::EndsWith(value, "mb",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    units = 1024.0 * 1024.0;
    number_string = value.substr(0, value.size() - 2);
  } else if (base::EndsWith(value, "gb",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    units = 1024.0 * 1024.0 * 1024.0;
    number_string = value.substr(0, value.size() - 2);
  } else if (base::EndsWith(value, "b", base::CompareCase::INSENSITIVE_ASCII)) {
    number_string = value.substr(0, value.size() - 1);
  } else {
    number_string = value;
  }

  double numerical_value = 0.0;
  *parse_ok = base::StringToDouble(number_string, &numerical_value);
  return static_cast<int64_t>(numerical_value * units);
}

template <typename ValueType>
bool TryParseValue(base::Optional<ValueType>* destination,
                   const std::string& string_value);

template <>
bool TryParseValue<int64_t>(base::Optional<int64_t>* destination,
                            const std::string& string_value) {
  bool parse_ok = false;
  int64_t int_value = ParseMemoryValue(string_value, &parse_ok);

  if (parse_ok) {
    *destination = int_value;
    return true;
  }

  LOG(ERROR) << "Invalid value for command line setting: " << string_value;
  return false;
}

template <>
bool TryParseValue<TextureDimensions>(
    base::Optional<TextureDimensions>* destination,
    const std::string& string_value) {
  std::vector<ParsedIntValue> int_values = ParseDimensions(string_value);

  if ((int_values.size() < 2) || (int_values.size() > 3)) {
    LOG(ERROR) << "Invalid value for parse value setting: " << string_value;
    return false;  // Parse failed.
  }

  for (size_t i = 0; i < int_values.size(); ++i) {
    if (int_values[i].error_) {
      LOG(ERROR) << "Invalid value for parse value setting: " << string_value;
      return false;  // Parse failed.
    }
  }

  const int bytes_per_pixel = int_values.size() == 3
                                  ? int_values[2].value_
                                  : kSkiaGlyphAtlasTextureBytesPerPixel;

  TextureDimensions tex_dimensions(int_values[0].value_, int_values[1].value_,
                                   bytes_per_pixel);

  *destination = tex_dimensions;
  return true;
}

template <typename ValueType>
ValueType GetAutosetValue();

template <>
int64_t GetAutosetValue() {
  return -1;
}

template <>
TextureDimensions GetAutosetValue() {
  return TextureDimensions(-1, -1, -1);
}

template <typename ValueType>
bool Set(const base::CommandLine& command_line,
         base::Optional<ValueType>* setting, const char* setting_name) {
  if (!command_line.HasSwitch(setting_name)) {
    return true;
  }

  std::string value = command_line.GetSwitchValueNative(setting_name);
  if (StringValueSignalsAutoset(value)) {
    *setting = GetAutosetValue<ValueType>();
    return true;
  }

  return TryParseValue(setting, value);
}
}  // namespace

AutoMemSettings GetDefaultBuildSettings() {
  AutoMemSettings settings(AutoMemSettings::kTypeBuild);
  configuration::Configuration* config =
      configuration::Configuration::GetInstance();
  settings.has_blitter = HasBlitter();

  settings.cobalt_encoded_image_cache_size_in_bytes =
      MakeValidIfGreaterThanOrEqualToZero(
          config->CobaltEncodedImageCacheSizeInBytes());
  settings.cobalt_image_cache_size_in_bytes =
      MakeValidIfGreaterThanOrEqualToZero(
          config->CobaltImageCacheSizeInBytes());
  settings.remote_typeface_cache_capacity_in_bytes =
      MakeValidIfGreaterThanOrEqualToZero(
          config->CobaltRemoteTypefaceCacheSizeInBytes());
  settings.skia_cache_size_in_bytes =
      MakeValidIfGreaterThanOrEqualToZero(config->CobaltSkiaCacheSizeInBytes());
  settings.skia_texture_atlas_dimensions = MakeDimensionsIfValid(
      TextureDimensions(config->CobaltSkiaGlyphAtlasWidth(),
                        config->CobaltSkiaGlyphAtlasHeight(),
                        kSkiaGlyphAtlasTextureBytesPerPixel));

  // Render tree node cache settings for various rasterizers.
  settings.software_surface_cache_size_in_bytes =
      MakeValidIfGreaterThanOrEqualToZero(
          config->CobaltSoftwareSurfaceCacheSizeInBytes());
  settings.offscreen_target_cache_size_in_bytes =
      MakeValidIfGreaterThanOrEqualToZero(
          config->CobaltOffscreenTargetCacheSizeInBytes());

#if SB_API_VERSION < 12
#if defined(COBALT_MAX_CPU_USAGE_IN_BYTES)
  settings.max_cpu_in_bytes =
      MakeValidIfGreaterThanOrEqualToZero(COBALT_MAX_CPU_USAGE_IN_BYTES);
#endif  // defined(COBALT_MAX_CPU_USAGE_IN_BYTES)
#if defined(COBALT_MAX_GPU_USAGE_IN_BYTES)
  settings.max_gpu_in_bytes =
      MakeValidIfGreaterThanOrEqualToZero(COBALT_MAX_GPU_USAGE_IN_BYTES);
#endif  // defined(COBALT_MAX_GPU_USAGE_IN_BYTES)
#endif  // SB_API_VERSION < 12

  settings.reduce_gpu_memory_by =
      MakeValidIfGreaterThanOrEqualToZero(config->CobaltReduceGpuMemoryBy());

  return settings;
}

AutoMemSettings GetSettings(const base::CommandLine& command_line) {
  AutoMemSettings settings(AutoMemSettings::kTypeCommandLine);
  settings.has_blitter = HasBlitter();

  Set(command_line, &settings.cobalt_encoded_image_cache_size_in_bytes,
      switches::kEncodedImageCacheSizeInBytes);
  Set(command_line, &settings.cobalt_image_cache_size_in_bytes,
      switches::kImageCacheSizeInBytes);
  Set(command_line, &settings.remote_typeface_cache_capacity_in_bytes,
      switches::kRemoteTypefaceCacheSizeInBytes);
  Set(command_line, &settings.skia_cache_size_in_bytes,
      switches::kSkiaCacheSizeInBytes);
  Set(command_line, &settings.skia_texture_atlas_dimensions,
      switches::kSkiaTextureAtlasDimensions);
  Set(command_line, &settings.software_surface_cache_size_in_bytes,
      switches::kSoftwareSurfaceCacheSizeInBytes);
  Set(command_line, &settings.offscreen_target_cache_size_in_bytes,
      switches::kOffscreenTargetCacheSizeInBytes);
  Set(command_line, &settings.max_cpu_in_bytes, switches::kMaxCobaltCpuUsage);
  Set(command_line, &settings.max_gpu_in_bytes, switches::kMaxCobaltGpuUsage);

  return settings;
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
