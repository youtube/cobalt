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

#include "cobalt/browser/memory_settings/auto_mem_settings.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "cobalt/browser/memory_settings/test_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace browser {
namespace memory_settings {
namespace {
base::CommandLine MakeCommandLine(const std::string& name,
                                  const std::string& value) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(name, value);
  return command_line;
}

void TestParseInt(const base::Optional<int64_t>& expected,
                  const std::string& value, const std::string& name,
                  base::Optional<int64_t>* setting) {
#define TEST_EXTRAS                            \
  "expected=" << expected.value() << ", "      \
              << "name=\"" << name << "\", "   \
              << "value=\"" << value << "\", " \
              << "setting=" << setting->value()

  if (!expected) {
    EXPECT_FALSE(*setting) << TEST_EXTRAS;
    return;
  }

  EXPECT_TRUE(*setting) << TEST_EXTRAS;
  if (!*setting) {
    // Check and quit so we can continue on failure without a crash.
    return;
  }
  EXPECT_EQ(expected.value(), **setting) << TEST_EXTRAS;
#undef TEST_EXTRAS
}

void TestAllParseInt(const base::Optional<int64_t>& expected,
                     const std::string& value) {
#define TEST_PARSE_INT(expected, value, switch_name, field_name)      \
  {                                                                   \
    AutoMemSettings settings =                                        \
        GetSettings(MakeCommandLine(switch_name, value));             \
    TestParseInt(expected, value, switch_name, &settings.field_name); \
  }

  TEST_PARSE_INT(expected, value, switches::kImageCacheSizeInBytes,
                 cobalt_image_cache_size_in_bytes);
  TEST_PARSE_INT(expected, value, switches::kRemoteTypefaceCacheSizeInBytes,
                 remote_typeface_cache_capacity_in_bytes);
  TEST_PARSE_INT(expected, value, switches::kSkiaCacheSizeInBytes,
                 skia_cache_size_in_bytes);
  TEST_PARSE_INT(expected, value, switches::kSoftwareSurfaceCacheSizeInBytes,
                 software_surface_cache_size_in_bytes);
  TEST_PARSE_INT(expected, value, switches::kOffscreenTargetCacheSizeInBytes,
                 offscreen_target_cache_size_in_bytes);
  TEST_PARSE_INT(expected, value, switches::kMaxCobaltCpuUsage,
                 max_cpu_in_bytes);
  TEST_PARSE_INT(expected, value, switches::kMaxCobaltGpuUsage,
                 max_gpu_in_bytes);
#undef TEST_PARSE_INT
}

void TestParseDimensions(const base::Optional<TextureDimensions>& expected,
                         const std::string& value) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kSkiaTextureAtlasDimensions, value);
  AutoMemSettings settings = GetSettings(command_line);
  if (!expected) {
    EXPECT_FALSE(settings.skia_texture_atlas_dimensions);
    return;
  }
  EXPECT_TRUE(settings.skia_texture_atlas_dimensions)
      << " value=\"" << value << "\", expected=" << *expected;
  if (!settings.skia_texture_atlas_dimensions) {
    return;
  }
  EXPECT_EQ(expected->width(), settings.skia_texture_atlas_dimensions->width())
      << " value=\"" << value << "\", expected=" << *expected;
  EXPECT_EQ(expected->height(),
            settings.skia_texture_atlas_dimensions->height())
      << " value=\"" << value << "\", expected=" << *expected;
  EXPECT_EQ(expected->bytes_per_pixel(),
            settings.skia_texture_atlas_dimensions->bytes_per_pixel())
      << " value=\"" << value << "\", expected=" << *expected;
}
}  // namespace

TEST(AutoMemSettingsTest, InitialState) {
  AutoMemSettings settings(AutoMemSettings::kTypeCommandLine);
  EXPECT_EQ(AutoMemSettings::kTypeCommandLine, settings.type);
  EXPECT_FALSE(settings.cobalt_image_cache_size_in_bytes);
  EXPECT_FALSE(settings.remote_typeface_cache_capacity_in_bytes);
  EXPECT_FALSE(settings.skia_cache_size_in_bytes);
  EXPECT_FALSE(settings.skia_texture_atlas_dimensions);
  EXPECT_FALSE(settings.software_surface_cache_size_in_bytes);
  EXPECT_FALSE(settings.offscreen_target_cache_size_in_bytes);
  EXPECT_FALSE(settings.max_cpu_in_bytes);
  EXPECT_FALSE(settings.max_gpu_in_bytes);

  AutoMemSettings build_settings(AutoMemSettings::kTypeBuild);
  EXPECT_EQ(AutoMemSettings::kTypeBuild, build_settings.type);
  EXPECT_FALSE(build_settings.max_cpu_in_bytes);
}

// Tests the expectation that numerous string variations (whole numbers vs
// fractions vs units) parse correctly.
TEST(AutoMemSettingsTest, ParseIntegers) {
  // Autoset
  TestAllParseInt(-1, "-1");
  TestAllParseInt(-1, "auto");
  TestAllParseInt(-1, "Auto");
  TestAllParseInt(-1, "AUTO");
  TestAllParseInt(-1, "autoset");
  TestAllParseInt(-1, "AutoSet");
  TestAllParseInt(-1, "AUTOSET");

  // Bytes.
  TestAllParseInt(1, "1");
  TestAllParseInt(1, "1B");
  TestAllParseInt(1, "1b");
  TestAllParseInt(1, "1B");
  TestAllParseInt(1, "1b");

  // Kilobytes and fractional amounts.
  TestAllParseInt(1024, "1KB");
  TestAllParseInt(1024, "1Kb");
  TestAllParseInt(1024, "1kB");
  TestAllParseInt(1024, "1kb");

  TestAllParseInt(512, ".5kb");
  TestAllParseInt(512, "0.5kb");
  TestAllParseInt(1536, "1.5kb");
  TestAllParseInt(1536, "1.50kb");

  // Megabytes and fractional amounts.
  TestAllParseInt(1024 * 1024, "1MB");
  TestAllParseInt(1024 * 1024, "1Mb");
  TestAllParseInt(1024 * 1024, "1mB");
  TestAllParseInt(1024 * 1024, "1mb");

  TestAllParseInt(512 * 1024, ".5mb");
  TestAllParseInt(512 * 1024, "0.5mb");
  TestAllParseInt(1536 * 1024, "1.5mb");
  TestAllParseInt(1536 * 1024, "1.50mb");

  // Gigabytes and fractional amounts.
  TestAllParseInt(1024 * 1024 * 1024, "1GB");
  TestAllParseInt(1024 * 1024 * 1024, "1Gb");
  TestAllParseInt(1024 * 1024 * 1024, "1gB");
  TestAllParseInt(1024 * 1024 * 1024, "1gb");

  TestAllParseInt(512 * 1024 * 1024, ".5gb");
  TestAllParseInt(1536 * 1024 * 1024, "1.50gb");
}

TEST(AutoMemSettingsTest, ParseDimensionsLower) {
  TestParseDimensions(TextureDimensions(1234, 5678, 2), "1234x5678");
}

TEST(AutoMemSettingsTest, ParseDimensionsOne) {
  TestParseDimensions(base::nullopt, "1234");
}

TEST(AutoMemSettingsTest, ParseDimensionsOneX) {
  TestParseDimensions(base::nullopt, "1234x");
}

TEST(AutoMemSettingsTest, ParseDimensionsTwoX) {
  TestParseDimensions(base::nullopt, "1234x5678x");
}

TEST(AutoMemSettingsTest, ParseDimensionsBadNum1) {
  TestParseDimensions(base::nullopt, "ABCDx1234");
}

TEST(AutoMemSettingsTest, ParseDimensionsBadNum2) {
  TestParseDimensions(base::nullopt, "1234xABCD");
}

TEST(AutoMemSettingsTest, ParseDimensionsUpper) {
  TestParseDimensions(TextureDimensions(1234, 5678, 2), "1234X5678");
}

TEST(AutoMemSettingsTest, ParseDimensionsBpp) {
  TestParseDimensions(TextureDimensions(1234, 5678, 12), "1234X5678X12");
  TestParseDimensions(TextureDimensions(1234, 5678, 1), "1234X5678x1");
  TestParseDimensions(TextureDimensions(1234, 5678, 2), "1234x5678x2");
  TestParseDimensions(TextureDimensions(1234, 5678, 3), "1234x5678X3");
}

TEST(AutoMemSettingsTest, ParseDimensionsAuto) {
  TestParseDimensions(TextureDimensions(-1, -1, -1), "-1");
  TestParseDimensions(TextureDimensions(-1, -1, -1), "auto");
  TestParseDimensions(TextureDimensions(-1, -1, -1), "Auto");
  TestParseDimensions(TextureDimensions(-1, -1, -1), "AUTO");
  TestParseDimensions(TextureDimensions(-1, -1, -1), "autoset");
  TestParseDimensions(TextureDimensions(-1, -1, -1), "AutoSet");
  TestParseDimensions(TextureDimensions(-1, -1, -1), "AUTOSET");
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
