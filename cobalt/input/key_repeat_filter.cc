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

#include "cobalt/input/key_repeat_filter.h"

#include "cobalt/base/token.h"
#include "cobalt/base/tokens.h"

namespace cobalt {
namespace input {

namespace {

const base::TimeDelta kRepeatInitialDelay =
    base::TimeDelta::FromMilliseconds(500);
const float kRepeatRateInHertz = 20.0f;
const base::TimeDelta kRepeatRate = base::TimeDelta::FromMilliseconds(
    base::Time::kMillisecondsPerSecond / kRepeatRateInHertz);

}  // namespace

KeyRepeatFilter::KeyRepeatFilter(const KeyboardEventCallback& callback)
    : KeyEventHandler(callback) {}

KeyRepeatFilter::KeyRepeatFilter(KeyEventHandler* filter)
    : KeyEventHandler(filter) {}

void KeyRepeatFilter::HandleKeyboardEvent(
    base::Token type, const dom::KeyboardEventInit& keyboard_event) {
  if (type == base::Tokens::keydown()) {
    HandleKeyDown(type, keyboard_event);
  }

  if (type == base::Tokens::keyup()) {
    HandleKeyUp(type, keyboard_event);
  }
}

void KeyRepeatFilter::HandleKeyDown(
    base::Token type, const dom::KeyboardEventInit& keyboard_event) {
  // Record the information of the KeyboardEvent for firing repeat events.
  last_event_data_ = keyboard_event;

  DispatchKeyboardEvent(type, keyboard_event);

  // This key down event is triggered for the first time, so start the timer
  // with |kRepeatInitialDelay|.
  key_repeat_timer_.Start(FROM_HERE, kRepeatInitialDelay, this,
                          &KeyRepeatFilter::FireKeyRepeatEvent);
}

void KeyRepeatFilter::HandleKeyUp(
    base::Token type, const dom::KeyboardEventInit& keyboard_event) {
  DispatchKeyboardEvent(type, keyboard_event);

  // If it is a key up event and it matches the previous one, stop the key
  // repeat timer.
  if (last_event_data_->key_code() == keyboard_event.key_code()) {
    key_repeat_timer_.Stop();
  }
}

void KeyRepeatFilter::FireKeyRepeatEvent() {
  dom::KeyboardEventInit repeat_event(*last_event_data_);
  repeat_event.set_repeat(true);

  DispatchKeyboardEvent(base::Tokens::keydown(), repeat_event);

  // If |FireKeyRepeatEvent| is triggered for the first time then reset the
  // timer to the repeat rate instead of the initial delay.
  if (key_repeat_timer_.GetCurrentDelay() == kRepeatInitialDelay) {
    key_repeat_timer_.Stop();
    key_repeat_timer_.Start(FROM_HERE, kRepeatRate, this,
                            &KeyRepeatFilter::FireKeyRepeatEvent);
  }
}

}  // namespace input
}  // namespace cobalt
