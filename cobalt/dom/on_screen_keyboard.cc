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
#include "cobalt/dom/window.h"
#include "cobalt/web/environment_settings_helper.h"
#include "cobalt/web/event_target.h"

namespace cobalt {
namespace dom {
namespace {
bool IsValidRGB(int r, int g, int b) {
  if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
    return false;
  } else {
    return true;
  }
}

bool ParseColor(const char* color_str, int& r, int& g, int& b) {
  size_t len = strlen(color_str);
  if (len == 0) {
    return false;
  }

  // Handle hexadecimal color notation #RRGGBB
  int r_tmp, g_tmp, b_tmp;
  bool is_hex =
      SbStringScanF(color_str, "#%02x%02x%02x", &r_tmp, &g_tmp, &b_tmp) == 3;
  if (is_hex && IsValidRGB(r_tmp, g_tmp, b_tmp)) {
    r = r_tmp;
    g = g_tmp;
    b = b_tmp;
    return true;
  }

  // Handle rgb color notation rgb(R, G, B)
  if (!is_hex && len >= 10 && strncasecmp("rgb(", color_str, 4) == 0) {
    int rgb_tmp[3] = {-1, -1, -1};
    const char* ptr = color_str + 4;
    int i = 0;
    while (*ptr) {
      if (isdigit(*ptr)) {
        char* end;
        rgb_tmp[i++] = static_cast<int>(strtol(ptr, &end, 10));
        if (i == 3) {
          break;
        }
        ptr = (const char*)end;
      } else if (isspace(*ptr) || *ptr == ',') {
        ptr++;
      } else {
        return false;
      }
    }

    if (IsValidRGB(rgb_tmp[0], rgb_tmp[1], rgb_tmp[2])) {
      r = rgb_tmp[0];
      g = rgb_tmp[1];
      b = rgb_tmp[2];
      return true;
    }
  }
  return false;
}
}  // namespace

OnScreenKeyboard::OnScreenKeyboard(
    script::EnvironmentSettings* settings, OnScreenKeyboardBridge* bridge,
    script::ScriptValueFactory* script_value_factory)
    : web::EventTarget(settings),
      bridge_(bridge),
      script_value_factory_(script_value_factory),
      environment_settings_(settings),
      next_ticket_(0) {
  DCHECK(bridge_) << "OnScreenKeyboardBridge must not be NULL";
  suggestions_supported_ = bridge_->SuggestionsSupported();
}

script::Handle<script::Promise<void>> OnScreenKeyboard::Show() {
  script::Handle<script::Promise<void>> promise =
      script_value_factory_->CreateBasicPromise<void>();
  int ticket = next_ticket_++;
  auto* global_wrappable = web::get_global_wrappable(environment_settings_);
  bool is_emplaced =
      ticket_to_show_promise_map_
          .emplace(ticket, std::unique_ptr<VoidPromiseValue::Reference>(
                               new VoidPromiseValue::Reference(global_wrappable,
                                                               promise)))
          .second;
  DCHECK(is_emplaced);
  bridge_->Show(data_.c_str(), ticket);

  if (background_color_.has_value()) {
    int r, g, b;
    if (ParseColor(background_color_.value().c_str(), r, g, b)) {
      bridge_->SetBackgroundColor(static_cast<uint8>(r), static_cast<uint8>(g),
                                  static_cast<uint8>(b));
    } else {
      LOG(WARNING) << "Invalid on-screen keyboard background color: "
                   << background_color_.value();
    }
  }
  if (light_theme_.has_value()) {
    bridge_->SetLightTheme(light_theme_.value());
  }

  return promise;
}

script::Handle<script::Promise<void>> OnScreenKeyboard::Hide() {
  script::Handle<script::Promise<void>> promise =
      script_value_factory_->CreateBasicPromise<void>();
  int ticket = next_ticket_++;
  auto* global_wrappable = web::get_global_wrappable(environment_settings_);
  bool is_emplaced =
      ticket_to_hide_promise_map_
          .emplace(ticket, std::unique_ptr<VoidPromiseValue::Reference>(
                               new VoidPromiseValue::Reference(global_wrappable,
                                                               promise)))
          .second;
  DCHECK(is_emplaced);
  bridge_->Hide(ticket);
  return promise;
}

script::Handle<script::Promise<void>> OnScreenKeyboard::Focus() {
  script::Handle<script::Promise<void>> promise =
      script_value_factory_->CreateBasicPromise<void>();
  int ticket = next_ticket_++;
  auto* global_wrappable = web::get_global_wrappable(environment_settings_);
  bool is_emplaced =
      ticket_to_focus_promise_map_
          .emplace(ticket, std::unique_ptr<VoidPromiseValue::Reference>(
                               new VoidPromiseValue::Reference(global_wrappable,
                                                               promise)))
          .second;
  DCHECK(is_emplaced);
  bridge_->Focus(ticket);
  return promise;
}

script::Handle<script::Promise<void>> OnScreenKeyboard::Blur() {
  script::Handle<script::Promise<void>> promise =
      script_value_factory_->CreateBasicPromise<void>();
  int ticket = next_ticket_++;
  auto* global_wrappable = web::get_global_wrappable(environment_settings_);
  bool is_emplaced =
      ticket_to_blur_promise_map_
          .emplace(ticket, std::unique_ptr<VoidPromiseValue::Reference>(
                               new VoidPromiseValue::Reference(global_wrappable,
                                                               promise)))
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
    auto* global_wrappable = web::get_global_wrappable(environment_settings_);
    bool is_emplaced =
        ticket_to_update_suggestions_promise_map_
            .emplace(ticket, std::unique_ptr<VoidPromiseValue::Reference>(
                                 new VoidPromiseValue::Reference(
                                     global_wrappable, promise)))
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

const web::EventTarget::EventListenerScriptValue* OnScreenKeyboard::onshow()
    const {
  return GetAttributeEventListener(base::Tokens::show());
}
void OnScreenKeyboard::set_onshow(
    const EventListenerScriptValue& event_listener) {
  SetAttributeEventListener(base::Tokens::show(), event_listener);
}

const web::EventTarget::EventListenerScriptValue* OnScreenKeyboard::onfocus()
    const {
  return GetAttributeEventListener(base::Tokens::focus());
}
void OnScreenKeyboard::set_onfocus(
    const EventListenerScriptValue& event_listener) {
  SetAttributeEventListener(base::Tokens::focus(), event_listener);
}

const web::EventTarget::EventListenerScriptValue* OnScreenKeyboard::onblur()
    const {
  return GetAttributeEventListener(base::Tokens::blur());
}
void OnScreenKeyboard::set_onblur(
    const EventListenerScriptValue& event_listener) {
  SetAttributeEventListener(base::Tokens::blur(), event_listener);
}

const web::EventTarget::EventListenerScriptValue* OnScreenKeyboard::onhide()
    const {
  return GetAttributeEventListener(base::Tokens::hide());
}
void OnScreenKeyboard::set_onhide(
    const EventListenerScriptValue& event_listener) {
  SetAttributeEventListener(base::Tokens::hide(), event_listener);
}

const web::EventTarget::EventListenerScriptValue* OnScreenKeyboard::oninput()
    const {
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
    DispatchEvent(new web::Event(base::Tokens::hide()));
  } else {
    LOG(ERROR)
        << "No promise matching ticket for OnScreenKeyboardHidden event.";
  }
}

void OnScreenKeyboard::DispatchShowEvent(int ticket) {
  if (ResolvePromise(ticket, &ticket_to_show_promise_map_)) {
    DispatchEvent(new web::Event(base::Tokens::show()));
  } else {
    LOG(ERROR) << "No promise matching ticket for OnScreenKeyboardShown event.";
  }
}

void OnScreenKeyboard::DispatchFocusEvent(int ticket) {
  if (ResolvePromise(ticket, &ticket_to_focus_promise_map_)) {
    DispatchEvent(new web::Event(base::Tokens::focus()));
  } else {
    LOG(ERROR)
        << "No promise matching ticket for OnScreenKeyboardFocused event.";
  }
}

void OnScreenKeyboard::DispatchBlurEvent(int ticket) {
  if (ResolvePromise(ticket, &ticket_to_blur_promise_map_)) {
    DispatchEvent(new web::Event(base::Tokens::blur()));
  } else {
    LOG(ERROR)
        << "No promise matching ticket for OnScreenKeyboardBlurred event.";
  }
}

void OnScreenKeyboard::DispatchSuggestionsUpdatedEvent(int ticket) {
  if (ResolvePromise(ticket, &ticket_to_update_suggestions_promise_map_)) {
    DispatchEvent(new web::Event(base::Tokens::suggestionsUpdated()));
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
