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

#include "cobalt/dom/on_screen_keyboard.h"
#include "base/callback.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/dom/window.h"
#include "starboard/log.h"
#include "starboard/window.h"

namespace cobalt {
namespace dom {
OnScreenKeyboard::OnScreenKeyboard(
    const Window::GetSbWindowCallback& get_sb_window_callback)
    : get_sb_window_callback_(get_sb_window_callback) {
  CHECK(!get_sb_window_callback_.is_null());
}

void OnScreenKeyboard::Show() {
#if SB_HAS(ON_SCREEN_KEYBOARD)
  SbWindow sb_window = get_sb_window_callback_.Run();

  if (!sb_window) {
    LOG(ERROR) << "OnScreenKeyboard::Show invalid without SbWindow.";
    return;
  }
  SbWindowShowOnScreenKeyboard(sb_window);
  // Trigger onshow.
  DispatchEvent(new dom::Event(base::Tokens::show()));
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)
}

void OnScreenKeyboard::Hide() {
#if SB_HAS(ON_SCREEN_KEYBOARD)
  SbWindow sb_window = get_sb_window_callback_.Run();

  if (!sb_window) {
    LOG(ERROR) << "OnScreenKeyboard::Hide invalid without SbWindow.";
    return;
  }
  SbWindowHideOnScreenKeyboard(sb_window);
  // Trigger onhide.
  DispatchEvent(new dom::Event(base::Tokens::hide()));
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)
}

const EventTarget::EventListenerScriptValue* OnScreenKeyboard::onshow() const {
  return GetAttributeEventListener(base::Tokens::show());
}
void OnScreenKeyboard::set_onshow(
    const EventListenerScriptValue& event_listener) {
  SetAttributeEventListener(base::Tokens::show(), event_listener);
}

const EventTarget::EventListenerScriptValue* OnScreenKeyboard::onhide() const {
  return GetAttributeEventListener(base::Tokens::hide());
}
void OnScreenKeyboard::set_onhide(
    const EventListenerScriptValue& event_listener) {
  SetAttributeEventListener(base::Tokens::hide(), event_listener);
}

const EventTarget::EventListenerScriptValue* OnScreenKeyboard::oninput() const {
  return GetAttributeEventListener(base::Tokens::input());
}

void OnScreenKeyboard::set_oninput(
    const EventListenerScriptValue& event_listener) {
  SetAttributeEventListener(base::Tokens::input(), event_listener);
}

bool OnScreenKeyboard::shown() const {
#if SB_HAS(ON_SCREEN_KEYBOARD)
  SbWindow sb_window = get_sb_window_callback_.Run();
  return SbWindowIsOnScreenKeyboardShown(sb_window);
#else   // SB_HAS(ON_SCREEN_KEYBOARD)
  return false;
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)
}

}  // namespace dom
}  // namespace cobalt
