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

#ifndef COBALT_BROWSER_MEMORY_SETTINGS_MEMORY_SETTINGS_H_
#define COBALT_BROWSER_MEMORY_SETTINGS_MEMORY_SETTINGS_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/optional.h"
#include "cobalt/browser/memory_settings/scaling_function.h"
#include "cobalt/browser/memory_settings/texture_dimensions.h"
#include "cobalt/math/size.h"
#include "starboard/configuration.h"
#include "starboard/types.h"

namespace cobalt {
namespace browser {
namespace memory_settings {

// An abstract MemorySetting. Subclasses will implement how the class parses
// string values.
class MemorySetting {
 public:
  virtual ~MemorySetting() {}
  // SourceType defines the location where the setting was set from.
  // kNotApplicable means the setting is not supported on the current system
  // configuration.
  enum SourceType { kUnset, kStarboardAPI, kBuildSetting, kCmdLine, kAutoSet,
                    kAutosetConstrained };
  enum ClassType { kInt, kDimensions };
  enum MemoryType { kCPU, kGPU, kNotApplicable };

  // Stringify's the value of this memory setting.
  virtual std::string ValueToString() const = 0;

  // Returns the memory consumption (in bytes) that the memory setting will
  // be allocated.
  virtual int64_t MemoryConsumption() const = 0;

  // Sets the memory scaling function. This will control whether this
  // MemorySetting will allow it's memory to be adjusted. If this is not
  // set then ComputeAbsoluteMemoryScale(...) will return 1.0 for all inputs
  // values, indicating that it can't be adjusted.
  void set_memory_scaling_function(ScalingFunction function);

  // Computes the absolute memory scale value from the requested_memory_scale.
  // The absolute memory scale can be multiplied against MemoryConsumption()
  // to predict the new memory consumption. This prediction should match the
  // actual memory consumption after ScaleMemory(memory_scale) is called.
  double ComputeAbsoluteMemoryScale(double requested_memory_scale) const;

  // Adjusts the memory by the percentage passed in. Note that the base value
  // is adjusted and repeatedly calling this function will repeatedly adjust
  // this value. For example, calling AdjustMemory(.5) twice will reduce the
  // memory by 25%.
  //
  // Calling this function will automatically set the source_type to
  // kAutosetConstrained.
  //
  // Note that the expectation here is that the memory scale passed in was
  // generated from ComputeAbsoluteMemoryScale().
  virtual void ScaleMemory(double memory_scale) = 0;

  // Setting kNotApplicable will invalidate the MemorySetting.
  void set_memory_type(MemoryType memory_type) { memory_type_ = memory_type; }
  const std::string& name() const { return name_; }
  SourceType source_type() const { return source_type_; }
  ClassType class_type() const { return class_type_; }
  MemoryType memory_type() const { return memory_type_; }

  bool valid() const {
    return (kNotApplicable != memory_type_) && (kUnset != source_type_);
  }

 protected:
  MemorySetting(ClassType type, const std::string& name);

  const ClassType class_type_;
  const std::string name_;
  SourceType source_type_;
  MemoryType memory_type_;  // Defaults to kCPU.
  ScalingFunction memory_scaling_function_;

 private:
  // Default constructor for MemorySetting is forbidden. Do not use it.
  MemorySetting();
  SB_DISALLOW_COPY_AND_ASSIGN(MemorySetting);
};

// A memory setting for integer values.
class IntSetting : public MemorySetting {
 public:
  explicit IntSetting(const std::string& name);

  std::string ValueToString() const override;
  int64_t MemoryConsumption() const override;
  void ScaleMemory(double absolute_constraining_value) override;

  int64_t value() const { return valid() ? value_ : 0; }
  base::optional<int64_t> optional_value() const {
    base::optional<int64_t> output;
    if (valid()) { output = value_; }
    return output;
  }
  void set_value(SourceType source_type, int64_t val) {
    source_type_ = source_type;
    value_ = val;
  }

 private:
  int64_t value_;

  SB_DISALLOW_COPY_AND_ASSIGN(IntSetting);
};

// A memory setting for dimensions type values like "2048x4096x2" where
// the first value is width, second is height, and third is bytes_per_pixel.
class DimensionSetting : public MemorySetting {
 public:
  explicit DimensionSetting(const std::string& name);

  std::string ValueToString() const override;
  int64_t MemoryConsumption() const override;

  TextureDimensions value() const {
    return valid() ? value_ : TextureDimensions();
  }
  base::optional<TextureDimensions> optional_value() const {
    base::optional<TextureDimensions> output;
    if (valid()) { output = value_; }
    return output;
  }

  void set_value(SourceType source_type, const TextureDimensions& val) {
    source_type_ = source_type;
    value_ = val;
  }

 private:
  TextureDimensions value_;

  SB_DISALLOW_COPY_AND_ASSIGN(DimensionSetting);
};

class SkiaGlyphAtlasTextureSetting : public DimensionSetting {
 public:
  SkiaGlyphAtlasTextureSetting();
  void ScaleMemory(double absolute_constraining_value) override;

 private:
  static size_t NumberOfReductions(double reduction_factor);
};

class JavaScriptGcThresholdSetting : public IntSetting {
 public:
  JavaScriptGcThresholdSetting();
  void PostInit();
};

int64_t SumMemoryConsumption(
    base::optional<MemorySetting::MemoryType> memory_type_filter,
    const std::vector<const MemorySetting*>& memory_settings);

int64_t SumMemoryConsumption(
    base::optional<MemorySetting::MemoryType> memory_type_filter,
    const std::vector<MemorySetting*>& memory_settings);

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_SETTINGS_MEMORY_SETTINGS_H_
