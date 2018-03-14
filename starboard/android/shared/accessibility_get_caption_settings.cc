// Copyright 2017 Google Inc. All Rights Reserved.
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

#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>

#include "base/compiler_specific.h"
#include "base/logging.h"

#include "starboard/accessibility.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/configuration.h"
#include "starboard/log.h"
#include "starboard/memory.h"

using starboard::android::shared::JniEnvExt;
using starboard::android::shared::ScopedLocalJavaRef;

namespace {

const int kRgbWhite = 0xFFFFFF;
const int kRgbBlack = 0x000000;
const int kRgbRed = 0xFF0000;
const int kRgbYellow = 0xFFFF00;
const int kRgbGreen = 0x00FF00;
const int kRgbCyan = 0x00FFFF;
const int kRgbBlue = 0x0000FF;
const int kRgbMagenta = 0xFF00FF;

const int kRgbColors[] = {
  kRgbWhite,
  kRgbBlack,
  kRgbRed,
  kRgbYellow,
  kRgbGreen,
  kRgbCyan,
  kRgbBlue,
  kRgbMagenta,
};

SbAccessibilityCaptionColor GetClosestCaptionColor(int color) {
  int ref_color = kRgbWhite;
  int min_distance = std::numeric_limits<int>::max();

  int r = 0xFF & (color >> 16);
  int g = 0xFF & (color >> 8);
  int b = 0xFF & (color);

  // Find the reference color with the least distance (squared).
  for (int i = 0; i < SB_ARRAY_SIZE(kRgbColors); i++) {
    int r_ref = 0xFF & (kRgbColors[i] >> 16);
    int g_ref = 0xFF & (kRgbColors[i] >> 8);
    int b_ref = 0xFF & (kRgbColors[i]);
    int distance_squared = pow(r - r_ref, 2) +
                           pow(g - g_ref, 2) +
                           pow(b - b_ref, 2);
    if (distance_squared < min_distance) {
      ref_color = kRgbColors[i];
      min_distance = distance_squared;
    }
  }

  switch (ref_color) {
    case kRgbWhite:
      return kSbAccessibilityCaptionColorWhite;
    case kRgbBlack:
      return kSbAccessibilityCaptionColorBlack;
    case kRgbRed:
      return kSbAccessibilityCaptionColorRed;
    case kRgbYellow:
      return kSbAccessibilityCaptionColorYellow;
    case kRgbGreen:
      return kSbAccessibilityCaptionColorGreen;
    case kRgbCyan:
      return kSbAccessibilityCaptionColorCyan;
    case kRgbBlue:
      return kSbAccessibilityCaptionColorBlue;
    case kRgbMagenta:
      return kSbAccessibilityCaptionColorMagenta;
    default:
      NOTREACHED() << "Invalid RGB color conversion";
      return kSbAccessibilityCaptionColorWhite;
  }
}

SbAccessibilityCaptionCharacterEdgeStyle
AndroidEdgeTypeToSbEdgeStyle(int edge_type) {
  switch (edge_type) {
    case 0:
      return kSbAccessibilityCaptionCharacterEdgeStyleNone;
    case 1:
      return kSbAccessibilityCaptionCharacterEdgeStyleUniform;
    case 2:
      return kSbAccessibilityCaptionCharacterEdgeStyleDropShadow;
    case 3:
      return kSbAccessibilityCaptionCharacterEdgeStyleRaised;
    case 4:
      return kSbAccessibilityCaptionCharacterEdgeStyleDepressed;
    default:
      NOTREACHED() << "Invalid edge type conversion";
      return kSbAccessibilityCaptionCharacterEdgeStyleNone;
  }
}

SbAccessibilityCaptionFontFamily AndroidFontFamilyToSbFontFamily(int family) {
  switch (family) {
    case 0:
      return kSbAccessibilityCaptionFontFamilyCasual;
    case 1:
      return kSbAccessibilityCaptionFontFamilyCursive;
    case 2:
      return kSbAccessibilityCaptionFontFamilyMonospaceSansSerif;
    case 3:
      return kSbAccessibilityCaptionFontFamilyMonospaceSerif;
    case 4:
      return kSbAccessibilityCaptionFontFamilyProportionalSansSerif;
    case 5:
      return kSbAccessibilityCaptionFontFamilyProportionalSerif;
    case 6:
      return kSbAccessibilityCaptionFontFamilySmallCapitals;
    default:
      NOTREACHED() << "Invalid font family conversion";
      return kSbAccessibilityCaptionFontFamilyCasual;
  }
}

int FindClosestReferenceValue(int value, const int reference[],
                              size_t reference_size) {
  int result = reference[0];
  int min_difference = std::numeric_limits<int>::max();

  for (int i = 0; i < reference_size; i++) {
    int difference = abs(reference[i] - value);
    if (difference < min_difference) {
      result = reference[i];
      min_difference = difference;
    }
  }
  return result;
}

const int kFontSizes[] = {
  25,
  50,
  75,
  100,
  125,
  150,
  175,
  200,
  225,
  250,
  275,
  300
};

SbAccessibilityCaptionFontSizePercentage GetClosestFontSizePercentage(
    int font_size_percent) {
  int reference_size = FindClosestReferenceValue(
      font_size_percent, kFontSizes, SB_ARRAY_SIZE(kFontSizes));
  switch (reference_size) {
    case 25:
      return kSbAccessibilityCaptionFontSizePercentage25;
    case 50:
      return kSbAccessibilityCaptionFontSizePercentage50;
    case 75:
      return kSbAccessibilityCaptionFontSizePercentage75;
    case 100:
      return kSbAccessibilityCaptionFontSizePercentage100;
    case 125:
      return kSbAccessibilityCaptionFontSizePercentage125;
    case 150:
      return kSbAccessibilityCaptionFontSizePercentage150;
    case 175:
      return kSbAccessibilityCaptionFontSizePercentage175;
    case 200:
      return kSbAccessibilityCaptionFontSizePercentage200;
    case 225:
      return kSbAccessibilityCaptionFontSizePercentage225;
    case 250:
      return kSbAccessibilityCaptionFontSizePercentage250;
    case 275:
      return kSbAccessibilityCaptionFontSizePercentage275;
    case 300:
      return kSbAccessibilityCaptionFontSizePercentage300;
    default:
      NOTREACHED() << "Invalid font size";
      return kSbAccessibilityCaptionFontSizePercentage100;
  }
}

const int kOpacities[] = {
  0,
  25,
  50,
  75,
  100,
};

SbAccessibilityCaptionOpacityPercentage GetClosestOpacity(int opacity_percent) {
  int reference_opacity_percent = FindClosestReferenceValue(
      opacity_percent, kOpacities, SB_ARRAY_SIZE(kOpacities));
  switch (reference_opacity_percent) {
    case 0:
      return kSbAccessibilityCaptionOpacityPercentage0;
    case 25:
      return kSbAccessibilityCaptionOpacityPercentage25;
    case 50:
      return kSbAccessibilityCaptionOpacityPercentage50;
    case 75:
      return kSbAccessibilityCaptionOpacityPercentage75;
    case 100:
      return kSbAccessibilityCaptionOpacityPercentage100;
    default:
      NOTREACHED() << "Invalid opacity percentage";
      return kSbAccessibilityCaptionOpacityPercentage100;
  }
}

SbAccessibilityCaptionState BooleanToCaptionState(bool is_set) {
  if (is_set) {
    return kSbAccessibilityCaptionStateSet;
  } else {
    return kSbAccessibilityCaptionStateUnset;
  }
}

void SetColorProperties(jobject j_caption_settings,
                        const char* color_field,
                        const char* has_color_field,
                        SbAccessibilityCaptionColor* color,
                        SbAccessibilityCaptionState* color_state,
                        SbAccessibilityCaptionOpacityPercentage* opacity,
                        SbAccessibilityCaptionState* opacity_state) {
  JniEnvExt* env = JniEnvExt::Get();
  jint j_color = env->GetIntFieldOrAbort(j_caption_settings, color_field, "I");
  *color = GetClosestCaptionColor(j_color);
  *opacity = GetClosestOpacity((0xFF & (j_color >> 24)) * 100 / 255);
  *color_state = BooleanToCaptionState(
      env->GetBooleanFieldOrAbort(j_caption_settings, has_color_field, "Z"));
  // Color and opacity are combined into a single ARGB value.
  // Therefore, if the color is set, so is the opacity.
  *opacity_state = *color_state;
}

}  // namespace

bool SbAccessibilityGetCaptionSettings(
  SbAccessibilityCaptionSettings* caption_settings) {
  if (!caption_settings ||
      !SbMemoryIsZero(caption_settings,
                      sizeof(SbAccessibilityCaptionSettings))) {
    return false;
  }

  JniEnvExt* env = JniEnvExt::Get();

  ScopedLocalJavaRef<jobject> j_caption_settings(
      env->CallStarboardObjectMethodOrAbort(
          "getCaptionSettings", "()Ldev/cobalt/media/CaptionSettings;"));

  jfloat font_scale =
      env->GetFloatFieldOrAbort(j_caption_settings.Get(), "fontScale", "F");
  caption_settings->font_size =
      GetClosestFontSizePercentage(100.0 * font_scale);
  // Android's captioning API always returns a font scale of 1 (100%) if
  // the font size has not been set. This means we have no way to check if
  // font size is "set" vs. "unset", so we'll just return "set" every time.
  caption_settings->font_size_state = kSbAccessibilityCaptionStateSet;

  // TODO: Convert Android typeface to font family.
  caption_settings->font_family = kSbAccessibilityCaptionFontFamilyCasual;
  caption_settings->font_family_state = kSbAccessibilityCaptionStateUnsupported;

  caption_settings->character_edge_style = AndroidEdgeTypeToSbEdgeStyle(
      env->GetIntFieldOrAbort(j_caption_settings.Get(), "edgeType", "I"));
  caption_settings->character_edge_style_state = BooleanToCaptionState(
      env->GetBooleanFieldOrAbort(j_caption_settings.Get(),
                                  "hasEdgeType", "Z"));

  SetColorProperties(
      j_caption_settings.Get(), "foregroundColor", "hasForegroundColor",
      &caption_settings->font_color,
      &caption_settings->font_color_state,
      &caption_settings->font_opacity,
      &caption_settings->font_opacity_state);

  SetColorProperties(
      j_caption_settings.Get(), "backgroundColor", "hasBackgroundColor",
      &caption_settings->background_color,
      &caption_settings->background_color_state,
      &caption_settings->background_opacity,
      &caption_settings->background_opacity_state);

  SetColorProperties(
      j_caption_settings.Get(), "windowColor", "hasWindowColor",
      &caption_settings->window_color,
      &caption_settings->window_color_state,
      &caption_settings->window_opacity,
      &caption_settings->window_opacity_state);

  caption_settings->is_enabled =
     env->GetBooleanFieldOrAbort(j_caption_settings.Get(), "isEnabled", "Z");
  caption_settings->supports_is_enabled = true;
  caption_settings->supports_set_enabled = false;

  return true;
}
