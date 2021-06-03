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

#ifndef COBALT_BROWSER_MEMORY_SETTINGS_TEST_COMMON_H_
#define COBALT_BROWSER_MEMORY_SETTINGS_TEST_COMMON_H_

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "cobalt/browser/memory_settings/memory_settings.h"
#include "cobalt/browser/memory_settings/texture_dimensions.h"
#include "cobalt/browser/switches.h"
#include "cobalt/math/size.h"
#include "testing/gtest/include/gtest/gtest.h"

// PrintToString() provides the proper functionality to pretty print custom
// types.

namespace testing {

template <>
inline ::std::string PrintToString(const cobalt::math::Size& value) {
  ::std::stringstream ss;
  ss << "math::Size(" << value.width() << ", " << value.height() << ")";
  return ss.str();
}

template <>
inline ::std::string PrintToString(
    const cobalt::browser::memory_settings::TextureDimensions& value) {
  ::std::stringstream ss;
  ss << "TextureDimensions(" << value.width() << "x" << value.height() << "x"
     << value.bytes_per_pixel() << ")";
  return ss.str();
}

}  // namespace testing

namespace cobalt {
namespace browser {
namespace memory_settings {

class TestDimensionSetting : public DimensionSetting {
 public:
  explicit TestDimensionSetting(const std::string& name)
      : DimensionSetting(name) {}
  virtual void ScaleMemory(double absolute_constraining_value) {
  }
};

class TestSettingGroup {
 public:
  TestSettingGroup() {}

  void LoadDefault() {
    MakeSetting(MemorySetting::kCmdLine, MemorySetting::kGPU,
                switches::kImageCacheSizeInBytes, 1234);

    MakeSetting(MemorySetting::kCmdLine, MemorySetting::kGPU,
                switches::kSkiaTextureAtlasDimensions,
                TextureDimensions(1234, 4567, 2));

    MakeSetting(MemorySetting::kCmdLine, MemorySetting::kGPU,
                switches::kSkiaCacheSizeInBytes, 12345678);

    MakeSetting(MemorySetting::kBuildSetting, MemorySetting::kNotApplicable,
                switches::kSoftwareSurfaceCacheSizeInBytes, 8910);
  }

  ~TestSettingGroup() {
    for (MemoryMap::const_iterator it = map_.begin(); it != map_.end(); ++it) {
      delete it->second;
    }
  }

  // The memory setting is owned internally.
  template <typename ValueType>
  void MakeSetting(MemorySetting::SourceType source_type,
                   MemorySetting::MemoryType memory_type,
                   const std::string& name, const ValueType& value) {
    const bool found = (map_.find(name) != map_.end());

    ASSERT_FALSE(found);
    map_[name] = CreateMemorySetting(source_type, memory_type, name, value);
  }

  std::vector<const MemorySetting*> AsConstVector() const {
    std::vector<const MemorySetting*> output;
    for (MemoryMap::const_iterator it = map_.begin(); it != map_.end(); ++it) {
      output.push_back(it->second);
    }
    return output;
  }

  std::vector<MemorySetting*> AsMutableVector() {
    std::vector<MemorySetting*> output;
    for (MemoryMap::iterator it = map_.begin(); it != map_.end(); ++it) {
      output.push_back(it->second);
    }
    return output;
  }

 private:
  template <typename ValueType>
  static MemorySetting* CreateMemorySetting(
      MemorySetting::SourceType source_type,
      MemorySetting::MemoryType memory_type, const std::string& name,
      const ValueType& value) {
    IntSetting* memory_setting = new IntSetting(name);
    memory_setting->set_memory_type(memory_type);
    memory_setting->set_value(source_type, value);
    return memory_setting;
  }

  typedef std::map<std::string, MemorySetting*> MemoryMap;
  MemoryMap map_;
};

// Specialization for TextureDimensions.

// static
template <>
inline MemorySetting* TestSettingGroup::CreateMemorySetting(
    MemorySetting::SourceType source_type,
    MemorySetting::MemoryType memory_type, const std::string& name,
    const TextureDimensions& value) {
  TestDimensionSetting* memory_setting = new TestDimensionSetting(name);
  memory_setting->set_memory_type(memory_type);
  memory_setting->set_value(source_type, value);
  return memory_setting;
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_SETTINGS_TEST_COMMON_H_
