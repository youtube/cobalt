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

scoped_ptr<MemorySetting> CreateMemorySetting(
    MemorySetting::ClassType class_type, MemorySetting::SourceType source_type,
    const std::string& name, const std::string& value) {
  scoped_ptr<MemorySetting> setting;

  switch (class_type) {
    case MemorySetting::kInt: {
      setting.reset(new IntSetting(name));
      break;
    }
    case MemorySetting::kDimensions: {
      setting.reset(new DimensionSetting(name));
      break;
    }
    default: {
      EXPECT_TRUE(false) << "Unexpected type " << class_type;
      setting.reset(new IntSetting(name));
      break;
    }
  }
  EXPECT_TRUE(setting->TryParseValue(source_type, value));
  return setting.Pass();
}

TEST(MemorySettingsPrettyPrint, ToString) {
  std::vector<const MemorySetting*> memory_settings_ptr;
  scoped_ptr<MemorySetting> image_cache_setting =
      CreateMemorySetting(MemorySetting::kInt, MemorySetting::kCmdLine,
                          switches::kImageCacheSizeInBytes, "1234");
  memory_settings_ptr.push_back(image_cache_setting.get());

  scoped_ptr<MemorySetting> js_gc_setting =
      CreateMemorySetting(MemorySetting::kInt, MemorySetting::kAutoSet,
                          switches::kJavaScriptGcThresholdInBytes, "1112");
  memory_settings_ptr.push_back(js_gc_setting.get());

  scoped_ptr<MemorySetting> skia_texture_setting =
      CreateMemorySetting(MemorySetting::kDimensions, MemorySetting::kCmdLine,
                          switches::kSkiaTextureAtlasDimensions, "1234x4567");
  memory_settings_ptr.push_back(skia_texture_setting.get());

  scoped_ptr<MemorySetting> skia_cache_setting =
      CreateMemorySetting(MemorySetting::kInt, MemorySetting::kNotApplicable,
                          switches::kSkiaCacheSizeInBytes, "0");
  memory_settings_ptr.push_back(skia_cache_setting.get());

  scoped_ptr<MemorySetting> software_surface_setting =
      CreateMemorySetting(MemorySetting::kInt, MemorySetting::kBuildSetting,
                          switches::kSoftwareSurfaceCacheSizeInBytes, "8910");
  memory_settings_ptr.push_back(software_surface_setting.get());

  std::string actual_string = GeneratePrettyPrintTable(memory_settings_ptr);

  std::string expected_string =
      "NAME                                   VALUE       SOURCE     \n"
      "+--------------------------------------+-----------+---------+\n"
      "| image_cache_size_in_bytes            |      1234 | CmdLine |\n"
      "+--------------------------------------+-----------+---------+\n"
      "| javascript_gc_threshold_in_bytes     |      1112 | AutoSet |\n"
      "+--------------------------------------+-----------+---------+\n"
      "| skia_atlas_texture_dimensions        | 1234x4567 | CmdLine |\n"
      "+--------------------------------------+-----------+---------+\n"
      "| skia_cache_size_in_bytes             |         0 |     N/A |\n"
      "+--------------------------------------+-----------+---------+\n"
      "| software_surface_cache_size_in_bytes |      8910 |   Build |\n"
      "+--------------------------------------+-----------+---------+\n";

  const bool strings_matched = (expected_string == actual_string);
  EXPECT_TRUE(strings_matched) << "Strings Mismatched:\n\n"
                               << "Actual:\n" << actual_string << "\n"
                               << "Expected:\n" << expected_string << "\n";
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
