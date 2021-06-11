// Copyright 2017 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/accessibility.h"
#include "starboard/memory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbAccessibilityTest, CanCallGetTextToSpeechSettings) {
  SbAccessibilityTextToSpeechSettings settings = {0};
  EXPECT_TRUE(SbAccessibilityGetTextToSpeechSettings(&settings));
}

TEST(SbAccessibilityTest, CallTextToSpeechWithInvalidArgument) {
  // |settings| should be zero-initialized.
  SbAccessibilityTextToSpeechSettings settings = {1};
  EXPECT_FALSE(SbAccessibilityGetTextToSpeechSettings(&settings));

  // Argument should not be NULL.
  EXPECT_FALSE(SbAccessibilityGetTextToSpeechSettings(NULL));
}

TEST(SbAccessibilityTest, CanCallGetDisplaySettings) {
  SbAccessibilityDisplaySettings settings = {0};
  EXPECT_TRUE(SbAccessibilityGetDisplaySettings(&settings));
}

TEST(SbAccessibilityTest, CallDisplayWithInvalidArgument) {
  // |settings| should be zero-initialized.
  SbAccessibilityDisplaySettings settings = {1};
  EXPECT_FALSE(SbAccessibilityGetDisplaySettings(&settings));

  // Argument should not be NULL.
  EXPECT_FALSE(SbAccessibilityGetDisplaySettings(NULL));
}

#if SB_API_VERSION >= 12 || SB_HAS(CAPTIONS)
TEST(SbAccessibilityTest, CallGetCaptionSettingsWithInvalidArgument) {
  // |settings| should be zero-initialized.
  const int kInvalidValue = 0xFE;
  SbAccessibilityCaptionSettings settings;
  memset(&settings, kInvalidValue, sizeof(settings));
  EXPECT_FALSE(SbAccessibilityGetCaptionSettings(&settings));

  // Argument should not be NULL.
  EXPECT_FALSE(SbAccessibilityGetCaptionSettings(NULL));
}

TEST(SbAccessibilityTest, GetCaptionSettingsReturnIsValid) {
  // |settings| should be zero-initialized.
  SbAccessibilityCaptionSettings settings;
  const int kValidInitialValue = 0;
  memset(&settings, kValidInitialValue, sizeof(settings));
  EXPECT_TRUE(SbAccessibilityGetCaptionSettings(&settings));

  if (settings.background_color_state !=
      kSbAccessibilityCaptionStateUnsupported) {
    EXPECT_GE(settings.background_color, kSbAccessibilityCaptionColorBlue);
    EXPECT_LE(settings.background_color, kSbAccessibilityCaptionColorYellow);
  }

  if (settings.background_opacity_state !=
      kSbAccessibilityCaptionStateUnsupported) {
    EXPECT_GE(settings.background_opacity,
              kSbAccessibilityCaptionOpacityPercentage0);
    EXPECT_LE(settings.background_opacity,
              kSbAccessibilityCaptionOpacityPercentage100);
  }

  if (settings.character_edge_style_state !=
      kSbAccessibilityCaptionStateUnsupported) {
    EXPECT_GE(settings.character_edge_style,
              kSbAccessibilityCaptionCharacterEdgeStyleNone);
    EXPECT_LE(settings.character_edge_style,
              kSbAccessibilityCaptionCharacterEdgeStyleDropShadow);
  }

  if (settings.font_color_state !=
      kSbAccessibilityCaptionStateUnsupported) {
    EXPECT_GE(settings.font_color, kSbAccessibilityCaptionColorBlue);
    EXPECT_LE(settings.font_color, kSbAccessibilityCaptionColorYellow);
  }

  if (settings.font_family_state !=
      kSbAccessibilityCaptionStateUnsupported) {
    EXPECT_GE(settings.font_family,
              kSbAccessibilityCaptionFontFamilyCasual);
    EXPECT_LE(settings.font_family,
              kSbAccessibilityCaptionFontFamilySmallCapitals);
  }

  if (settings.font_opacity_state !=
      kSbAccessibilityCaptionStateUnsupported) {
    EXPECT_GE(settings.font_opacity,
              kSbAccessibilityCaptionOpacityPercentage0);
    EXPECT_LE(settings.font_opacity,
              kSbAccessibilityCaptionOpacityPercentage100);
  }

  if (settings.font_size_state !=
      kSbAccessibilityCaptionStateUnsupported) {
    EXPECT_GE(settings.font_size, kSbAccessibilityCaptionFontSizePercentage25);
    EXPECT_LE(settings.font_size, kSbAccessibilityCaptionFontSizePercentage300);
  }

  if (settings.window_color_state !=
      kSbAccessibilityCaptionStateUnsupported) {
    EXPECT_GE(settings.window_color, kSbAccessibilityCaptionColorBlue);
    EXPECT_LE(settings.window_color, kSbAccessibilityCaptionColorYellow);
  }

  if (settings.window_opacity_state !=
      kSbAccessibilityCaptionStateUnsupported) {
    EXPECT_GE(settings.window_opacity,
              kSbAccessibilityCaptionOpacityPercentage0);
    EXPECT_LE(settings.window_opacity,
              kSbAccessibilityCaptionOpacityPercentage100);
  }
}

TEST(SbAccessibilityTest, CallSetCaptionsEnabled) {
  SbAccessibilityCaptionSettings settings;
  const int kValidInitialValue = 0;
  memset(&settings, kValidInitialValue, sizeof(settings));
  EXPECT_TRUE(SbAccessibilityGetCaptionSettings(&settings));

  if (settings.supports_is_enabled && settings.supports_set_enabled) {
    // Try changing the enabled state.
    EXPECT_TRUE(SbAccessibilitySetCaptionsEnabled(!settings.is_enabled));

    SbAccessibilityCaptionSettings settings2;
    memset(&settings2, kValidInitialValue, sizeof(settings2));
    EXPECT_TRUE(SbAccessibilityGetCaptionSettings(&settings2));
    EXPECT_NE(settings.is_enabled, settings2.is_enabled);

    // Reset the enabled state so the unit test doesn't propagate a new setting.
    EXPECT_TRUE(SbAccessibilitySetCaptionsEnabled(settings.is_enabled));

    SbAccessibilityCaptionSettings settings3;
    memset(&settings3, kValidInitialValue, sizeof(settings3));
    EXPECT_TRUE(SbAccessibilityGetCaptionSettings(&settings3));
    EXPECT_EQ(settings.is_enabled, settings3.is_enabled);
  }
}
#endif  // SB_API_VERSION >= 12 || SB_HAS(CAPTIONS)

}  // namespace
}  // namespace nplb
}  // namespace starboard
