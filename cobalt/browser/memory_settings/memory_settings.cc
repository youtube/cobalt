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

#include "cobalt/browser/memory_settings/memory_settings.h"

#include <algorithm>
#include <locale>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "cobalt/browser/memory_settings/constants.h"
#include "cobalt/browser/switches.h"
#include "cobalt/math/linear_interpolator.h"

namespace cobalt {
namespace browser {
namespace memory_settings {

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
    if (texture_dims.width() <= 1 && texture_dims.height() <= 1) {
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

int64_t SumMemoryConsumption(
    base::Optional<MemorySetting::MemoryType> memory_type_filter,
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
    base::Optional<MemorySetting::MemoryType> memory_type_filter,
    const std::vector<MemorySetting*>& memory_settings) {
  const std::vector<const MemorySetting*> const_vector(memory_settings.begin(),
                                                       memory_settings.end());
  return SumMemoryConsumption(memory_type_filter, const_vector);
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
