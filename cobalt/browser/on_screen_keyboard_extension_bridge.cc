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

#include "cobalt/browser/on_screen_keyboard_extension_bridge.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "starboard/event.h"

namespace cobalt {
namespace browser {
// static
bool OnScreenKeyboardExtensionBridge::IsSupported() {
  const CobaltExtensionOnScreenKeyboardApi* on_screen_keyboard_extension =
      static_cast<const CobaltExtensionOnScreenKeyboardApi*>(
          SbSystemGetExtension(kCobaltExtensionOnScreenKeyboardName));
  return (on_screen_keyboard_extension &&
          strcmp(on_screen_keyboard_extension->name,
                 kCobaltExtensionOnScreenKeyboardName) == 0 &&
          on_screen_keyboard_extension->version >= 1);
}

void OnScreenKeyboardExtensionBridge::Show(const char* input_text, int ticket) {
  // Delay providing the SbWindow until as late as possible.
  on_screen_keyboard_extension_->Show(sb_window_provider_.Run(), input_text,
                                      ticket);
}

void OnScreenKeyboardExtensionBridge::Hide(int ticket) {
  // Delay providing the SbWindow until as late as possible.
  on_screen_keyboard_extension_->Hide(sb_window_provider_.Run(), ticket);
}

void OnScreenKeyboardExtensionBridge::Focus(int ticket) {
  // Delay providing the SbWindow until as late as possible.
  on_screen_keyboard_extension_->Focus(sb_window_provider_.Run(), ticket);
}

void OnScreenKeyboardExtensionBridge::Blur(int ticket) {
  // Delay providing the SbWindow until as late as possible.
  on_screen_keyboard_extension_->Blur(sb_window_provider_.Run(), ticket);
}

void OnScreenKeyboardExtensionBridge::UpdateSuggestions(
    const script::Sequence<std::string>& suggestions, int ticket) {
  std::unique_ptr<const char*[]> suggestions_data(
      new const char*[suggestions.size()]);
  for (script::Sequence<std::string>::size_type i = 0; i < suggestions.size();
       ++i) {
    suggestions_data[i] = suggestions.at(i).c_str();
  }
  // Delay providing the SbWindow until as late as possible.
  on_screen_keyboard_extension_->UpdateSuggestions(
      sb_window_provider_.Run(), suggestions_data.get(),
      static_cast<int>(suggestions.size()), ticket);
}

bool OnScreenKeyboardExtensionBridge::IsShown() const {
  // Delay providing the SbWindow until as late as possible.
  return on_screen_keyboard_extension_->IsShown(sb_window_provider_.Run());
}

bool OnScreenKeyboardExtensionBridge::SuggestionsSupported() const {
  // Delay providing the SbWindow until as late as possible.
  return on_screen_keyboard_extension_->SuggestionsSupported(
      sb_window_provider_.Run());
}

scoped_refptr<dom::DOMRect> OnScreenKeyboardExtensionBridge::BoundingRect()
    const {
  // Delay providing the SbWindow until as late as possible.
  SbWindowRect sb_window_rect = SbWindowRect();
  if (!on_screen_keyboard_extension_->GetBoundingRect(sb_window_provider_.Run(),
                                                      &sb_window_rect)) {
    return nullptr;
  }

  scoped_refptr<dom::DOMRect> bounding_rect =
      new dom::DOMRect(sb_window_rect.x, sb_window_rect.y, sb_window_rect.width,
                       sb_window_rect.height);
  return bounding_rect;
}

bool OnScreenKeyboardExtensionBridge::IsValidTicket(int ticket) const {
  return ticket != kSbEventOnScreenKeyboardInvalidTicket;
}

void OnScreenKeyboardExtensionBridge::SetKeepFocus(bool keep_focus) {
  // Delay providing the SbWindow until as late as possible.
  on_screen_keyboard_extension_->SetKeepFocus(sb_window_provider_.Run(),
                                              keep_focus);
}

void OnScreenKeyboardExtensionBridge::SetBackgroundColor(uint8 r, uint8 g,
                                                         uint8 b) {
  on_screen_keyboard_extension_->SetBackgroundColor(sb_window_provider_.Run(),
                                                    r, g, b);
}

void OnScreenKeyboardExtensionBridge::SetLightTheme(bool light_theme) {
  on_screen_keyboard_extension_->SetLightTheme(sb_window_provider_.Run(),
                                               light_theme);
}
}  // namespace browser
}  // namespace cobalt
