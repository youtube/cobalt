// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_ON_SCREEN_KEYBOARD_H_
#define COBALT_DOM_ON_SCREEN_KEYBOARD_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/callback.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/dom_rect.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/dom/on_screen_keyboard_bridge.h"
#include "cobalt/dom/window.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/wrappable.h"
#include "starboard/window.h"

namespace cobalt {
namespace dom {

class Window;
class OnScreenKeyboardMockBridge;

class OnScreenKeyboard : public EventTarget {
 public:
  typedef script::ScriptValue<script::Promise<void>> VoidPromiseValue;

  typedef std::unordered_map<int, std::unique_ptr<VoidPromiseValue::Reference>>
      TicketToPromiseMap;

  OnScreenKeyboard(script::EnvironmentSettings* settings,
                   OnScreenKeyboardBridge* bridge,
                   script::ScriptValueFactory* script_value_factory);

  // Shows the on screen keyboard by calling a Starboard function.
  script::Handle<script::Promise<void>> Show();

  // Hides the on screen keyboard by calling a Starboard function.
  script::Handle<script::Promise<void>> Hide();

  // Focuses the on screen keyboard by calling a Starboard function.
  script::Handle<script::Promise<void>> Focus();

  // Blurs the on screen keyboard by calling a Starboard function.
  script::Handle<script::Promise<void>> Blur();

  // Updates the on screen keyboard suggestions by calling a Starboard function.
  script::Handle<script::Promise<void>> UpdateSuggestions(
      const script::Sequence<std::string>& suggestions);

  std::string data() const { return data_; }
  void set_data(const std::string& data) { data_ = data; }

  bool is_composing() const { return is_composing_; }
  void set_is_composing(const bool is_composing) {
    is_composing_ = is_composing;
  }

  const EventListenerScriptValue* onshow() const;
  void set_onshow(const EventListenerScriptValue& event_listener);

  const EventListenerScriptValue* onhide() const;
  void set_onhide(const EventListenerScriptValue& event_listener);

  const EventListenerScriptValue* onfocus() const;
  void set_onfocus(const EventListenerScriptValue& event_listener);

  const EventListenerScriptValue* onblur() const;
  void set_onblur(const EventListenerScriptValue& event_listener);

  const EventListenerScriptValue* oninput() const;
  void set_oninput(const EventListenerScriptValue& event_listener);

  // If the keyboard is shown.
  bool shown() const;

  // If the keyboard has suggestions implemented.
  bool suggestions_supported() const { return suggestions_supported_; }

  // The rectangle of the keyboard in screen pixel coordinates.
  scoped_refptr<DOMRect> bounding_rect() const;

  void set_keep_focus(bool keep_focus);
  bool keep_focus() const { return keep_focus_; }

  // Called by the WebModule to dispatch DOM show, hide, focus, blur and
  // suggestions updated events.
  void DispatchHideEvent(int ticket);
  void DispatchShowEvent(int ticket);
  void DispatchFocusEvent(int ticket);
  void DispatchBlurEvent(int ticket);
  void DispatchSuggestionsUpdatedEvent(int ticket);

  DEFINE_WRAPPABLE_TYPE(OnScreenKeyboard);

 private:
  friend class OnScreenKeyboardMockBridge;

  bool ResolvePromise(int ticket, TicketToPromiseMap* ticket_to_promise_map);

  ~OnScreenKeyboard() override {}

  TicketToPromiseMap ticket_to_hide_promise_map_;
  TicketToPromiseMap ticket_to_show_promise_map_;
  TicketToPromiseMap ticket_to_focus_promise_map_;
  TicketToPromiseMap ticket_to_blur_promise_map_;
  TicketToPromiseMap ticket_to_update_suggestions_promise_map_;

  OnScreenKeyboardBridge* bridge_;

  script::ScriptValueFactory* const script_value_factory_;

  std::string data_;
  bool is_composing_;

  int next_ticket_;

  bool keep_focus_ = false;

  bool suggestions_supported_;

  DISALLOW_COPY_AND_ASSIGN(OnScreenKeyboard);
};
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_ON_SCREEN_KEYBOARD_H_
