// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/on_screen_keyboard_starboard_bridge.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "starboard/event.h"

namespace cobalt {
namespace browser {
// static
bool OnScreenKeyboardStarboardBridge::IsSupported() {
  return SbWindowOnScreenKeyboardIsSupported();
}

void OnScreenKeyboardStarboardBridge::Show(const char* input_text, int ticket) {
  // Delay providing the SbWindow until as late as possible.
  SbWindowShowOnScreenKeyboard(sb_window_provider_.Run(), input_text, ticket);
}

void OnScreenKeyboardStarboardBridge::Hide(int ticket) {
  // Delay providing the SbWindow until as late as possible.
  SbWindowHideOnScreenKeyboard(sb_window_provider_.Run(), ticket);
}

void OnScreenKeyboardStarboardBridge::Focus(int ticket) {
  // Delay providing the SbWindow until as late as possible.
  SbWindowFocusOnScreenKeyboard(sb_window_provider_.Run(), ticket);
}

void OnScreenKeyboardStarboardBridge::Blur(int ticket) {
  // Delay providing the SbWindow until as late as possible.
  SbWindowBlurOnScreenKeyboard(sb_window_provider_.Run(), ticket);
}

void OnScreenKeyboardStarboardBridge::UpdateSuggestions(
    const script::Sequence<std::string>& suggestions, int ticket) {
  std::unique_ptr<const char*[]> suggestions_data(
      new const char*[suggestions.size()]);
  for (script::Sequence<std::string>::size_type i = 0; i < suggestions.size();
       ++i) {
    suggestions_data[i] = suggestions.at(i).c_str();
  }
  // Delay providing the SbWindow until as late as possible.
  SbWindowUpdateOnScreenKeyboardSuggestions(
      sb_window_provider_.Run(), suggestions_data.get(),
      static_cast<int>(suggestions.size()), ticket);
}

bool OnScreenKeyboardStarboardBridge::IsShown() const {
  // Delay providing the SbWindow until as late as possible.
  return SbWindowIsOnScreenKeyboardShown(sb_window_provider_.Run());
}

bool OnScreenKeyboardStarboardBridge::SuggestionsSupported() const {
  // Delay providing the SbWindow until as late as possible.
  return SbWindowOnScreenKeyboardSuggestionsSupported(
      sb_window_provider_.Run());
}

scoped_refptr<dom::DOMRect> OnScreenKeyboardStarboardBridge::BoundingRect()
    const {
  // Delay providing the SbWindow until as late as possible.
  SbWindowRect sb_window_rect = SbWindowRect();
  if (!SbWindowGetOnScreenKeyboardBoundingRect(sb_window_provider_.Run(),
                                               &sb_window_rect)) {
    return nullptr;
  }
  scoped_refptr<dom::DOMRect> bounding_rect =
      new dom::DOMRect(sb_window_rect.x, sb_window_rect.y, sb_window_rect.width,
                       sb_window_rect.height);
  return bounding_rect;
}

bool OnScreenKeyboardStarboardBridge::IsValidTicket(int ticket) const {
  return ticket != kSbEventOnScreenKeyboardInvalidTicket;
}

void OnScreenKeyboardStarboardBridge::SetKeepFocus(bool keep_focus) {
  // Delay providing the SbWindow until as late as possible.
  return SbWindowSetOnScreenKeyboardKeepFocus(sb_window_provider_.Run(),
                                              keep_focus);
}

void OnScreenKeyboardStarboardBridge::SetBackgroundColor(uint8 r, uint8 g,
                                                         uint8 b) {
  const CobaltExtensionOnScreenKeyboardApi* on_screen_keyboard_extension =
      static_cast<const CobaltExtensionOnScreenKeyboardApi*>(
          SbSystemGetExtension(kCobaltExtensionOnScreenKeyboardName));

  if (on_screen_keyboard_extension &&
      strcmp(on_screen_keyboard_extension->name,
             kCobaltExtensionOnScreenKeyboardName) == 0 &&
      on_screen_keyboard_extension->version >= 1) {
    on_screen_keyboard_extension->SetBackgroundColor(sb_window_provider_.Run(),
                                                     r, g, b);
  }
}

void OnScreenKeyboardStarboardBridge::SetLightTheme(bool light_theme) {
  const CobaltExtensionOnScreenKeyboardApi* on_screen_keyboard_extension =
      static_cast<const CobaltExtensionOnScreenKeyboardApi*>(
          SbSystemGetExtension(kCobaltExtensionOnScreenKeyboardName));

  if (on_screen_keyboard_extension &&
      strcmp(on_screen_keyboard_extension->name,
             kCobaltExtensionOnScreenKeyboardName) == 0 &&
      on_screen_keyboard_extension->version >= 1) {
    on_screen_keyboard_extension->SetLightTheme(sb_window_provider_.Run(),
                                                light_theme);
  }
}
}  // namespace browser
}  // namespace cobalt
