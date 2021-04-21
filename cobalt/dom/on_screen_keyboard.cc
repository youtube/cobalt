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

#include "cobalt/dom/on_screen_keyboard.h"

#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/dom/window.h"

namespace cobalt {
namespace dom {

OnScreenKeyboard::OnScreenKeyboard(
    script::EnvironmentSettings* settings, OnScreenKeyboardBridge* bridge,
    script::ScriptValueFactory* script_value_factory)
    : EventTarget(settings),
      bridge_(bridge),
      script_value_factory_(script_value_factory),
      next_ticket_(0) {
  DCHECK(bridge_) << "OnScreenKeyboardBridge must not be NULL";
  suggestions_supported_ = bridge_->SuggestionsSupported();
}

script::Handle<script::Promise<void>> OnScreenKeyboard::Show() {
  script::Handle<script::Promise<void>> promise =
      script_value_factory_->CreateBasicPromise<void>();
  int ticket = next_ticket_++;
  bool is_emplaced =
      ticket_to_show_promise_map_
          .emplace(ticket, std::unique_ptr<VoidPromiseValue::Reference>(
                               new VoidPromiseValue::Reference(this, promise)))
          .second;
  DCHECK(is_emplaced);
  bridge_->Show(data_.c_str(), ticket);
  return promise;
}

script::Handle<script::Promise<void>> OnScreenKeyboard::Hide() {
  script::Handle<script::Promise<void>> promise =
      script_value_factory_->CreateBasicPromise<void>();
  int ticket = next_ticket_++;
  bool is_emplaced =
      ticket_to_hide_promise_map_
          .emplace(ticket, std::unique_ptr<VoidPromiseValue::Reference>(
                               new VoidPromiseValue::Reference(this, promise)))
          .second;
  DCHECK(is_emplaced);
  bridge_->Hide(ticket);
  return promise;
}

script::Handle<script::Promise<void>> OnScreenKeyboard::Focus() {
  script::Handle<script::Promise<void>> promise =
      script_value_factory_->CreateBasicPromise<void>();
  int ticket = next_ticket_++;
  bool is_emplaced =
      ticket_to_focus_promise_map_
          .emplace(ticket, std::unique_ptr<VoidPromiseValue::Reference>(
                               new VoidPromiseValue::Reference(this, promise)))
          .second;
  DCHECK(is_emplaced);
  bridge_->Focus(ticket);
  return promise;
}

script::Handle<script::Promise<void>> OnScreenKeyboard::Blur() {
  script::Handle<script::Promise<void>> promise =
      script_value_factory_->CreateBasicPromise<void>();
  int ticket = next_ticket_++;
  bool is_emplaced =
      ticket_to_blur_promise_map_
          .emplace(ticket, std::unique_ptr<VoidPromiseValue::Reference>(
                               new VoidPromiseValue::Reference(this, promise)))
          .second;
  DCHECK(is_emplaced);
  bridge_->Blur(ticket);
  return promise;
}

script::Handle<script::Promise<void>> OnScreenKeyboard::UpdateSuggestions(
    const script::Sequence<std::string>& suggestions) {
  script::Handle<script::Promise<void>> promise =
      script_value_factory_->CreateBasicPromise<void>();
  if (suggestions_supported_) {
    int ticket = next_ticket_++;
    bool is_emplaced =
        ticket_to_update_suggestions_promise_map_
            .emplace(ticket,
                     std::unique_ptr<VoidPromiseValue::Reference>(
                         new VoidPromiseValue::Reference(this, promise)))
            .second;
    DCHECK(is_emplaced);
    bridge_->UpdateSuggestions(suggestions, ticket);
  } else {
    LOG(WARNING)
        << "Starboard version " << SB_API_VERSION
        << " does not support on-screen keyboard suggestions on this platform.";
    promise->Reject();
  }
  return promise;
}

const EventTarget::EventListenerScriptValue* OnScreenKeyboard::onshow() const {
  return GetAttributeEventListener(base::Tokens::show());
}
void OnScreenKeyboard::set_onshow(
    const EventListenerScriptValue& event_listener) {
  SetAttributeEventListener(base::Tokens::show(), event_listener);
}

const EventTarget::EventListenerScriptValue* OnScreenKeyboard::onfocus() const {
  return GetAttributeEventListener(base::Tokens::focus());
}
void OnScreenKeyboard::set_onfocus(
    const EventListenerScriptValue& event_listener) {
  SetAttributeEventListener(base::Tokens::focus(), event_listener);
}

const EventTarget::EventListenerScriptValue* OnScreenKeyboard::onblur() const {
  return GetAttributeEventListener(base::Tokens::blur());
}
void OnScreenKeyboard::set_onblur(
    const EventListenerScriptValue& event_listener) {
  SetAttributeEventListener(base::Tokens::blur(), event_listener);
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

bool OnScreenKeyboard::shown() const { return bridge_->IsShown(); }

scoped_refptr<DOMRect> OnScreenKeyboard::bounding_rect() const {
  return bridge_->BoundingRect();
}

void OnScreenKeyboard::set_keep_focus(bool keep_focus) {
  keep_focus_ = keep_focus;
  bridge_->SetKeepFocus(keep_focus);
}

void OnScreenKeyboard::DispatchHideEvent(int ticket) {
  if (ResolvePromise(ticket, &ticket_to_hide_promise_map_)) {
    DispatchEvent(new dom::Event(base::Tokens::hide()));
  } else {
    LOG(ERROR)
        << "No promise matching ticket for OnScreenKeyboardHidden event.";
  }
}

void OnScreenKeyboard::DispatchShowEvent(int ticket) {
  if (ResolvePromise(ticket, &ticket_to_show_promise_map_)) {
    DispatchEvent(new dom::Event(base::Tokens::show()));
  } else {
    LOG(ERROR) << "No promise matching ticket for OnScreenKeyboardShown event.";
  }
}

void OnScreenKeyboard::DispatchFocusEvent(int ticket) {
  if (ResolvePromise(ticket, &ticket_to_focus_promise_map_)) {
    DispatchEvent(new dom::Event(base::Tokens::focus()));
  } else {
    LOG(ERROR)
        << "No promise matching ticket for OnScreenKeyboardFocused event.";
  }
}

void OnScreenKeyboard::DispatchBlurEvent(int ticket) {
  if (ResolvePromise(ticket, &ticket_to_blur_promise_map_)) {
    DispatchEvent(new dom::Event(base::Tokens::blur()));
  } else {
    LOG(ERROR)
        << "No promise matching ticket for OnScreenKeyboardBlurred event.";
  }
}

void OnScreenKeyboard::DispatchSuggestionsUpdatedEvent(int ticket) {
  if (ResolvePromise(ticket, &ticket_to_update_suggestions_promise_map_)) {
    DispatchEvent(new dom::Event(base::Tokens::suggestionsUpdated()));
  } else {
    LOG(ERROR) << "No promise matching ticket for "
                  "OnScreenKeyboardSuggestionsUpdated event.";
  }
}

bool OnScreenKeyboard::ResolvePromise(
    int ticket, TicketToPromiseMap* ticket_to_promise_map) {
  if (bridge_->IsValidTicket(ticket)) {
    TicketToPromiseMap::const_iterator it = ticket_to_promise_map->find(ticket);
    if (it == ticket_to_promise_map->end()) {
      return false;
    }
    it->second->value().Resolve();
    ticket_to_promise_map->erase(it);
  }
  return true;
}

}  // namespace dom
}  // namespace cobalt
