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

#include "cobalt/browser/memory_settings/memory_settings.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "cobalt/browser/memory_settings/build_settings.h"
#include "cobalt/browser/memory_settings/constants.h"
#include "cobalt/browser/switches.h"
#include "cobalt/math/linear_interpolator.h"
#include "nb/lexical_cast.h"

namespace cobalt {
namespace browser {
namespace memory_settings {
namespace {

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
  std::string value_str = input;
  std::transform(value_str.begin(), value_str.end(),
                 value_str.begin(), ::tolower);
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
int64_t ParseMemoryValue(const std::string& value, bool* parse_ok) {
  // nb::lexical_cast<> will parse out the number but it will ignore the
  // unit part, such as "kb" or "mb".
  int64_t numerical_value = nb::lexical_cast<int64_t>(value, parse_ok);
  if (!(*parse_ok) || value.size() < 2) {
    return numerical_value;
  }

  // Lowercasing the string makes the units easier to detect.
  std::string value_lower_case = value;
  std::transform(value_lower_case.begin(), value_lower_case.end(),
                 value_lower_case.begin(),
                 ::tolower);

  if (StringEndsWith(value_lower_case, "kb")) {
    numerical_value *= 1024;  // convert kb -> bytes.
  } else if (StringEndsWith(value_lower_case, "mb")) {
    numerical_value *= 1024 * 1024;  // convert mb -> bytes.
  }
  return numerical_value;
}
}  // namespace

void MemorySetting::set_memory_scaling_function(ScalingFunction function) {
  memory_scaling_function_ = function;
}

double MemorySetting::ComputeAbsoluteMemoryScale(
    double requested_memory_scale) const {
  if (memory_scaling_function_.is_null()) {
    return 1.0;
  } else {
    return memory_scaling_function_.Run(requested_memory_scale);
  }
}

MemorySetting::MemorySetting(ClassType type, const std::string& name)
    : class_type_(type),
      name_(name),
      source_type_(kUnset),
      memory_type_(kCPU) {}

IntSetting::IntSetting(const std::string& name)
    : MemorySetting(kInt, name), value_() {}

std::string IntSetting::ValueToString() const {
  std::stringstream ss;
  ss << value();
  return ss.str();
}

int64_t IntSetting::MemoryConsumption() const { return value(); }

void IntSetting::ScaleMemory(double memory_scale) {
  DCHECK_LE(0.0, memory_scale);
  DCHECK_LE(memory_scale, 1.0);

  const int64_t new_value = static_cast<int64_t>(value() * memory_scale);
  set_value(MemorySetting::kAutosetConstrained, new_value);
}

bool IntSetting::TryParseValue(SourceType source_type,
                               const std::string& string_value) {
  bool parse_ok = false;
  int64_t int_value = ParseMemoryValue(string_value, &parse_ok);

  if (parse_ok) {
    set_value(source_type, int_value);
    return true;
  } else {
    LOG(ERROR) << "Invalid value for command line setting: " << string_value;
    return false;
  }
}

DimensionSetting::DimensionSetting(const std::string& name)
    : MemorySetting(kDimensions, name) {}

std::string DimensionSetting::ValueToString() const {
  std::stringstream ss;
  TextureDimensions value_data = value();
  ss << value_data.width() << "x" << value_data.height() << "x"
     << value_data.bytes_per_pixel();
  return ss.str();
}

int64_t DimensionSetting::MemoryConsumption() const {
  return value().TotalBytes();
}

bool DimensionSetting::TryParseValue(SourceType source_type,
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

  const int bytes_per_pixel =
      int_values.size() == 3 ?
      int_values[2].value_ : kSkiaGlyphAtlasTextureBytesPerPixel;

  TextureDimensions tex_dimensions(int_values[0].value_, int_values[1].value_,
                                   bytes_per_pixel);

  set_value(source_type, tex_dimensions);
  return true;
}

SkiaGlyphAtlasTextureSetting::SkiaGlyphAtlasTextureSetting()
    : DimensionSetting(switches::kSkiaTextureAtlasDimensions) {
  set_memory_scaling_function(MakeSkiaGlyphAtlasMemoryScaler());
}

void SkiaGlyphAtlasTextureSetting::ScaleMemory(double memory_scale) {
  DCHECK_LE(0.0, memory_scale);
  DCHECK_LE(memory_scale, 1.0);

  if (!valid()) {
    return;
  }
  const size_t number_of_reductions = NumberOfReductions(memory_scale);
  if (number_of_reductions == 0) {
    return;
  }

  TextureDimensions texture_dims = value();
  DCHECK_LT(0, texture_dims.width());
  DCHECK_LT(0, texture_dims.height());

  for (size_t i = 0; i < number_of_reductions; ++i) {
    if (texture_dims.width() <= 1 && texture_dims.height() <=1) {
      break;
    }
    if (texture_dims.width() > texture_dims.height()) {
      texture_dims.set_width(texture_dims.width() / 2);
    } else {
      texture_dims.set_height(texture_dims.height() / 2);
    }
  }

  set_value(MemorySetting::kAutosetConstrained, texture_dims);
}

size_t SkiaGlyphAtlasTextureSetting::NumberOfReductions(
    double reduction_factor) {
  size_t num_of_reductions = 0;
  while (reduction_factor <= 0.5f) {
    ++num_of_reductions;
    reduction_factor *= 2.0;
  }
  return num_of_reductions;
}

JavaScriptGcThresholdSetting::JavaScriptGcThresholdSetting()
    : IntSetting(switches::kJavaScriptGcThresholdInBytes) {
}

void JavaScriptGcThresholdSetting::PostInit() {
  const int64_t normal_memory_consumption = MemoryConsumption();
  const int64_t min_memory_consumption =
      std::min<int64_t>(normal_memory_consumption, 1 * 1024 * 1024);

  ScalingFunction function =
      MakeJavaScriptGCScaler(min_memory_consumption,
                             normal_memory_consumption);
  set_memory_scaling_function(function);
}

int64_t SumMemoryConsumption(
    base::optional<MemorySetting::MemoryType> memory_type_filter,
    const std::vector<const MemorySetting*>& memory_settings) {
  int64_t sum = 0;
  for (size_t i = 0; i < memory_settings.size(); ++i) {
    const MemorySetting* setting = memory_settings[i];
    if (!memory_type_filter || memory_type_filter == setting->memory_type()) {
      sum += setting->MemoryConsumption();
    }
  }
  return sum;
}

int64_t SumMemoryConsumption(
    base::optional<MemorySetting::MemoryType> memory_type_filter,
    const std::vector<MemorySetting*>& memory_settings) {
  const std::vector<const MemorySetting*> const_vector(
      memory_settings.begin(), memory_settings.end());
  return SumMemoryConsumption(memory_type_filter, const_vector);
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
