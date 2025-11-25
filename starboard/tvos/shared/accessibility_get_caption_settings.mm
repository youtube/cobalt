// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#import <MediaAccessibility/MediaAccessibility.h>

#include "starboard/common/memory.h"
#include "starboard/shared/starboard/accessibility_internal.h"
#include "starboard/tvos/shared/accessibility_extension.h"

using starboard::shared::starboard::GetClosestCaptionColorRGB;
using starboard::shared::starboard::GetClosestFontSizePercentage;
using starboard::shared::starboard::GetClosestOpacity;

namespace starboard {
namespace shared {
namespace uikit {

namespace accessibility {
namespace {

constexpr MACaptionAppearanceDomain kUserDomain =
    kMACaptionAppearanceDomainUser;

SbAccessibilityCaptionCharacterEdgeStyle MAEdgeStyleToSbEdgeStyle(
    MACaptionAppearanceTextEdgeStyle edge_style) {
  switch (edge_style) {
    case kMACaptionAppearanceTextEdgeStyleNone:
      return kSbAccessibilityCaptionCharacterEdgeStyleNone;
    case kMACaptionAppearanceTextEdgeStyleRaised:
      return kSbAccessibilityCaptionCharacterEdgeStyleRaised;
    case kMACaptionAppearanceTextEdgeStyleDepressed:
      return kSbAccessibilityCaptionCharacterEdgeStyleDepressed;
    case kMACaptionAppearanceTextEdgeStyleUniform:
      return kSbAccessibilityCaptionCharacterEdgeStyleUniform;
    case kMACaptionAppearanceTextEdgeStyleDropShadow:
      return kSbAccessibilityCaptionCharacterEdgeStyleDropShadow;
    default:
      SB_NOTREACHED() << "Invalid edge type conversion";
      return kSbAccessibilityCaptionCharacterEdgeStyleNone;
  }
}

SbAccessibilityCaptionColor CGColorRefToClosestCaptionColor(
    CGColorRef cg_color) {
  SB_DCHECK(CGColorGetNumberOfComponents(cg_color) == 4);
  const CGFloat* components = CGColorGetComponents(cg_color);
  return GetClosestCaptionColorRGB(255.0 * components[0], 255.0 * components[1],
                                   255.0 * components[2]);
}

SbAccessibilityCaptionState MABehaviorToSbCaptionState(
    MACaptionAppearanceBehavior behavior) {
  if (behavior == kMACaptionAppearanceBehaviorUseValue) {
    // The preference setting should always be used.
    return kSbAccessibilityCaptionStateOverride;
  } else {
    // The preference setting should be used unless the content media being
    // played has its own custom value for this setting.
    return kSbAccessibilityCaptionStateSet;
  }
}

}  // namespace

bool GetCaptionSettings(SbAccessibilityCaptionSettings* caption_settings) {
  if (!caption_settings ||
      !common::MemoryIsZero(caption_settings,
                            sizeof(SbAccessibilityCaptionSettings))) {
    return false;
  }

  MACaptionAppearanceBehavior font_size_behavior;
  CGFloat font_scale = MACaptionAppearanceGetRelativeCharacterSize(
      kUserDomain, &font_size_behavior);
  caption_settings->font_size =
      GetClosestFontSizePercentage(100.0 * font_scale);
  caption_settings->font_size_state =
      MABehaviorToSbCaptionState(font_size_behavior);

  // There is an option in the system settings to choose a typeface; however,
  // there is currently no way to query that typeface without first specifying a
  // font style (which our case maps to a SbAccessibilityCaptionFontFamily).
  // We will leave as the default for now and mark as unsupported.
  caption_settings->font_family = kSbAccessibilityCaptionFontFamilyCasual;
  caption_settings->font_family_state = kSbAccessibilityCaptionStateUnsupported;

  MACaptionAppearanceBehavior character_edge_style_behavior;
  caption_settings->character_edge_style =
      MAEdgeStyleToSbEdgeStyle(MACaptionAppearanceGetTextEdgeStyle(
          kUserDomain, &character_edge_style_behavior));
  caption_settings->character_edge_style_state =
      MABehaviorToSbCaptionState(character_edge_style_behavior);

  MACaptionAppearanceBehavior font_color_behavior;
  caption_settings->font_color =
      CGColorRefToClosestCaptionColor(MACaptionAppearanceCopyForegroundColor(
          kUserDomain, &font_color_behavior));
  caption_settings->font_color_state =
      MABehaviorToSbCaptionState(font_color_behavior);

  MACaptionAppearanceBehavior font_opacity_behavior;
  caption_settings->font_opacity =
      GetClosestOpacity(MACaptionAppearanceGetForegroundOpacity(
                            kUserDomain, &font_opacity_behavior) *
                        100);
  caption_settings->font_opacity_state =
      MABehaviorToSbCaptionState(font_opacity_behavior);

  MACaptionAppearanceBehavior background_color_behavior;
  caption_settings->background_color =
      CGColorRefToClosestCaptionColor(MACaptionAppearanceCopyBackgroundColor(
          kUserDomain, &background_color_behavior));
  caption_settings->background_color_state =
      MABehaviorToSbCaptionState(font_color_behavior);

  MACaptionAppearanceBehavior background_opacity_behavior;
  caption_settings->background_opacity =
      GetClosestOpacity(MACaptionAppearanceGetBackgroundOpacity(
                            kUserDomain, &background_opacity_behavior) *
                        100);
  caption_settings->background_opacity_state =
      MABehaviorToSbCaptionState(background_opacity_behavior);

  MACaptionAppearanceBehavior window_color_behavior;
  caption_settings->window_color = CGColorRefToClosestCaptionColor(
      MACaptionAppearanceCopyWindowColor(kUserDomain, &window_color_behavior));
  caption_settings->window_color_state =
      MABehaviorToSbCaptionState(window_color_behavior);

  MACaptionAppearanceBehavior window_opacity_behavior;
  caption_settings->window_opacity =
      GetClosestOpacity(MACaptionAppearanceGetWindowOpacity(
                            kUserDomain, &window_opacity_behavior) *
                        100);
  caption_settings->window_opacity_state =
      MABehaviorToSbCaptionState(window_opacity_behavior);

  caption_settings->is_enabled =
      (MACaptionAppearanceGetDisplayType(kUserDomain) ==
       kMACaptionAppearanceDisplayTypeAlwaysOn);
  caption_settings->supports_is_enabled = true;
  caption_settings->supports_set_enabled = true;
  caption_settings->supports_override = true;

  return true;
}

}  // namespace accessibility
}  // namespace uikit
}  // namespace shared
}  // namespace starboard
