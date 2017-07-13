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

#include "cobalt/browser/memory_settings/auto_mem_settings.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/optional.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "cobalt/browser/memory_settings/constants.h"
#include "cobalt/browser/switches.h"
#include "nb/lexical_cast.h"

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

char ToLowerCharTypesafe(int c) { return static_cast<char>(::tolower(c)); }

std::string ToLower(const std::string& input) {
  std::string value_str = input;
  std::transform(value_str.begin(), value_str.end(), value_str.begin(),
                 ToLowerCharTypesafe);

  return value_str;
}

bool StringValueSignalsAutoset(const std::string& value) {
  std::string value_lower_case = ToLower(value);
  return ((value_lower_case == "auto") || (value_lower_case == "autoset") ||
          (value_lower_case == "-1"));
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
  std::string value_str = ToLower(input);
  std::vector<ParsedIntValue> output;

  std::vector<std::string> lengths;
  base::SplitString(value_str, 'x', &lengths);

  for (size_t i = 0; i < lengths.size(); ++i) {
    ParsedIntValue parsed_value;
    parsed_value.error_ = !base::StringToInt(lengths[i], &parsed_value.value_);
    output.push_back(parsed_value);
  }
  return output;
}

bool StringEndsWith(const std::string& value, const std::string& ending) {
  if (ending.size() > value.size()) {
    return false;
  }
  // Reverse search through the back of the string.
  return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

// Handles bytes: "12435"
// Handles kilobytes: "128KB"
// Handles megabytes: "64MB"
// Handles gigabytes: "1GB"
// Handles fractional units for kilo/mega/gigabytes
int64_t ParseMemoryValue(const std::string& value, bool* parse_ok) {
  // nb::lexical_cast<> will parse out the number but it will ignore the
  // unit part, such as "kb" or "mb".
  double numerical_value = nb::lexical_cast<double>(value, parse_ok);
  if (!(*parse_ok)) {
    return static_cast<int64_t>(numerical_value);
  }

  // Lowercasing the string makes the units easier to detect.
  std::string value_lower_case = ToLower(value);

  if (StringEndsWith(value_lower_case, "kb")) {
    numerical_value *= 1024;  // convert kb -> bytes.
  } else if (StringEndsWith(value_lower_case, "mb")) {
    numerical_value *= 1024 * 1024;  // convert mb -> bytes.
  } else if (StringEndsWith(value_lower_case, "gb")) {
    numerical_value *= 1024 * 1024 * 1024;  // convert gb -> bytes.
  }
  return static_cast<int64_t>(numerical_value);
}

template <typename ValueType>
bool TryParseValue(base::optional<ValueType>* destination,
                   const std::string& string_value);

template <>
bool TryParseValue<int64_t>(base::optional<int64_t>* destination,
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
    base::optional<TextureDimensions>* destination,
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
bool Set(const CommandLine& command_line, base::optional<ValueType>* setting,
         const char* setting_name) {
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
      MakeDimensionsIfValid(TextureDimensions(
          COBALT_SKIA_GLYPH_ATLAS_WIDTH, COBALT_SKIA_GLYPH_ATLAS_HEIGHT,
          kSkiaGlyphAtlasTextureBytesPerPixel));

  // Render tree node cache settings for various rasterizers.
  settings.software_surface_cache_size_in_bytes =
      MakeValidIfGreaterThanOrEqualToZero(
          COBALT_SOFTWARE_SURFACE_CACHE_SIZE_IN_BYTES);
  settings.offscreen_target_cache_size_in_bytes =
      MakeValidIfGreaterThanOrEqualToZero(
          COBALT_OFFSCREEN_TARGET_CACHE_SIZE_IN_BYTES);

  settings.max_cpu_in_bytes =
      MakeValidIfGreaterThanOrEqualToZero(COBALT_MAX_CPU_USAGE_IN_BYTES);
  settings.max_gpu_in_bytes =
      MakeValidIfGreaterThanOrEqualToZero(COBALT_MAX_GPU_USAGE_IN_BYTES);

  settings.reduce_cpu_memory_by =
      MakeValidIfGreaterThanOrEqualToZero(COBALT_REDUCE_CPU_MEMORY_BY);
  settings.reduce_gpu_memory_by =
      MakeValidIfGreaterThanOrEqualToZero(COBALT_REDUCE_GPU_MEMORY_BY);

  return settings;
}

AutoMemSettings GetSettings(const CommandLine& command_line) {
  AutoMemSettings settings(AutoMemSettings::kTypeCommandLine);
  settings.has_blitter = HasBlitter();

  Set(command_line, &settings.cobalt_image_cache_size_in_bytes,
      switches::kImageCacheSizeInBytes);
  Set(command_line, &settings.javascript_garbage_collection_threshold_in_bytes,
      switches::kJavaScriptGcThresholdInBytes);
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
  Set(command_line, &settings.reduce_cpu_memory_by,
      switches::kReduceCpuMemoryBy);
  Set(command_line, &settings.reduce_gpu_memory_by,
      switches::kReduceGpuMemoryBy);

  return settings;
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
