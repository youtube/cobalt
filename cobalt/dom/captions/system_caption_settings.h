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

#ifndef COBALT_DOM_CAPTIONS_SYSTEM_CAPTION_SETTINGS_H_
#define COBALT_DOM_CAPTIONS_SYSTEM_CAPTION_SETTINGS_H_

#include <string>

#include "cobalt/base/event.h"
#include "cobalt/base/event_dispatcher.h"
#include "cobalt/dom/captions/caption_character_edge_style.h"
#include "cobalt/dom/captions/caption_color.h"
#include "cobalt/dom/captions/caption_font_family.h"
#include "cobalt/dom/captions/caption_font_size_percentage.h"
#include "cobalt/dom/captions/caption_opacity_percentage.h"
#include "cobalt/dom/captions/caption_state.h"
#include "cobalt/dom/event_target.h"

namespace cobalt {
namespace dom {
namespace captions {

class SystemCaptionSettings : public EventTarget {
 public:
  SystemCaptionSettings() {}

  base::optional<std::string> background_color();
  CaptionState background_color_state();

  base::optional<std::string> background_opacity();
  CaptionState background_opacity_state();

  base::optional<std::string> character_edge_style();
  CaptionState character_edge_style_state();

  base::optional<std::string> font_color();
  CaptionState font_color_state();

  base::optional<std::string> font_family();
  CaptionState font_family_state();

  base::optional<std::string> font_opacity();
  CaptionState font_opacity_state();

  base::optional<std::string> font_size();
  CaptionState font_size_state();

  base::optional<std::string> window_color();
  CaptionState window_color_state();

  base::optional<std::string> window_opacity();
  CaptionState window_opacity_state();

  bool is_enabled();
  void set_is_enabled(bool active);

  bool supports_is_enabled();
  bool supports_set_enabled();

  bool supports_override();

  DEFINE_WRAPPABLE_TYPE(SystemCaptionSettings);

  const EventListenerScriptValue* onchanged() const;
  void set_onchanged(const EventListenerScriptValue& event_listener);

  void OnCaptionSettingsChanged();

 private:
  // TODO: Delete these functions and their implementations if nullable
  // enums work with our IDL generation code. Make the implementations of our
  // getters for style properties return the enum values themselves
  // instead of the strings that they map to.
  const char* CaptionCharacterEdgeStyleToString(
      CaptionCharacterEdgeStyle character_edge_style);
  const char* CaptionColorToString(CaptionColor color);
  const char* CaptionFontFamilyToString(CaptionFontFamily font_family);
  const char* CaptionFontSizePercentageToString(
      CaptionFontSizePercentage font_size);
  const char* CaptionOpacityPercentageToString(
      CaptionOpacityPercentage opacity);
};
}  // namespace captions
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_CAPTIONS_SYSTEM_CAPTION_SETTINGS_H_
