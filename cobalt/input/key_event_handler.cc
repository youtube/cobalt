// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/input/key_event_handler.h"

namespace cobalt {
namespace input {

void KeyEventHandler::HandleKeyboardEvent(
    base::Token type, const dom::KeyboardEventInit& keyboard_event) {
  DispatchKeyboardEvent(type, keyboard_event);
}

KeyEventHandler::KeyEventHandler(const KeyboardEventCallback& callback)
    : keyboard_event_callback_(callback), keyboard_event_filter_(NULL) {}

KeyEventHandler::KeyEventHandler(KeyEventHandler* filter)
    : keyboard_event_filter_(filter) {}

void KeyEventHandler::DispatchKeyboardEvent(
    base::Token type, const dom::KeyboardEventInit& keyboard_event) const {
  // If we have a key filter attached to this object, let it filter the event,
  // otherwise call the stored callback function directly.
  if (keyboard_event_filter_) {
    keyboard_event_filter_->HandleKeyboardEvent(type, keyboard_event);
  } else {
    keyboard_event_callback_.Run(type, keyboard_event);
  }
}

}  // namespace input
}  // namespace cobalt
