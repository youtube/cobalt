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
#include "cobalt/browser/switches.h"
#include "cobalt/math/size.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace browser {
namespace memory_settings {

const math::Size kResolution1080p(1920, 1080);

#define EXPECT_MEMORY_SETTING(SETTING, SOURCE, VALUE)                       \
  EXPECT_EQ(VALUE, SETTING->value()) << " failure for " << SETTING->name(); \
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
                          MemorySetting::kCmdLine, 1234);

    EXPECT_MEMORY_SETTING(auto_mem.javascript_gc_threshold_in_bytes(),
                          MemorySetting::kCmdLine, 2345);

    // Certain features are only available for the blitter, and some features
    // are disabled, vice versa.
    if (builtin_settings.has_blitter) {
      // When blitter is active then skia_atlas_texture_dimensions are
      // not applicable because this is an OpenGl egl feature.
      EXPECT_MEMORY_SETTING(auto_mem.skia_atlas_texture_dimensions(),
                            MemorySetting::kNotApplicable, math::Size(0, 0));

      // Skia cache is also an egl-only feature, which is not applicable
      // for blitter.
      EXPECT_MEMORY_SETTING(auto_mem.skia_cache_size_in_bytes(),
                            MemorySetting::kNotApplicable, 0);

      EXPECT_MEMORY_SETTING(auto_mem.software_surface_cache_size_in_bytes(),
                            MemorySetting::kCmdLine, 4567);
    } else {
      // Skia atlas is an egl-only feature and therefore enabled.
      EXPECT_MEMORY_SETTING(auto_mem.skia_atlas_texture_dimensions(),
                            MemorySetting::kCmdLine, math::Size(1234, 5678));

      // Skia cache is an egl-only feature therefore it is enabled for egl.
      EXPECT_MEMORY_SETTING(auto_mem.skia_cache_size_in_bytes(),
                            MemorySetting::kCmdLine, 3456);

      // Software surface cache is a blitter-only feature, therefore disabled
      // in egl.
      EXPECT_MEMORY_SETTING(auto_mem.software_surface_cache_size_in_bytes(),
                            MemorySetting::kNotApplicable, 0);
    }
  }
}

// Tests that skia atlas texture will be bind to the built in value, iff it has
// been set.
TEST(AutoMem, SkiaAtlasTextureAtlasSize) {
  CommandLine empty_command_line(CommandLine::NO_PROGRAM);
  BuildSettings builtin_settings;
  BuildSettings builtin_settings_with_default;

  builtin_settings_with_default.skia_texture_atlas_dimensions =
      math::Size(1234, 5678);

  AutoMem auto_mem(kResolution1080p, empty_command_line, builtin_settings);
  AutoMem auto_mem_with_default(kResolution1080p, empty_command_line,
                                builtin_settings_with_default);

  // Expect that when the skia_atlas_texture_dimensions is specified in the
  // build settings that it will bind to the auto-set value (computed from
  // CalculateSkiaGlyphAtlasTextureSize(...)).
  EXPECT_MEMORY_SETTING(auto_mem.skia_atlas_texture_dimensions(),
                        MemorySetting::kAutoSet,
                        CalculateSkiaGlyphAtlasTextureSize(kResolution1080p));

  // Expect that when the skia_atlas_texture_dimensions is specified in the
  // build settings that it will bind to the final value.
  EXPECT_MEMORY_SETTING(auto_mem_with_default.skia_atlas_texture_dimensions(),
                        MemorySetting::kBuildSetting, math::Size(1234, 5678));
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
      CalculateSoftwareSurfaceCacheSizeInBytes(kResolution1080p));

  EXPECT_MEMORY_SETTING(
      auto_mem_with_surface_cache.software_surface_cache_size_in_bytes(),
      MemorySetting::kBuildSetting, 1234);
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
                        MemorySetting::kAutoSet,
                        CalculateSkiaCacheSize(kResolution1080p));

  EXPECT_MEMORY_SETTING(auto_mem_with_skia_cache.skia_cache_size_in_bytes(),
                        MemorySetting::kBuildSetting, 1234);
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt

#undef EXPECT_MEMORY_SETTING
