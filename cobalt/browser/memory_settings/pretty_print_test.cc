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

#include "cobalt/browser/memory_settings/pretty_print.h"

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/browser/memory_settings/memory_settings.h"
#include "cobalt/browser/switches.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/system.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace browser {
namespace memory_settings {

class TestSettingGroup {
 public:
  TestSettingGroup() {}

  void LoadDefault() {
    MakeSetting(MemorySetting::kInt, MemorySetting::kCmdLine,
                MemorySetting::kGPU, switches::kImageCacheSizeInBytes, "1234");

    MakeSetting(MemorySetting::kInt, MemorySetting::kAutoSet,
                MemorySetting::kCPU, switches::kJavaScriptGcThresholdInBytes,
                "1112");

    MakeSetting(MemorySetting::kDimensions, MemorySetting::kCmdLine,
                MemorySetting::kGPU, switches::kSkiaTextureAtlasDimensions,
                "1234x4567");

    MakeSetting(MemorySetting::kInt, MemorySetting::kCmdLine,
                MemorySetting::kGPU, switches::kSkiaCacheSizeInBytes,
                "12345678");

    MakeSetting(MemorySetting::kInt, MemorySetting::kBuildSetting,
                MemorySetting::kNotApplicable,
                switches::kSoftwareSurfaceCacheSizeInBytes, "8910");
  }

  ~TestSettingGroup() {
    for (ConstIter it = map_.begin(); it != map_.end(); ++it) {
      delete it->second;
    }
  }

  // The memory setting is owned internally.
  void MakeSetting(MemorySetting::ClassType class_type,
                   MemorySetting::SourceType source_type,
                   MemorySetting::MemoryType memory_type,
                   const std::string& name, const std::string& value) {
   const bool found = (map_.find(name) !=  map_.end());

    ASSERT_FALSE(found);
    map_[name] =
        CreateMemorySetting(class_type, source_type, memory_type, name, value);
  }

  std::vector<const MemorySetting*> AsConstVector() const {
    std::vector<const MemorySetting*> output;
    for (ConstIter it = map_.begin(); it != map_.end(); ++it) {
      output.push_back(it->second);
    }
    return output;
  }

 private:
  static MemorySetting* CreateMemorySetting(
      MemorySetting::ClassType class_type,
      MemorySetting::SourceType source_type,
      MemorySetting::MemoryType memory_type, const std::string& name,
      const std::string& value) {
    MemorySetting* memory_setting = NULL;
    switch (class_type) {
      case MemorySetting::kInt: {
        memory_setting = new IntSetting(name);
        break;
      }
      case MemorySetting::kDimensions: {
        memory_setting = new DimensionSetting(name);
        break;
      }
      default: {
        EXPECT_TRUE(false) << "Unexpected type " << class_type;
        memory_setting = new IntSetting(name);
        break;
      }
    }
    EXPECT_TRUE(memory_setting->TryParseValue(source_type, value));
    memory_setting->set_memory_type(memory_type);
    return memory_setting;
  }

  typedef std::map<std::string, MemorySetting*>::const_iterator ConstIter;
  std::map<std::string, MemorySetting*> map_;
};

TEST(MemorySettingsPrettyPrint, GeneratePrettyPrintTable) {
  TestSettingGroup setting_group;
  setting_group.LoadDefault();
  std::string actual_string =
      GeneratePrettyPrintTable(setting_group.AsConstVector());

  const char* expected_string =
      " SETTING NAME                           VALUE                   TYPE   SOURCE    \n"
      " _______________________________________________________________________________ \n"
      "|                                      |             |         |      |         |\n"
      "| image_cache_size_in_bytes            |        1234 |  0.0 MB |  GPU | CmdLine |\n"
      "|______________________________________|_____________|_________|______|_________|\n"
      "|                                      |             |         |      |         |\n"
      "| javascript_gc_threshold_in_bytes     |        1112 |  0.0 MB |  CPU | AutoSet |\n"
      "|______________________________________|_____________|_________|______|_________|\n"
      "|                                      |             |         |      |         |\n"
      "| skia_atlas_texture_dimensions        | 1234x4567x2 | 10.7 MB |  GPU | CmdLine |\n"
      "|______________________________________|_____________|_________|______|_________|\n"
      "|                                      |             |         |      |         |\n"
      "| skia_cache_size_in_bytes             |    12345678 | 11.8 MB |  GPU | CmdLine |\n"
      "|______________________________________|_____________|_________|______|_________|\n"
      "|                                      |             |         |      |         |\n"
      "| software_surface_cache_size_in_bytes |         N/A |     N/A |  N/A |     N/A |\n"
      "|______________________________________|_____________|_________|______|_________|\n";

  EXPECT_STREQ(expected_string, actual_string.c_str());
}

TEST(MemorySettingsPrettyPrint, GenerateMemoryTableWithUnsetGpuMemory) {
  IntSetting cpu_memory_setting("max_cpu_memory");
  cpu_memory_setting.set_value(
      MemorySetting::kBuildSetting, 256 * 1024 * 1024);
  IntSetting gpu_memory_setting("max_gpu_memory");

  std::string actual_output =
      GenerateMemoryTable(cpu_memory_setting,  // 256 MB CPU available
                          gpu_memory_setting,
                          128 * 1024 * 1024,  // 128 MB CPU consumption
                          0);                 // 0 MB GPU consumption.

  const char* expected_output =
      " MEMORY           SOURCE   TOTAL       SETTINGS CONSUME   \n"
      " ________________________________________________________ \n"
      "|                |        |           |                  |\n"
      "| max_cpu_memory |  Build |  256.0 MB |         128.0 MB |\n"
      "|________________|________|___________|__________________|\n"
      "|                |        |           |                  |\n"
      "| max_gpu_memory |  Unset | <UNKNOWN> |           0.0 MB |\n"
      "|________________|________|___________|__________________|\n";

  EXPECT_STREQ(expected_output, actual_output.c_str()) << actual_output;
}

TEST(MemorySettingsPrettyPrint, GenerateMemoryTableWithGpuMemory) {
  IntSetting cpu_memory_setting("max_cpu_memory");
  cpu_memory_setting.set_value(
      MemorySetting::kBuildSetting, 256 * 1024 * 1024);
  IntSetting gpu_memory_setting("max_gpu_memory");
  gpu_memory_setting.set_value(
      MemorySetting::kBuildSetting, 64 * 1024 * 1024);

  std::string actual_output =
      GenerateMemoryTable(cpu_memory_setting,  // 256 MB CPU available.
                          gpu_memory_setting,   // 64 MB GPU available.
                          128 * 1024 * 1024,  // 128 MB CPU consumption.
                          23592960);          // 22.5 MB GPU consumption.

  const char* expected_output =
      " MEMORY           SOURCE   TOTAL      SETTINGS CONSUME   \n"
      " _______________________________________________________ \n"
      "|                |        |          |                  |\n"
      "| max_cpu_memory |  Build | 256.0 MB |         128.0 MB |\n"
      "|________________|________|__________|__________________|\n"
      "|                |        |          |                  |\n"
      "| max_gpu_memory |  Build |  64.0 MB |          22.5 MB |\n"
      "|________________|________|__________|__________________|\n";

  EXPECT_STREQ(expected_output, actual_output.c_str()) << actual_output;
}

TEST(MemorySettingsPrettyPrint, ToString) {
  TestSettingGroup test_setting_group;
  test_setting_group.LoadDefault();

  std::string actual_string = GeneratePrettyPrintTable(
      test_setting_group.AsConstVector());

  const char* expected_string =
      " SETTING NAME                           VALUE                   TYPE   SOURCE    \n"
      " _______________________________________________________________________________ \n"
      "|                                      |             |         |      |         |\n"
      "| image_cache_size_in_bytes            |        1234 |  0.0 MB |  GPU | CmdLine |\n"
      "|______________________________________|_____________|_________|______|_________|\n"
      "|                                      |             |         |      |         |\n"
      "| javascript_gc_threshold_in_bytes     |        1112 |  0.0 MB |  CPU | AutoSet |\n"
      "|______________________________________|_____________|_________|______|_________|\n"
      "|                                      |             |         |      |         |\n"
      "| skia_atlas_texture_dimensions        | 1234x4567x2 | 10.7 MB |  GPU | CmdLine |\n"
      "|______________________________________|_____________|_________|______|_________|\n"
      "|                                      |             |         |      |         |\n"
      "| skia_cache_size_in_bytes             |    12345678 | 11.8 MB |  GPU | CmdLine |\n"
      "|______________________________________|_____________|_________|______|_________|\n"
      "|                                      |             |         |      |         |\n"
      "| software_surface_cache_size_in_bytes |         N/A |     N/A |  N/A |     N/A |\n"
      "|______________________________________|_____________|_________|______|_________|\n";

  EXPECT_STREQ(expected_string, actual_string.c_str()) << actual_string;
}

TEST(MemorySettingsPrettyPrint, GenerateMemoryWithInvalidGpuMemoryConsumption) {
  IntSetting cpu_memory_setting("max_cpu_memory");
  cpu_memory_setting.set_value(
      MemorySetting::kBuildSetting, 256 * 1024 * 1024);
  IntSetting gpu_memory_setting("max_gpu_memory");
  gpu_memory_setting.set_value(MemorySetting::kStarboardAPI, 0);

  const base::optional<int64_t> no_gpu_memory;
  std::string actual_output = GenerateMemoryTable(
      cpu_memory_setting,  // 256 MB CPU available.
      gpu_memory_setting,  // Signals that no gpu memory is available
                           //   on this system.
      128 * 1024 * 1024,   // 128 MB CPU consumption.
      16 * 1024 * 1024);   // 16 MB GPU consumption.

  const char* expected_output =
      " MEMORY           SOURCE          TOTAL       SETTINGS CONSUME   \n"
      " _______________________________________________________________ \n"
      "|                |               |           |                  |\n"
      "| max_cpu_memory |         Build |  256.0 MB |         128.0 MB |\n"
      "|________________|_______________|___________|__________________|\n"
      "|                |               |           |                  |\n"
      "| max_gpu_memory | Starboard API | <UNKNOWN> |          16.0 MB |\n"
      "|________________|_______________|___________|__________________|\n";

  EXPECT_STREQ(expected_output, actual_output.c_str()) << actual_output;
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
