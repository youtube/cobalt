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
#include "cobalt/browser/memory_settings/test_common.h"
#include "cobalt/browser/switches.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/system.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace browser {
namespace memory_settings {

TEST(MemorySettingsPrettyPrint, GeneratePrettyPrintTable) {
  TestSettingGroup setting_group;
  setting_group.LoadDefault();
  std::string actual_string =
      GeneratePrettyPrintTable(false, setting_group.AsConstVector());

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
      GenerateMemoryTable(false,               // No color.
                          cpu_memory_setting,  // 256 MB CPU available
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
      GenerateMemoryTable(false,               // No color.
                          cpu_memory_setting,  // 256 MB CPU available.
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
      false,  // No color.
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
      false,               // No color.
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
