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

#include "cobalt/browser/memory_settings/auto_mem.h"

#include <string>
#include <vector>

#include "base/optional.h"
#include "cobalt/browser/memory_settings/build_settings.h"
#include "cobalt/browser/memory_settings/calculations.h"
#include "cobalt/browser/memory_settings/test_common.h"
#include "cobalt/browser/switches.h"
#include "cobalt/math/size.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace browser {
namespace memory_settings {

const math::Size kResolution1080p(1920, 1080);

#define EXPECT_MEMORY_SETTING(SETTING, SOURCE, MEMORY_TYPE, VALUE)          \
  EXPECT_EQ(VALUE, SETTING->value()) << " failure for " << SETTING->name(); \
  EXPECT_EQ(MEMORY_TYPE, SETTING->memory_type()) << "failure for "          \
                                                 << SETTING->memory_type(); \
  EXPECT_EQ(SOURCE, SETTING->source_type()) << " failure for "              \
                                            << SETTING->name();

// Tests the expectation that the command-line overrides will be applied.
// Settings which are enabled/disabled when blitter is enabled/disabled are
// also tested.
TEST(AutoMem, CommandLineOverrides) {
  // Load up command line settings of command lines.
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kImageCacheSizeInBytes, "1234");
  command_line.AppendSwitchASCII(switches::kJavaScriptGcThresholdInBytes,
                                 "2345");
  command_line.AppendSwitchASCII(switches::kSkiaCacheSizeInBytes, "3456");
  command_line.AppendSwitchASCII(switches::kSkiaTextureAtlasDimensions,
                                 "1234x5678");
  command_line.AppendSwitchASCII(switches::kSoftwareSurfaceCacheSizeInBytes,
                                 "4567");

  for (int i = 0; i <= 1; ++i) {
    BuildSettings builtin_settings = GetDefaultBuildSettings();
    builtin_settings.has_blitter = (i == 0);

    AutoMem auto_mem(kResolution1080p, command_line, builtin_settings);

    // image_cache_size_in_bytes and javascript_gc_threshold_in_bytes settings
    // ignore the blitter type.
    EXPECT_MEMORY_SETTING(auto_mem.image_cache_size_in_bytes(),
                          MemorySetting::kCmdLine, MemorySetting::kGPU, 1234);

    EXPECT_MEMORY_SETTING(auto_mem.javascript_gc_threshold_in_bytes(),
                          MemorySetting::kCmdLine, MemorySetting::kCPU, 2345);

    // Certain features are only available for the blitter, and some features
    // are disabled, vice versa.
    if (builtin_settings.has_blitter) {
      // When blitter is active then skia_atlas_texture_dimensions are
      // not applicable because this is an OpenGl egl feature.
      EXPECT_MEMORY_SETTING(
          auto_mem.skia_atlas_texture_dimensions(), MemorySetting::kCmdLine,
          MemorySetting::kNotApplicable, TextureDimensions(0, 0, 0));
      // Skia cache is also an egl-only feature, which is not applicable
      // for blitter.
      EXPECT_MEMORY_SETTING(auto_mem.skia_cache_size_in_bytes(),
                            MemorySetting::kCmdLine,
                            MemorySetting::kNotApplicable, 0);

      EXPECT_MEMORY_SETTING(auto_mem.software_surface_cache_size_in_bytes(),
                            MemorySetting::kCmdLine, MemorySetting::kCPU, 4567);
    } else {
      // Skia atlas is an egl-only feature and therefore enabled.
      EXPECT_MEMORY_SETTING(auto_mem.skia_atlas_texture_dimensions(),
                            MemorySetting::kCmdLine, MemorySetting::kGPU,
                            TextureDimensions(1234, 5678, 2));

      // Skia cache is an egl-only feature therefore it is enabled for egl.
      EXPECT_MEMORY_SETTING(auto_mem.skia_cache_size_in_bytes(),
                            MemorySetting::kCmdLine, MemorySetting::kGPU, 3456);

      // Software surface cache is a blitter-only feature, therefore disabled
      // in egl.
      EXPECT_MEMORY_SETTING(auto_mem.software_surface_cache_size_in_bytes(),
                            MemorySetting::kCmdLine,
                            MemorySetting::kNotApplicable, 0);
    }
  }
}

// Tests the expectation that if the command line specifies that the variable
// is "autoset" that the builtin setting is overriden.
TEST(AutoMem, CommandLineSpecifiesAutoset) {
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kImageCacheSizeInBytes, "autoset");
  BuildSettings build_settings;
  build_settings.cobalt_image_cache_size_in_bytes = 1234;

  AutoMem auto_mem(kResolution1080p, command_line, build_settings);

  EXPECT_MEMORY_SETTING(auto_mem.image_cache_size_in_bytes(),
                        MemorySetting::kAutoSet, MemorySetting::kGPU,
                        CalculateImageCacheSize(kResolution1080p));
}

// Tests that skia atlas texture will be bind to the built in value, iff it has
// been set.
TEST(AutoMem, SkiaGlyphAtlasTextureSize) {
  CommandLine empty_command_line(CommandLine::NO_PROGRAM);
  BuildSettings builtin_settings;
  BuildSettings builtin_settings_with_default;

  builtin_settings_with_default.skia_texture_atlas_dimensions =
      TextureDimensions(1234, 5678, 2);

  AutoMem auto_mem(kResolution1080p, empty_command_line, builtin_settings);
  AutoMem auto_mem_with_default(kResolution1080p, empty_command_line,
                                builtin_settings_with_default);

  // Expect that when the skia_atlas_texture_dimensions is specified in the
  // build settings that it will bind to the auto-set value (computed from
  // CalculateSkiaGlyphAtlasTextureSize(...)).
  EXPECT_MEMORY_SETTING(auto_mem.skia_atlas_texture_dimensions(),
                        MemorySetting::kAutoSet, MemorySetting::kGPU,
                        CalculateSkiaGlyphAtlasTextureSize(kResolution1080p));

  // Expect that when the skia_atlas_texture_dimensions is specified in the
  // build settings that it will bind to the final value.
  EXPECT_MEMORY_SETTING(auto_mem_with_default.skia_atlas_texture_dimensions(),
                        MemorySetting::kBuildSetting, MemorySetting::kGPU,
                        TextureDimensions(1234, 5678, 2));
}

// Tests that software surface cache will be bind to the built in value, iff
// it has been set.
TEST(AutoMem, SoftwareSurfaceCacheSizeInBytes) {
  CommandLine empty_command_line(CommandLine::NO_PROGRAM);
  BuildSettings builtin_settings;
  BuildSettings builtin_settings_with_default;
  // Enable the setting by enabling the blitter.
  builtin_settings.has_blitter = true;
  builtin_settings_with_default.has_blitter = true;
  builtin_settings_with_default.software_surface_cache_size_in_bytes = 1234;

  AutoMem auto_mem(kResolution1080p, empty_command_line, builtin_settings);
  AutoMem auto_mem_with_surface_cache(kResolution1080p, empty_command_line,
                                      builtin_settings_with_default);

  // Expect that when the software_surface_cache_size_in_bytes is specified in
  // the/ build settings that it will bind to the auto-set value (computed from
  // CalculateSoftwareSurfaceCacheSizeInBytes(...)).
  EXPECT_MEMORY_SETTING(
      auto_mem.software_surface_cache_size_in_bytes(), MemorySetting::kAutoSet,
      MemorySetting::kCPU,
      CalculateSoftwareSurfaceCacheSizeInBytes(kResolution1080p));

  EXPECT_MEMORY_SETTING(
      auto_mem_with_surface_cache.software_surface_cache_size_in_bytes(),
      MemorySetting::kBuildSetting, MemorySetting::kCPU, 1234);
}

// Tests that skia cache will be bind to the built in value, iff
// it has been set.
TEST(AutoMem, SkiaCacheSizeInBytes) {
  CommandLine empty_command_line(CommandLine::NO_PROGRAM);
  BuildSettings builtin_settings;
  BuildSettings builtin_settings_with_default;
  builtin_settings_with_default.skia_cache_size_in_bytes = 1234;

  AutoMem auto_mem(kResolution1080p, empty_command_line, builtin_settings);
  AutoMem auto_mem_with_skia_cache(kResolution1080p, empty_command_line,
                                   builtin_settings_with_default);

  EXPECT_MEMORY_SETTING(auto_mem.skia_cache_size_in_bytes(),
                        MemorySetting::kAutoSet, MemorySetting::kGPU,
                        CalculateSkiaCacheSize(kResolution1080p));

  EXPECT_MEMORY_SETTING(auto_mem_with_skia_cache.skia_cache_size_in_bytes(),
                        MemorySetting::kBuildSetting, MemorySetting::kGPU,
                        1234);
}

TEST(AutoMem, AllMemorySettingsAreOrderedByName) {
  CommandLine empty_command_line(CommandLine::NO_PROGRAM);
  BuildSettings builtin_settings;

  AutoMem auto_mem(kResolution1080p, empty_command_line, builtin_settings);

  std::vector<const MemorySetting*> settings = auto_mem.AllMemorySettings();

  for (size_t i = 1; i < settings.size(); ++i) {
    ASSERT_LT(settings[i-1]->name(), settings[i]->name());
  }
}

// Tests the expectation that constraining the CPU memory to 40MB will result
// in AutoMem reducing the the memory footprint.
TEST(AutoMem, ConstrainedCPUEnvironment) {
  CommandLine empty_command_line(CommandLine::NO_PROGRAM);
  BuildSettings builtin_settings;
  builtin_settings.max_cpu_in_bytes = 40 * 1024 * 1024;

  AutoMem auto_mem(kResolution1080p, empty_command_line, builtin_settings);

  std::vector<const MemorySetting*> settings = auto_mem.AllMemorySettings();

  const int64_t cpu_memory_consumption =
      SumMemoryConsumption(MemorySetting::kCPU, settings);

  EXPECT_LE(cpu_memory_consumption, 40 * 1024 * 1024);
}

// Tests the expectation that constraining the CPU memory to 40MB will result
// in AutoMem reducing the the memory footprint.
TEST(AutoMem, ConstrainedGPUEnvironment) {
  CommandLine empty_command_line(CommandLine::NO_PROGRAM);
  BuildSettings builtin_settings;
  builtin_settings.max_gpu_in_bytes = 57 * 1024 * 1024;
  AutoMem auto_mem(kResolution1080p, empty_command_line, builtin_settings);

  std::vector<const MemorySetting*> settings = auto_mem.AllMemorySettings();
  const int64_t gpu_memory_consumption =
      SumMemoryConsumption(MemorySetting::kGPU, settings);
  EXPECT_LE(gpu_memory_consumption, 57 * 1024 * 1024);
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt

#undef EXPECT_MEMORY_SETTING
