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
//

#include <string>

#include "base/compiler_specific.h"
#include "base/logging.h"

#include "cobalt/dom/captions/caption_character_edge_style.h"
#include "cobalt/dom/captions/caption_color.h"
#include "cobalt/dom/captions/caption_font_family.h"
#include "cobalt/dom/captions/caption_font_size_percentage.h"
#include "cobalt/dom/captions/caption_opacity_percentage.h"
#include "cobalt/dom/captions/caption_state.h"
#include "cobalt/dom/captions/system_caption_settings.h"
#include "cobalt/dom/event_target.h"

namespace cobalt {
namespace dom {
namespace captions {

SystemCaptionSettings::SystemCaptionSettings() {}

base::optional<std::string> SystemCaptionSettings::background_color() {
  return base::nullopt;
}

CaptionState SystemCaptionSettings::background_color_state() {
  return CaptionState::kCaptionStateUnset;
}

base::optional<std::string>
SystemCaptionSettings::background_opacity() {
  return base::nullopt;
}

CaptionState SystemCaptionSettings::background_opacity_state() {
  return CaptionState::kCaptionStateUnset;
}

base::optional<std::string>
SystemCaptionSettings::character_edge_style() {
  return base::nullopt;
}

CaptionState SystemCaptionSettings::character_edge_style_state() {
  return CaptionState::kCaptionStateUnset;
}

base::optional<std::string> SystemCaptionSettings::font_color() {
  return base::nullopt;
}

CaptionState SystemCaptionSettings::font_color_state() {
  return CaptionState::kCaptionStateUnset;
}

base::optional<std::string> SystemCaptionSettings::font_family() {
  return base::nullopt;
}

CaptionState SystemCaptionSettings::font_family_state() {
  return CaptionState::kCaptionStateUnset;
}

base::optional<std::string> SystemCaptionSettings::font_opacity() {
  return base::nullopt;
}

CaptionState SystemCaptionSettings::font_opacity_state() {
  return CaptionState::kCaptionStateUnset;
}

base::optional<std::string> SystemCaptionSettings::font_size() {
  return base::nullopt;
}

CaptionState SystemCaptionSettings::font_size_state() {
  return CaptionState::kCaptionStateUnset;
}

base::optional<std::string> SystemCaptionSettings::window_color() {
  return base::nullopt;
}

CaptionState SystemCaptionSettings::window_color_state() {
  return CaptionState::kCaptionStateUnset;
}

base::optional<std::string>
SystemCaptionSettings::window_opacity() {
  return base::nullopt;
}

CaptionState SystemCaptionSettings::window_opacity_state() {
  return CaptionState::kCaptionStateUnset;
}

bool SystemCaptionSettings::is_enabled() {
  return false;
}

void SystemCaptionSettings::set_is_enabled(bool active) {
  UNREFERENCED_PARAMETER(active);
}

bool SystemCaptionSettings::supports_is_enabled() {
  return false;
}

bool SystemCaptionSettings::supports_set_enabled() {
  return false;
}

bool SystemCaptionSettings::supports_override() {
  return true;
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
