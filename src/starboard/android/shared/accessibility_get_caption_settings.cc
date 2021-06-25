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

#include <cmath>
#include <cstdlib>
#include <string>

#include "starboard/accessibility.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/common/log.h"
#include "starboard/common/memory.h"
#include "starboard/configuration.h"
#include "starboard/shared/starboard/accessibility_internal.h"

using starboard::android::shared::JniEnvExt;
using starboard::android::shared::ScopedLocalJavaRef;
using starboard::shared::starboard::GetClosestCaptionColor;
using starboard::shared::starboard::GetClosestFontSizePercentage;
using starboard::shared::starboard::GetClosestOpacity;

namespace {

SbAccessibilityCaptionCharacterEdgeStyle AndroidEdgeTypeToSbEdgeStyle(
    int edge_type) {
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
      SB_NOTREACHED() << "Invalid edge type conversion";
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
      SB_NOTREACHED() << "Invalid font family conversion";
      return kSbAccessibilityCaptionFontFamilyCasual;
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
      !starboard::common::MemoryIsZero(
          caption_settings, sizeof(SbAccessibilityCaptionSettings))) {
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
  caption_settings->character_edge_style_state =
      BooleanToCaptionState(env->GetBooleanFieldOrAbort(
          j_caption_settings.Get(), "hasEdgeType", "Z"));

  SetColorProperties(
      j_caption_settings.Get(), "foregroundColor", "hasForegroundColor",
      &caption_settings->font_color, &caption_settings->font_color_state,
      &caption_settings->font_opacity, &caption_settings->font_opacity_state);

  SetColorProperties(j_caption_settings.Get(), "backgroundColor",
                     "hasBackgroundColor", &caption_settings->background_color,
                     &caption_settings->background_color_state,
                     &caption_settings->background_opacity,
                     &caption_settings->background_opacity_state);

  SetColorProperties(j_caption_settings.Get(), "windowColor", "hasWindowColor",
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
