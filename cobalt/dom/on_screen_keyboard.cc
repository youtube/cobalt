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
#include "base/compiler_specific.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/dom/window.h"
#include "starboard/event.h"
#include "starboard/log.h"
#include "starboard/window.h"

namespace cobalt {
namespace dom {
OnScreenKeyboard::OnScreenKeyboard(
    const Window::GetSbWindowCallback& get_sb_window_callback,
    script::ScriptValueFactory* script_value_factory)
    : get_sb_window_callback_(get_sb_window_callback),
      script_value_factory_(script_value_factory),
      next_ticket_(0) {}

scoped_ptr<OnScreenKeyboard::VoidPromiseValue> OnScreenKeyboard::Show() {
  scoped_ptr<VoidPromiseValue> promise =
      script_value_factory_->CreateBasicPromise<void>();
  VoidPromiseValue::StrongReference promise_reference(*promise);
#if SB_HAS(ON_SCREEN_KEYBOARD)
  CHECK(!get_sb_window_callback_.is_null());
  SbWindow sb_window = get_sb_window_callback_.Run();

  if (!sb_window) {
    LOG(ERROR) << "OnScreenKeyboard::Show invalid without SbWindow.";
    return scoped_ptr<VoidPromiseValue>(NULL);
  }
  int ticket = next_ticket_++;
  bool is_emplaced =
      ticket_to_show_promise_map_
          .emplace(ticket, std::unique_ptr<VoidPromiseValue::StrongReference>(
                               new VoidPromiseValue::StrongReference(*promise)))
          .second;
  DCHECK(is_emplaced);
  SbWindowShowOnScreenKeyboard(sb_window, data_.c_str(), ticket);
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)
  return promise.Pass();
}

scoped_ptr<OnScreenKeyboard::VoidPromiseValue> OnScreenKeyboard::Hide() {
  scoped_ptr<VoidPromiseValue> promise =
      script_value_factory_->CreateBasicPromise<void>();
  VoidPromiseValue::StrongReference promise_reference(*promise);
#if SB_HAS(ON_SCREEN_KEYBOARD)
  CHECK(!get_sb_window_callback_.is_null());
  SbWindow sb_window = get_sb_window_callback_.Run();

  if (!sb_window) {
    LOG(ERROR) << "OnScreenKeyboard::Hide invalid without SbWindow.";
    return scoped_ptr<VoidPromiseValue>(NULL);
  }
  int ticket = next_ticket_++;
  bool is_emplaced =
      ticket_to_hide_promise_map_
          .emplace(ticket, std::unique_ptr<VoidPromiseValue::StrongReference>(
                               new VoidPromiseValue::StrongReference(*promise)))
          .second;
  DCHECK(is_emplaced);
  SbWindowHideOnScreenKeyboard(sb_window, ticket);
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)
  return promise.Pass();
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
  CHECK(!get_sb_window_callback_.is_null());
  SbWindow sb_window = get_sb_window_callback_.Run();
  return SbWindowIsOnScreenKeyboardShown(sb_window);
#else   // SB_HAS(ON_SCREEN_KEYBOARD)
  return false;
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)
}

void OnScreenKeyboard::DispatchHideEvent(int ticket) {
#if SB_HAS(ON_SCREEN_KEYBOARD)
  if (ticket != kSbEventOnScreenKeyboardInvalidTicket) {
    TicketToPromiseMap::const_iterator it =
        ticket_to_hide_promise_map_.find(ticket);
    DCHECK(it != ticket_to_hide_promise_map_.end())
        << "No promise matching ticket for OnScreenKeyboardHidden event.";
    it->second->value().Resolve();
    ticket_to_hide_promise_map_.erase(it);
  }
  DispatchEvent(new dom::Event(base::Tokens::hide()));
#else   // SB_HAS(ON_SCREEN_KEYBOARD)
  UNREFERENCED_PARAMETER(ticket);
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)
}

void OnScreenKeyboard::DispatchShowEvent(int ticket) {
#if SB_HAS(ON_SCREEN_KEYBOARD)
  if (ticket != kSbEventOnScreenKeyboardInvalidTicket) {
    TicketToPromiseMap::const_iterator it =
        ticket_to_show_promise_map_.find(ticket);
    DCHECK(it != ticket_to_show_promise_map_.end())
        << "No promise matching ticket for OnScreenKeyboardShown event.";
    it->second->value().Resolve();
    ticket_to_show_promise_map_.erase(it);
  }
  DispatchEvent(new dom::Event(base::Tokens::show()));
#else   // SB_HAS(ON_SCREEN_KEYBOARD)
  UNREFERENCED_PARAMETER(ticket);
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)
}

}  // namespace dom
}  // namespace cobalt
