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

#include "base/compiler_specific.h"
#include "base/optional.h"
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
  enum SourceType { kUnset, kCmdLine, kBuildSetting, kAutoSet };
  enum ClassType { kInt, kDimensions };
  enum MemoryType { kCPU, kGPU, kNotApplicable };

  virtual std::string ValueToString() const = 0;
  // Returns true if the TryParseValue() succeeded when converting the string
  // into the internal value. If false, then the object should not be changed.
  virtual bool TryParseValue(SourceType source_type,
                             const std::string& string_value) = 0;

  virtual int64_t MemoryConsumption() const = 0;

  // Setting kNotApplicable will invalidate the MemorySetting.
  void set_memory_type(MemoryType memory_type) { memory_type_ = memory_type; }

  const std::string& name() const { return name_; }
  SourceType source_type() const { return source_type_; }
  ClassType class_type() const { return class_type_; }
  MemoryType memory_type() const { return memory_type_; }

  bool valid() const { return kNotApplicable != memory_type_; }

 protected:
  MemorySetting(ClassType type, const std::string& name);

  const ClassType class_type_;
  const std::string name_;
  SourceType source_type_;
  MemoryType memory_type_;  // Defaults to kCPU.

 private:
  // Default constructor for MemorySetting is forbidden. Do not use it.
  MemorySetting();
  SB_DISALLOW_COPY_AND_ASSIGN(MemorySetting);
};

// A memory setting for integer values.
class IntSetting : public MemorySetting {
 public:
  explicit IntSetting(const std::string& name);

  std::string ValueToString() const OVERRIDE;
  virtual int64_t MemoryConsumption() const OVERRIDE;

  int64_t value() const { return valid() ? value_ : 0; }
  void set_value(SourceType source_type, int64_t val) {
    source_type_ = source_type;
    value_ = val;
  }
  bool TryParseValue(SourceType source_type,
                     const std::string& string_value) OVERRIDE;

 private:
  int64_t value_;

  SB_DISALLOW_COPY_AND_ASSIGN(IntSetting);
};

// A memory setting for dimensions type values like "2048x4096x2" where
// the first value is width, second is height, and third is bytes_per_pixel.
class DimensionSetting : public MemorySetting {
 public:
  explicit DimensionSetting(const std::string& name);

  std::string ValueToString() const OVERRIDE;
  virtual int64_t MemoryConsumption() const OVERRIDE;

  TextureDimensions value() const {
    return valid() ? value_ : TextureDimensions();
  }
  void set_value(SourceType source_type, const TextureDimensions& val) {
    source_type_ = source_type;
    value_ = val;
  }
  bool TryParseValue(SourceType source_type,
                     const std::string& string_value) OVERRIDE;

 private:
  TextureDimensions value_;

  SB_DISALLOW_COPY_AND_ASSIGN(DimensionSetting);
};

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_SETTINGS_MEMORY_SETTINGS_H_
