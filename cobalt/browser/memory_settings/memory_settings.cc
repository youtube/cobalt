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

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "cobalt/browser/memory_settings/build_settings.h"
#include "cobalt/browser/memory_settings/constants.h"
#include "cobalt/browser/switches.h"
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
std::vector<ParsedIntValue> ParseDimensions(const std::string& value_str) {
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

}  // namespace

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

bool IntSetting::TryParseValue(SourceType source_type,
                               const std::string& string_value) {
  bool parse_ok = false;
  int64_t int_value = nb::lexical_cast<int64_t>(string_value, &parse_ok);

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

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
