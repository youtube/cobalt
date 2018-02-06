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

#include <string>

#include "base/compiler_specific.h"
#include "base/logging.h"

#include "cobalt/base/accessibility_caption_settings_changed_event.h"
#include "cobalt/base/event_dispatcher.h"
#include "cobalt/dom/captions/caption_character_edge_style.h"
#include "cobalt/dom/captions/caption_color.h"
#include "cobalt/dom/captions/caption_font_family.h"
#include "cobalt/dom/captions/caption_font_size_percentage.h"
#include "cobalt/dom/captions/caption_opacity_percentage.h"
#include "cobalt/dom/captions/caption_state.h"
#include "cobalt/dom/captions/system_caption_settings.h"
#include "cobalt/dom/event_target.h"

#include "starboard/accessibility.h"
#include "starboard/memory.h"

namespace cobalt {
namespace dom {
namespace captions {

#if SB_HAS(CAPTIONS)
namespace {

CaptionColor ToCobaltCaptionColor(SbAccessibilityCaptionColor color) {
  switch (color) {
    case kSbAccessibilityCaptionColorBlue:
      return CaptionColor::kCaptionColorBlue;
    case kSbAccessibilityCaptionColorBlack:
      return CaptionColor::kCaptionColorBlack;
    case kSbAccessibilityCaptionColorCyan:
      return CaptionColor::kCaptionColorCyan;
    case kSbAccessibilityCaptionColorGreen:
      return CaptionColor::kCaptionColorGreen;
    case kSbAccessibilityCaptionColorMagenta:
      return CaptionColor::kCaptionColorMagenta;
    case kSbAccessibilityCaptionColorRed:
      return CaptionColor::kCaptionColorRed;
    case kSbAccessibilityCaptionColorWhite:
      return CaptionColor::kCaptionColorWhite;
    case kSbAccessibilityCaptionColorYellow:
      return CaptionColor::kCaptionColorYellow;
    default:
      NOTREACHED() << "Invalid color conversion";
      return CaptionColor::kCaptionColorWhite;
  }
}

CaptionCharacterEdgeStyle ToCobaltCaptionCharacterEdgeStyle(
  SbAccessibilityCaptionCharacterEdgeStyle style) {
  switch (style) {
    case kSbAccessibilityCaptionCharacterEdgeStyleNone:
      return CaptionCharacterEdgeStyle::
              kCaptionCharacterEdgeStyleNone;
    case kSbAccessibilityCaptionCharacterEdgeStyleRaised:
      return CaptionCharacterEdgeStyle::
              kCaptionCharacterEdgeStyleRaised;
    case kSbAccessibilityCaptionCharacterEdgeStyleDepressed:
      return CaptionCharacterEdgeStyle::
              kCaptionCharacterEdgeStyleDepressed;
    case kSbAccessibilityCaptionCharacterEdgeStyleUniform:
      return CaptionCharacterEdgeStyle::
              kCaptionCharacterEdgeStyleUniform;
    case kSbAccessibilityCaptionCharacterEdgeStyleDropShadow:
      return CaptionCharacterEdgeStyle::
              kCaptionCharacterEdgeStyleDropShadow;
    default:
      NOTREACHED() << "Invalid character edge style conversion";
      return CaptionCharacterEdgeStyle::kCaptionCharacterEdgeStyleNone;
  }
}

CaptionFontFamily ToCobaltCaptionFontFamily(
    SbAccessibilityCaptionFontFamily font_family) {
  switch (font_family) {
    case kSbAccessibilityCaptionFontFamilyCasual:
      return CaptionFontFamily::kCaptionFontFamilyCasual;
    case kSbAccessibilityCaptionFontFamilyCursive:
      return CaptionFontFamily::kCaptionFontFamilyCursive;
    case kSbAccessibilityCaptionFontFamilyMonospaceSansSerif:
      return CaptionFontFamily::kCaptionFontFamilyMonospaceSansSerif;
    case kSbAccessibilityCaptionFontFamilyMonospaceSerif:
      return CaptionFontFamily::kCaptionFontFamilyMonospaceSerif;
    case kSbAccessibilityCaptionFontFamilyProportionalSansSerif:
      return CaptionFontFamily::kCaptionFontFamilyProportionalSerif;
    case kSbAccessibilityCaptionFontFamilyProportionalSerif:
      return CaptionFontFamily::kCaptionFontFamilyProportionalSansSerif;
    case kSbAccessibilityCaptionFontFamilySmallCapitals:
      return CaptionFontFamily::kCaptionFontFamilySmallCapitals;
    default:
      NOTREACHED() << "Invalid font family conversion";
      return CaptionFontFamily::kCaptionFontFamilyCasual;
  }
}

CaptionFontSizePercentage ToCobaltCaptionFontSizePercentage(
    int font_size) {
  switch (font_size) {
    case kSbAccessibilityCaptionFontSizePercentage25:
      return CaptionFontSizePercentage::kCaptionFontSizePercentage25;
    case kSbAccessibilityCaptionFontSizePercentage50:
      return CaptionFontSizePercentage::kCaptionFontSizePercentage50;
    case kSbAccessibilityCaptionFontSizePercentage75:
      return CaptionFontSizePercentage::kCaptionFontSizePercentage75;
    case kSbAccessibilityCaptionFontSizePercentage100:
      return CaptionFontSizePercentage::kCaptionFontSizePercentage100;
    case kSbAccessibilityCaptionFontSizePercentage125:
      return CaptionFontSizePercentage::kCaptionFontSizePercentage125;
    case kSbAccessibilityCaptionFontSizePercentage150:
      return CaptionFontSizePercentage::kCaptionFontSizePercentage150;
    case kSbAccessibilityCaptionFontSizePercentage175:
      return CaptionFontSizePercentage::kCaptionFontSizePercentage175;
    case kSbAccessibilityCaptionFontSizePercentage200:
      return CaptionFontSizePercentage::kCaptionFontSizePercentage200;
    case kSbAccessibilityCaptionFontSizePercentage225:
      return CaptionFontSizePercentage::kCaptionFontSizePercentage225;
    case kSbAccessibilityCaptionFontSizePercentage250:
      return CaptionFontSizePercentage::kCaptionFontSizePercentage250;
    case kSbAccessibilityCaptionFontSizePercentage275:
      return CaptionFontSizePercentage::kCaptionFontSizePercentage275;
    case kSbAccessibilityCaptionFontSizePercentage300:
      return CaptionFontSizePercentage::kCaptionFontSizePercentage300;
    default:
      NOTREACHED() << "Invalid font size percentage conversion";
      return CaptionFontSizePercentage::kCaptionFontSizePercentage100;
  }
}

CaptionOpacityPercentage ToCobaltCaptionOpacityPercentage(
  SbAccessibilityCaptionOpacityPercentage opacity) {
  switch (opacity) {
    case kSbAccessibilityCaptionOpacityPercentage0:
      return CaptionOpacityPercentage::kCaptionOpacityPercentage0;
    case kSbAccessibilityCaptionOpacityPercentage25:
      return CaptionOpacityPercentage::kCaptionOpacityPercentage25;
    case kSbAccessibilityCaptionOpacityPercentage50:
      return CaptionOpacityPercentage::kCaptionOpacityPercentage50;
    case kSbAccessibilityCaptionOpacityPercentage75:
      return CaptionOpacityPercentage::kCaptionOpacityPercentage75;
    case kSbAccessibilityCaptionOpacityPercentage100:
      return CaptionOpacityPercentage::kCaptionOpacityPercentage100;
    default:
      NOTREACHED() << "Invalid opacity conversion";
      return CaptionOpacityPercentage::kCaptionOpacityPercentage100;
  }
}

CaptionState ToCobaltCaptionState(SbAccessibilityCaptionState state) {
  switch (state) {
    case kSbAccessibilityCaptionStateUnsupported:
      return CaptionState::kCaptionStateUnsupported;
    case kSbAccessibilityCaptionStateUnset:
      return CaptionState::kCaptionStateUnset;
    case kSbAccessibilityCaptionStateSet:
      return CaptionState::kCaptionStateSet;
    case kSbAccessibilityCaptionStateOverride:
      return CaptionState::kCaptionStateOverride;
    default:
      NOTREACHED() << "Invalid caption state conversion";
      return CaptionState::kCaptionStateUnsupported;
  }
}

}  // namespace

#endif  // SB_HAS(CAPTIONS)

void SystemCaptionSettings::OnCaptionSettingsChanged() {
  DispatchEventAndRunCallback(base::Tokens::change(),
                              base::Closure(base::Bind(base::DoNothing)));
}

base::optional<std::string> SystemCaptionSettings::background_color() {
#if SB_HAS(CAPTIONS)
  SbAccessibilityCaptionSettings caption_settings;
  SbMemorySet(&caption_settings, 0, sizeof(caption_settings));
  bool success = SbAccessibilityGetCaptionSettings(&caption_settings);

  if (!success) {
    return base::nullopt;
  }

  const char* color = SystemCaptionSettings::CaptionColorToString(
      ToCobaltCaptionColor(caption_settings.background_color));
  if (color == nullptr) {
    return base::nullopt;
  } else {
    return std::string(color);
  }
#else
  return base::nullopt;
#endif  // SB_HAS(CAPTIONS)
}

CaptionState SystemCaptionSettings::background_color_state() {
#if SB_HAS(CAPTIONS)
  SbAccessibilityCaptionSettings caption_settings;
  SbMemorySet(&caption_settings, 0, sizeof(caption_settings));
  bool success = SbAccessibilityGetCaptionSettings(&caption_settings);
  if (success) {
    return ToCobaltCaptionState(caption_settings.background_color_state);
  } else {
    return CaptionState::kCaptionStateUnsupported;
  }
#else
  return CaptionState::kCaptionStateUnsupported;
#endif  // SB_HAS(CAPTIONS)
}

base::optional<std::string>
SystemCaptionSettings::background_opacity() {
#if SB_HAS(CAPTIONS)
  SbAccessibilityCaptionSettings caption_settings;
  SbMemorySet(&caption_settings, 0, sizeof(caption_settings));
  bool success = SbAccessibilityGetCaptionSettings(&caption_settings);

  if (!success) {
    return base::nullopt;
  }

  const char* opacity =
    SystemCaptionSettings::CaptionOpacityPercentageToString(
        ToCobaltCaptionOpacityPercentage(caption_settings.background_opacity));
  if (opacity == nullptr) {
    return base::nullopt;
  } else {
    return std::string(opacity);
  }
#else
  return base::nullopt;
#endif  // SB_HAS(CAPTIONS)
}

CaptionState SystemCaptionSettings::background_opacity_state() {
#if SB_HAS(CAPTIONS)
  SbAccessibilityCaptionSettings caption_settings;
  SbMemorySet(&caption_settings, 0, sizeof(caption_settings));
  bool success = SbAccessibilityGetCaptionSettings(&caption_settings);
  if (success) {
    return ToCobaltCaptionState(caption_settings.background_opacity_state);
  } else {
    return CaptionState::kCaptionStateUnsupported;
  }
#else
  return CaptionState::kCaptionStateUnsupported;
#endif  // SB_HAS(CAPTIONS)
}

base::optional<std::string>
SystemCaptionSettings::character_edge_style() {
#if SB_HAS(CAPTIONS)
  SbAccessibilityCaptionSettings caption_settings;
  SbMemorySet(&caption_settings, 0, sizeof(caption_settings));
  bool success = SbAccessibilityGetCaptionSettings(&caption_settings);

  if (!success) {
    return base::nullopt;
  }

  const char* character_edge_style =
    SystemCaptionSettings::CaptionCharacterEdgeStyleToString(
        ToCobaltCaptionCharacterEdgeStyle(
            caption_settings.character_edge_style));
  if (character_edge_style == nullptr) {
    return base::nullopt;
  } else {
    return std::string(character_edge_style);
  }
#else
  return base::nullopt;
#endif  // SB_HAS(CAPTIONS)
}

CaptionState SystemCaptionSettings::character_edge_style_state() {
#if SB_HAS(CAPTIONS)
  SbAccessibilityCaptionSettings caption_settings;
  SbMemorySet(&caption_settings, 0, sizeof(caption_settings));
  bool success = SbAccessibilityGetCaptionSettings(&caption_settings);
  if (success) {
    return ToCobaltCaptionState(
        caption_settings.character_edge_style_state);
  } else {
    return CaptionState::kCaptionStateUnsupported;
  }
#else
  return CaptionState::kCaptionStateUnsupported;
#endif  // SB_HAS(CAPTIONS)
}

base::optional<std::string> SystemCaptionSettings::font_color() {
#if SB_HAS(CAPTIONS)
  SbAccessibilityCaptionSettings caption_settings;
  SbMemorySet(&caption_settings, 0, sizeof(caption_settings));
  bool success = SbAccessibilityGetCaptionSettings(&caption_settings);

  if (!success) {
    return base::nullopt;
  }

  const char* color = SystemCaptionSettings::CaptionColorToString(
      ToCobaltCaptionColor(caption_settings.font_color));
  if (color == nullptr) {
    return base::nullopt;
  } else {
    return std::string(color);
  }
#else
  return base::nullopt;
#endif  // SB_HAS(CAPTIONS)
}

CaptionState SystemCaptionSettings::font_color_state() {
#if SB_HAS(CAPTIONS)
  SbAccessibilityCaptionSettings caption_settings;
  SbMemorySet(&caption_settings, 0, sizeof(caption_settings));
  bool success = SbAccessibilityGetCaptionSettings(&caption_settings);
  if (success) {
    return ToCobaltCaptionState(caption_settings.font_color_state);
  } else {
    return CaptionState::kCaptionStateUnsupported;
  }
#else
  return CaptionState::kCaptionStateUnsupported;
#endif  // SB_HAS(CAPTIONS)
}

base::optional<std::string> SystemCaptionSettings::font_family() {
#if SB_HAS(CAPTIONS)
  SbAccessibilityCaptionSettings caption_settings;
  SbMemorySet(&caption_settings, 0, sizeof(caption_settings));
  bool success = SbAccessibilityGetCaptionSettings(&caption_settings);

  if (!success) {
    return base::nullopt;
  }

  const char* font_family =
    SystemCaptionSettings::CaptionFontFamilyToString(
        ToCobaltCaptionFontFamily(caption_settings.font_family));
  if (font_family == nullptr) {
    return base::nullopt;
  } else {
    return std::string(font_family);
  }
#else
  return base::nullopt;
#endif  // SB_HAS(CAPTIONS)
}

CaptionState SystemCaptionSettings::font_family_state() {
#if SB_HAS(CAPTIONS)
  SbAccessibilityCaptionSettings caption_settings;
  SbMemorySet(&caption_settings, 0, sizeof(caption_settings));
  bool success = SbAccessibilityGetCaptionSettings(&caption_settings);
  if (success) {
    return ToCobaltCaptionState(caption_settings.font_family_state);
  } else {
    return CaptionState::kCaptionStateUnsupported;
  }
#else
  return CaptionState::kCaptionStateUnsupported;
#endif  // SB_HAS(CAPTIONS)
}

base::optional<std::string> SystemCaptionSettings::font_opacity() {
#if SB_HAS(CAPTIONS)
  SbAccessibilityCaptionSettings caption_settings;
  SbMemorySet(&caption_settings, 0, sizeof(caption_settings));
  bool success = SbAccessibilityGetCaptionSettings(&caption_settings);

  if (!success) {
    return base::nullopt;
  }

  const char* opacity =
    SystemCaptionSettings::CaptionOpacityPercentageToString(
        ToCobaltCaptionOpacityPercentage(caption_settings.font_opacity));
  if (opacity == nullptr) {
    return base::nullopt;
  } else {
    return std::string(opacity);
  }
#else
  return base::nullopt;
#endif  // SB_HAS(CAPTIONS)
}

CaptionState SystemCaptionSettings::font_opacity_state() {
#if SB_HAS(CAPTIONS)
  SbAccessibilityCaptionSettings caption_settings;
  SbMemorySet(&caption_settings, 0, sizeof(caption_settings));
  bool success = SbAccessibilityGetCaptionSettings(&caption_settings);
  if (success) {
    return ToCobaltCaptionState(caption_settings.font_opacity_state);
  } else {
    return CaptionState::kCaptionStateUnsupported;
  }
#else
  return CaptionState::kCaptionStateUnsupported;
#endif  // SB_HAS(CAPTIONS)
}

base::optional<std::string> SystemCaptionSettings::font_size() {
#if SB_HAS(CAPTIONS)
  SbAccessibilityCaptionSettings caption_settings;
  SbMemorySet(&caption_settings, 0, sizeof(caption_settings));
  bool success = SbAccessibilityGetCaptionSettings(&caption_settings);

  if (!success) {
    return base::nullopt;
  }

  const char* font_size =
    SystemCaptionSettings::CaptionFontSizePercentageToString(
        ToCobaltCaptionFontSizePercentage(caption_settings.font_size));
  if (font_size == nullptr) {
    return base::nullopt;
  } else {
    return std::string(font_size);
  }
#else
  return base::nullopt;
#endif  // SB_HAS(CAPTIONS)
}

CaptionState SystemCaptionSettings::font_size_state() {
#if SB_HAS(CAPTIONS)
  SbAccessibilityCaptionSettings caption_settings;
  SbMemorySet(&caption_settings, 0, sizeof(caption_settings));
  bool success = SbAccessibilityGetCaptionSettings(&caption_settings);
  if (success) {
    return ToCobaltCaptionState(caption_settings.font_size_state);
  } else {
    return CaptionState::kCaptionStateUnsupported;
  }
#else
  return CaptionState::kCaptionStateUnsupported;
#endif  // SB_HAS(CAPTIONS)
}

base::optional<std::string> SystemCaptionSettings::window_color() {
#if SB_HAS(CAPTIONS)
  SbAccessibilityCaptionSettings caption_settings;
  SbMemorySet(&caption_settings, 0, sizeof(caption_settings));
  bool success = SbAccessibilityGetCaptionSettings(&caption_settings);

  if (!success) {
    return base::nullopt;
  }

  const char* color = SystemCaptionSettings::CaptionColorToString(
      ToCobaltCaptionColor(caption_settings.window_color));
  if (color == nullptr) {
    return base::nullopt;
  } else {
    return std::string(color);
  }
#else
  return base::nullopt;
#endif  // SB_HAS(CAPTIONS)
}

CaptionState SystemCaptionSettings::window_color_state() {
#if SB_HAS(CAPTIONS)
  SbAccessibilityCaptionSettings caption_settings;
  SbMemorySet(&caption_settings, 0, sizeof(caption_settings));
  bool success = SbAccessibilityGetCaptionSettings(&caption_settings);
  if (success) {
    return ToCobaltCaptionState(caption_settings.window_color_state);
  } else {
    return CaptionState::kCaptionStateUnsupported;
  }
#else
  return CaptionState::kCaptionStateUnsupported;
#endif  // SB_HAS(CAPTIONS)
}

base::optional<std::string>
SystemCaptionSettings::window_opacity() {
#if SB_HAS(CAPTIONS)
  SbAccessibilityCaptionSettings caption_settings;
  SbMemorySet(&caption_settings, 0, sizeof(caption_settings));
  bool success = SbAccessibilityGetCaptionSettings(&caption_settings);

  if (!success) {
    return base::nullopt;
  }

  const char* opacity =
    SystemCaptionSettings::CaptionOpacityPercentageToString(
        ToCobaltCaptionOpacityPercentage(caption_settings.window_opacity));
  if (opacity == nullptr) {
    return base::nullopt;
  } else {
    return std::string(opacity);
  }
#else
  return base::nullopt;
#endif  // SB_HAS(CAPTIONS)
}

CaptionState SystemCaptionSettings::window_opacity_state() {
#if SB_HAS(CAPTIONS)
  SbAccessibilityCaptionSettings caption_settings;
  SbMemorySet(&caption_settings, 0, sizeof(caption_settings));
  bool success = SbAccessibilityGetCaptionSettings(&caption_settings);
  if (success) {
    return ToCobaltCaptionState(caption_settings.window_opacity_state);
  } else {
    return CaptionState::kCaptionStateUnsupported;
  }
#else
  return CaptionState::kCaptionStateUnsupported;
#endif  // SB_HAS(CAPTIONS)
}

bool SystemCaptionSettings::is_enabled() {
#if SB_HAS(CAPTIONS)
  SbAccessibilityCaptionSettings caption_settings;
  SbMemorySet(&caption_settings, 0, sizeof(caption_settings));
  bool success = SbAccessibilityGetCaptionSettings(&caption_settings);

  return (success) ? caption_settings.is_enabled : false;

#else
  return false;
#endif  // SB_HAS(CAPTIONS)
}

void SystemCaptionSettings::set_is_enabled(bool active) {
  UNREFERENCED_PARAMETER(active);
}

bool SystemCaptionSettings::supports_is_enabled() {
#if SB_HAS(CAPTIONS)
  SbAccessibilityCaptionSettings caption_settings;
  SbMemorySet(&caption_settings, 0, sizeof(caption_settings));
  bool success = SbAccessibilityGetCaptionSettings(&caption_settings);

  return (success) ? caption_settings.supports_is_enabled : false;

#else
  return false;
#endif  // SB_HAS(CAPTIONS)
}

bool SystemCaptionSettings::supports_set_enabled() {
#if SB_HAS(CAPTIONS)
  SbAccessibilityCaptionSettings caption_settings;
  SbMemorySet(&caption_settings, 0, sizeof(caption_settings));
  bool success = SbAccessibilityGetCaptionSettings(&caption_settings);

  return (success) ? caption_settings.supports_set_enabled : false;

#else
  return false;
#endif  // SB_HAS(CAPTIONS)
}

bool SystemCaptionSettings::supports_override() {
#if SB_HAS(CAPTIONS)
  SbAccessibilityCaptionSettings caption_settings;
  SbMemorySet(&caption_settings, 0, sizeof(caption_settings));
  bool success = SbAccessibilityGetCaptionSettings(&caption_settings);

  return (success) ? caption_settings.supports_override : false;

#else
  return false;
#endif  // SB_HAS(CAPTIONS)
}

const EventTarget::EventListenerScriptValue*
SystemCaptionSettings::onchanged() const {
  return EventTarget::GetAttributeEventListener(base::Tokens::change());
}

void SystemCaptionSettings::set_onchanged(
    const EventTarget::EventListenerScriptValue& event_listener) {
  EventTarget::SetAttributeEventListener(base::Tokens::change(),
                                         event_listener);
}

const char* SystemCaptionSettings::CaptionCharacterEdgeStyleToString(
    CaptionCharacterEdgeStyle character_edge_style) {
  switch (character_edge_style) {
    case CaptionCharacterEdgeStyle::kCaptionCharacterEdgeStyleNone:
      return "none";
    case CaptionCharacterEdgeStyle::kCaptionCharacterEdgeStyleRaised:
      return "raised";
    case CaptionCharacterEdgeStyle::kCaptionCharacterEdgeStyleDepressed:
      return "depressed";
    case CaptionCharacterEdgeStyle::kCaptionCharacterEdgeStyleUniform:
      return "uniform";
    case CaptionCharacterEdgeStyle::kCaptionCharacterEdgeStyleDropShadow:
      return "drop-shadow";
    default:
      NOTREACHED() << "Invalid character edge style";
      return nullptr;
  }
}

const char* SystemCaptionSettings::CaptionColorToString(CaptionColor color) {
  switch (color) {
    case CaptionColor::kCaptionColorBlue:
      return "blue";
    case CaptionColor::kCaptionColorBlack:
      return "black";
    case CaptionColor::kCaptionColorCyan:
      return "cyan";
    case CaptionColor::kCaptionColorGreen:
      return "green";
    case CaptionColor::kCaptionColorMagenta:
      return "magenta";
    case CaptionColor::kCaptionColorRed:
      return "red";
    case CaptionColor::kCaptionColorWhite:
      return "white";
    case CaptionColor::kCaptionColorYellow:
      return "yellow";
    default:
      NOTREACHED() << "Invalid color";
      return nullptr;
  }
}

const char* SystemCaptionSettings::CaptionFontFamilyToString(
    CaptionFontFamily font_family) {
  switch (font_family) {
    case CaptionFontFamily::kCaptionFontFamilyCasual:
      return "casual";
    case CaptionFontFamily::kCaptionFontFamilyCursive:
      return "cursive";
    case CaptionFontFamily::kCaptionFontFamilyMonospaceSansSerif:
      return "monospace-sans-serif";
    case CaptionFontFamily::kCaptionFontFamilyMonospaceSerif:
      return "monospace-serif";
    case CaptionFontFamily::kCaptionFontFamilyProportionalSansSerif:
      return "proportional-sans-serif";
    case CaptionFontFamily::kCaptionFontFamilyProportionalSerif:
      return "proportional-serif";
    case CaptionFontFamily::kCaptionFontFamilySmallCapitals:
      return "small-capitals";
    default:
      NOTREACHED() << "Invalid font family";
      return nullptr;
  }
}

const char* SystemCaptionSettings::CaptionFontSizePercentageToString(
    CaptionFontSizePercentage font_size) {
  switch (font_size) {
    case CaptionFontSizePercentage::kCaptionFontSizePercentage25:
      return "25";
    case CaptionFontSizePercentage::kCaptionFontSizePercentage50:
      return "50";
    case CaptionFontSizePercentage::kCaptionFontSizePercentage75:
      return "75";
    case CaptionFontSizePercentage::kCaptionFontSizePercentage100:
      return "100";
    case CaptionFontSizePercentage::kCaptionFontSizePercentage125:
      return "125";
    case CaptionFontSizePercentage::kCaptionFontSizePercentage150:
      return "150";
    case CaptionFontSizePercentage::kCaptionFontSizePercentage175:
      return "175";
    case CaptionFontSizePercentage::kCaptionFontSizePercentage200:
      return "200";
    case CaptionFontSizePercentage::kCaptionFontSizePercentage225:
      return "225";
    case CaptionFontSizePercentage::kCaptionFontSizePercentage250:
      return "250";
    case CaptionFontSizePercentage::kCaptionFontSizePercentage275:
      return "275";
    case CaptionFontSizePercentage::kCaptionFontSizePercentage300:
      return "300";
    default:
      NOTREACHED() << "Invalid font size";
      return nullptr;
  }
}

const char* SystemCaptionSettings::CaptionOpacityPercentageToString(
    CaptionOpacityPercentage opacity) {
  switch (opacity) {
    case CaptionOpacityPercentage::kCaptionOpacityPercentage0:
      return "0";
    case CaptionOpacityPercentage::kCaptionOpacityPercentage25:
      return "25";
    case CaptionOpacityPercentage::kCaptionOpacityPercentage50:
      return "50";
    case CaptionOpacityPercentage::kCaptionOpacityPercentage75:
      return "75";
    case CaptionOpacityPercentage::kCaptionOpacityPercentage100:
      return "100";
    default:
      NOTREACHED() << "Invalid opacity";
      return nullptr;
  }
}

}  // namespace captions
}  // namespace dom
}  // namespace cobalt
