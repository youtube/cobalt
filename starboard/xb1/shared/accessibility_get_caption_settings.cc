// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include <Windows.h>

#include "starboard/common/memory.h"
#include "starboard/shared/starboard/accessibility_internal.h"
#include "starboard/xb1/shared/accessibility_extension.h"

namespace starboard {
namespace xb1 {
namespace shared {
namespace accessibility {

namespace {

using namespace Windows::Media::ClosedCaptioning;

SbAccessibilityCaptionCharacterEdgeStyle
ClosedCaptionEdgeEffectToSbAccessibilityCaptionCharacterEdgeStyle(
    ClosedCaptionEdgeEffect edgeStyle) {
  switch (edgeStyle) {
    case ClosedCaptionEdgeEffect::Default:
      return kSbAccessibilityCaptionCharacterEdgeStyleNone;
    case ClosedCaptionEdgeEffect::None:
      return kSbAccessibilityCaptionCharacterEdgeStyleNone;
    case ClosedCaptionEdgeEffect::Raised:
      return kSbAccessibilityCaptionCharacterEdgeStyleRaised;
    case ClosedCaptionEdgeEffect::Depressed:
      return kSbAccessibilityCaptionCharacterEdgeStyleDepressed;
    case ClosedCaptionEdgeEffect::Uniform:
      return kSbAccessibilityCaptionCharacterEdgeStyleUniform;
    case ClosedCaptionEdgeEffect::DropShadow:
      return kSbAccessibilityCaptionCharacterEdgeStyleDropShadow;
  }
}

SbAccessibilityCaptionColor ClosedCaptionColorToSbAccessibilityCaptionColor(
    ClosedCaptionColor color) {
  switch (color) {
    case ClosedCaptionColor::Default:
      return kSbAccessibilityCaptionColorWhite;
    case ClosedCaptionColor::White:
      return kSbAccessibilityCaptionColorWhite;
    case ClosedCaptionColor::Black:
      return kSbAccessibilityCaptionColorBlack;
    case ClosedCaptionColor::Red:
      return kSbAccessibilityCaptionColorRed;
    case ClosedCaptionColor::Green:
      return kSbAccessibilityCaptionColorGreen;
    case ClosedCaptionColor::Blue:
      return kSbAccessibilityCaptionColorBlue;
    case ClosedCaptionColor::Yellow:
      return kSbAccessibilityCaptionColorYellow;
    case ClosedCaptionColor::Magenta:
      return kSbAccessibilityCaptionColorMagenta;
    case ClosedCaptionColor::Cyan:
      return kSbAccessibilityCaptionColorCyan;
  }
}

SbAccessibilityCaptionFontFamily
ClosedCaptionStyleToSbAccessibilityCaptionFontFamily(ClosedCaptionStyle style) {
  switch (style) {
    case ClosedCaptionStyle::Default:
      return kSbAccessibilityCaptionFontFamilyCasual;
    case ClosedCaptionStyle::MonospacedWithSerifs:
      return kSbAccessibilityCaptionFontFamilyMonospaceSerif;
    case ClosedCaptionStyle::ProportionalWithSerifs:
      return kSbAccessibilityCaptionFontFamilyProportionalSerif;
    case ClosedCaptionStyle::MonospacedWithoutSerifs:
      return kSbAccessibilityCaptionFontFamilyMonospaceSansSerif;
    case ClosedCaptionStyle::ProportionalWithoutSerifs:
      return kSbAccessibilityCaptionFontFamilyProportionalSansSerif;
    case ClosedCaptionStyle::Casual:
      return kSbAccessibilityCaptionFontFamilyCasual;
    case ClosedCaptionStyle::Cursive:
      return kSbAccessibilityCaptionFontFamilyCursive;
    case ClosedCaptionStyle::SmallCapitals:
      return kSbAccessibilityCaptionFontFamilySmallCapitals;
  }
}

SbAccessibilityCaptionFontSizePercentage
ClosedCaptionSizeToSbAccessibilityCaptionFontSizePercentage(
    ClosedCaptionSize opacity) {
  switch (opacity) {
    case ClosedCaptionSize::Default:
      return kSbAccessibilityCaptionFontSizePercentage100;
    case ClosedCaptionSize::FiftyPercent:
      return kSbAccessibilityCaptionFontSizePercentage50;
    case ClosedCaptionSize::OneHundredPercent:
      return kSbAccessibilityCaptionFontSizePercentage100;
    case ClosedCaptionSize::OneHundredFiftyPercent:
      return kSbAccessibilityCaptionFontSizePercentage150;
    case ClosedCaptionSize::TwoHundredPercent:
      return kSbAccessibilityCaptionFontSizePercentage200;
  }
}

SbAccessibilityCaptionOpacityPercentage
ClosedCaptionOpacityToSbAccessibilityCaptionOpacityPercentage(
    ClosedCaptionOpacity opacity) {
  switch (opacity) {
    case ClosedCaptionOpacity::Default:
      return kSbAccessibilityCaptionOpacityPercentage100;
    case ClosedCaptionOpacity::OneHundredPercent:
      return kSbAccessibilityCaptionOpacityPercentage100;
    case ClosedCaptionOpacity::SeventyFivePercent:
      return kSbAccessibilityCaptionOpacityPercentage75;
    case ClosedCaptionOpacity::TwentyFivePercent:
      return kSbAccessibilityCaptionOpacityPercentage25;
    case ClosedCaptionOpacity::ZeroPercent:
      return kSbAccessibilityCaptionOpacityPercentage0;
  }
}

}  // namespace.

bool GetCaptionSettings(SbAccessibilityCaptionSettings* caption_settings) {
  if (!caption_settings ||
      !starboard::common::MemoryIsZero(
          caption_settings, sizeof(SbAccessibilityCaptionSettings))) {
    return false;
  }

  caption_settings->background_color =
      ClosedCaptionColorToSbAccessibilityCaptionColor(
          ClosedCaptionProperties::BackgroundColor);
  caption_settings->background_color_state = kSbAccessibilityCaptionStateSet;

  caption_settings->background_opacity =
      ClosedCaptionOpacityToSbAccessibilityCaptionOpacityPercentage(
          ClosedCaptionProperties::BackgroundOpacity);
  caption_settings->background_opacity_state = kSbAccessibilityCaptionStateSet;

  caption_settings->character_edge_style =
      ClosedCaptionEdgeEffectToSbAccessibilityCaptionCharacterEdgeStyle(
          ClosedCaptionProperties::FontEffect);
  caption_settings->character_edge_style_state =
      kSbAccessibilityCaptionStateSet;

  caption_settings->font_color =
      ClosedCaptionColorToSbAccessibilityCaptionColor(
          ClosedCaptionProperties::FontColor);
  caption_settings->font_color_state = kSbAccessibilityCaptionStateSet;

  caption_settings->font_family =
      ClosedCaptionStyleToSbAccessibilityCaptionFontFamily(
          ClosedCaptionProperties::FontStyle);
  caption_settings->font_family_state = kSbAccessibilityCaptionStateSet;

  caption_settings->font_opacity =
      ClosedCaptionOpacityToSbAccessibilityCaptionOpacityPercentage(
          ClosedCaptionProperties::FontOpacity);
  caption_settings->font_opacity_state = kSbAccessibilityCaptionStateSet;

  caption_settings->font_size =
      ClosedCaptionSizeToSbAccessibilityCaptionFontSizePercentage(
          ClosedCaptionProperties::FontSize);
  caption_settings->font_size_state = kSbAccessibilityCaptionStateSet;

  caption_settings->window_color =
      ClosedCaptionColorToSbAccessibilityCaptionColor(
          ClosedCaptionProperties::RegionColor);
  caption_settings->window_color_state = kSbAccessibilityCaptionStateSet;

  caption_settings->window_opacity =
      ClosedCaptionOpacityToSbAccessibilityCaptionOpacityPercentage(
          ClosedCaptionProperties::RegionOpacity);
  caption_settings->window_opacity_state = kSbAccessibilityCaptionStateSet;

  caption_settings->is_enabled =
      ClosedCaptionProperties::BackgroundColor != ClosedCaptionColor::Default;
  caption_settings->supports_is_enabled = true;
  caption_settings->supports_set_enabled = false;
  return true;
}

}  // namespace accessibility
}  // namespace shared
}  // namespace xb1
}  // namespace starboard
