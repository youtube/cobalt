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

#include "base/bind.h"
#include "base/callback.h"
#include "cobalt/browser/memory_settings/constrainer.h"
#include "cobalt/browser/memory_settings/memory_settings.h"
#include "cobalt/browser/memory_settings/test_common.h"
#include "starboard/configuration.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace browser {
namespace memory_settings {
namespace {
const int64_t kOneMegabyte = 1 * 1024 * 1024;
const int64_t kTwoMegabytes = 2 * 1024 * 1024;
const int64_t kFiveMegabytes = 5 * 1024 * 1024;
const base::optional<int64_t> kNoGpuMemory;

ScalingFunction MakeLinearConstrainer() {
  // Linearly scale, but clamp between 0 and 1.
  return MakeLinearMemoryScaler(0.0, 1.0);
}

}  // namespace.

// Tests the expectation that given one IntSetting which occupies 2MB of
// the available memory, and max_cpu_setting of 1MB, that
// ConstrainToMemoryLimits() function will reduce the memory setting
// down 1MB.
TEST(ConstrainToMemoryLimits, ConstrainCpuMemoryWithOneSetting) {
  IntSetting int_setting("dummy_cpu_setting");
  int_setting.set_memory_type(MemorySetting::kCPU);
  int_setting.set_value(MemorySetting::kAutoSet, kTwoMegabytes);

  ScalingFunction constrainer(MakeLinearConstrainer());

  int_setting.set_memory_scaling_function(constrainer);

  std::vector<MemorySetting*> settings;
  settings.push_back(&int_setting);

  std::vector<std::string> error_msgs;

  // Will reduce the memory usage of the IntSetting.
  ConstrainToMemoryLimits(kOneMegabyte, kNoGpuMemory, &settings, &error_msgs);
  EXPECT_EQ(kOneMegabyte, int_setting.MemoryConsumption());
  EXPECT_TRUE(error_msgs.empty());
}

// Tests the expectation that given two IntSettings, one that was AutoSet and
// one that was set by the command line and one set by the build system, that
// only the AutoSet IntSetting will be constrained.
TEST(ConstrainToMemoryLimits, ConstrainerIgnoresNonAutosetVariables) {
  IntSetting int_setting_autoset("autoset_cpu_setting");
  int_setting_autoset.set_memory_type(MemorySetting::kCPU);
  int_setting_autoset.set_value(MemorySetting::kAutoSet, kTwoMegabytes);
  int_setting_autoset.set_memory_scaling_function(MakeLinearConstrainer());

  IntSetting int_setting_cmdline("cmdline_cpu_setting");
  int_setting_cmdline.set_memory_type(MemorySetting::kCPU);
  int_setting_cmdline.set_value(MemorySetting::kCmdLine, kTwoMegabytes);
  int_setting_cmdline.set_memory_scaling_function(MakeLinearConstrainer());

  IntSetting int_setting_builtin("builtin_cpu_setting");
  int_setting_builtin.set_memory_type(MemorySetting::kCPU);
  int_setting_builtin.set_value(MemorySetting::kBuildSetting, kTwoMegabytes);
  int_setting_builtin.set_memory_scaling_function(MakeLinearConstrainer());

  std::vector<MemorySetting*> settings;
  settings.push_back(&int_setting_autoset);
  settings.push_back(&int_setting_cmdline);
  settings.push_back(&int_setting_builtin);

  std::vector<std::string> error_msgs;

  // Right now we need to shave off 1MB, but this can only come from the
  // MemorySetting that was autoset.
  ConstrainToMemoryLimits(kFiveMegabytes, kNoGpuMemory, &settings,
                          &error_msgs);

  EXPECT_EQ(kOneMegabyte, int_setting_autoset.MemoryConsumption());
  EXPECT_EQ(kTwoMegabytes, int_setting_cmdline.MemoryConsumption());
  EXPECT_EQ(kTwoMegabytes, int_setting_builtin.MemoryConsumption());

  EXPECT_TRUE(error_msgs.empty());
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
