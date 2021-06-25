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

#include "cobalt/browser/memory_settings/pretty_print.h"

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "cobalt/browser/memory_settings/memory_settings.h"
#include "cobalt/browser/memory_settings/test_common.h"
#include "cobalt/browser/switches.h"
#include "starboard/common/log.h"
#include "starboard/memory.h"
#include "starboard/string.h"
#include "starboard/system.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace browser {
namespace memory_settings {

namespace {
// Returns true if all tokens exist in the string in the given order.
bool HasTokensInOrder(const std::string& value,
                      std::initializer_list<const char*> tokens) {
  std::string::size_type current_position = 0;
  for (auto token : tokens) {
    std::string::size_type position = value.find(token, current_position);
    EXPECT_NE(position, std::string::npos);
    EXPECT_GE(position, current_position);
    if (position == std::string::npos) {
      SB_DLOG(INFO) << "Token \"" << token << "\" not found in order.";
      return false;
    }
    current_position = position + strlen(token);
  }
  return true;
}
}  // namespace

TEST(MemorySettingsPrettyPrint, GeneratePrettyPrintTable) {
  TestSettingGroup setting_group;
  setting_group.LoadDefault();
  std::string actual_string =
      GeneratePrettyPrintTable(false, setting_group.AsConstVector());

  // clang-format off
  EXPECT_TRUE(HasTokensInOrder(
      actual_string,
      {"SETTING NAME", "VALUE", "TYPE", "SOURCE", "\n",
       "image_cache_size_in_bytes", "1234", "0.0 MB", "GPU", "CmdLine", "\n",
       "skia_atlas_texture_dimensions", "1234x4567x2", "10.7 MB", "GPU", "CmdLine", "\n",  // NOLINT(whitespace/line_length)
       "skia_cache_size_in_bytes", "12345678", "11.8 MB", "GPU", "CmdLine", "\n",  // NOLINT(whitespace/line_length)
       "software_surface_cache_size_in_bytes", "N/A", "N/A", "N/A", "N/A", "\n"
      }));
  // clang-format on
}

TEST(MemorySettingsPrettyPrint, GenerateMemoryTableWithUnsetGpuMemory) {
  IntSetting cpu_memory_setting("max_cpu_memory");
  cpu_memory_setting.set_value(MemorySetting::kBuildSetting, 256 * 1024 * 1024);
  IntSetting gpu_memory_setting("max_gpu_memory");

  std::string actual_output =
      GenerateMemoryTable(false,               // No color.
                          cpu_memory_setting,  // 256 MB CPU available
                          gpu_memory_setting,
                          128 * 1024 * 1024,  // 128 MB CPU consumption
                          0);                 // 0 MB GPU consumption.

  // clang-format off
  EXPECT_TRUE(HasTokensInOrder(
      actual_output, {"MEMORY", "SOURCE", "TOTAL", "SETTINGS CONSUME", "\n",
                      "max_cpu_memory", "Build", "256.0 MB", "128.0 MB", "\n",
                      "max_gpu_memory", "Unset", "<UNKNOWN>", "0.0 MB", "\n"}));
  // clang-format on
}

TEST(MemorySettingsPrettyPrint, GenerateMemoryTableWithGpuMemory) {
  IntSetting cpu_memory_setting("max_cpu_memory");
  cpu_memory_setting.set_value(MemorySetting::kBuildSetting, 256 * 1024 * 1024);
  IntSetting gpu_memory_setting("max_gpu_memory");
  gpu_memory_setting.set_value(MemorySetting::kBuildSetting, 64 * 1024 * 1024);

  std::string actual_output =
      GenerateMemoryTable(false,               // No color.
                          cpu_memory_setting,  // 256 MB CPU available.
                          gpu_memory_setting,  // 64 MB GPU available.
                          128 * 1024 * 1024,   // 128 MB CPU consumption.
                          23592960);           // 22.5 MB GPU consumption.

  // clang-format off
  EXPECT_TRUE(HasTokensInOrder(
      actual_output, {"MEMORY", "SOURCE", "TOTAL", "SETTINGS CONSUME", "\n",
                      "max_cpu_memory", "Build", "256.0 MB", "128.0 MB", "\n",
                      "max_gpu_memory", "Build", "64.0 MB", "22.5 MB", "\n"}));
  // clang-format on
}

TEST(MemorySettingsPrettyPrint, GenerateMemoryWithInvalidGpuMemoryConsumption) {
  IntSetting cpu_memory_setting("max_cpu_memory");
  cpu_memory_setting.set_value(MemorySetting::kBuildSetting, 256 * 1024 * 1024);
  IntSetting gpu_memory_setting("max_gpu_memory");
  gpu_memory_setting.set_value(MemorySetting::kStarboardAPI, 0);

  std::string actual_output = GenerateMemoryTable(
      false,               // No color.
      cpu_memory_setting,  // 256 MB CPU available.
      gpu_memory_setting,  // Signals that no gpu memory is available
                           //   on this system.
      128 * 1024 * 1024,   // 128 MB CPU consumption.
      16 * 1024 * 1024);   // 16 MB GPU consumption.

  // clang-format off
  EXPECT_TRUE(HasTokensInOrder(
      actual_output, {"MEMORY", "SOURCE", "TOTAL", "SETTINGS CONSUME", "\n",
                      "max_cpu_memory", "Build", "256.0 MB", "128.0 MB", "\n",
                      "max_gpu_memory", "Starboard API", "<UNKNOWN>", "16.0 MB", "\n"  // NOLINT(whitespace/line_length)
                     }));
  // clang-format on
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
