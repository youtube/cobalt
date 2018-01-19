// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "base/bind.h"
#include "base/callback.h"

#include "starboard/event.h"

#if SB_HAS(ON_SCREEN_KEYBOARD)
namespace cobalt {
namespace browser {
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

bool OnScreenKeyboardStarboardBridge::IsShown() const {
  // Delay providing the SbWindow until as late as possible.
  return SbWindowIsOnScreenKeyboardShown(sb_window_provider_.Run());
}

bool OnScreenKeyboardStarboardBridge::IsValidTicket(int ticket) const {
  return ticket != kSbEventOnScreenKeyboardInvalidTicket;
}

void OnScreenKeyboardStarboardBridge::SetKeepFocus(bool keep_focus) {
  // Delay providing the SbWindow until as late as possible.
  return SbWindowSetOnScreenKeyboardKeepFocus(sb_window_provider_.Run(),
                                              keep_focus);
}
}  // namespace browser
}  // namespace cobalt
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)
