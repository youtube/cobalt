/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/input/key_repeat_filter.h"

#include "cobalt/dom/event_names.h"

namespace cobalt {
namespace input {

namespace {

const base::TimeDelta kRepeatInitialDelay =
    base::TimeDelta::FromMilliseconds(500);
const float kRepeatRateInHertz = 10.0f;
const base::TimeDelta kRepeatRate = base::TimeDelta::FromMilliseconds(
    base::Time::kMillisecondsPerSecond / kRepeatRateInHertz);

}  // namespace

KeyRepeatFilter::KeyRepeatFilter(const KeyboardEventCallback& callback)
    : keyboard_event_callback_(callback) {}

void KeyRepeatFilter::HandleKeyEvent(
    const scoped_refptr<dom::KeyboardEvent>& keyboard_event) {
  if (keyboard_event->type() == dom::EventNames::GetInstance()->keydown()) {
    HandleKeyDown(keyboard_event);
  }

  if (keyboard_event->type() == dom::EventNames::GetInstance()->keyup()) {
    HandleKeyUp(keyboard_event);
  }
}

void KeyRepeatFilter::HandleKeyDown(
    const scoped_refptr<dom::KeyboardEvent>& keyboard_event) {
  keyboard_event_ = keyboard_event;
  keyboard_event_callback_.Run(keyboard_event);

  // This key down event is triggered for the first time, so start the timer
  // with |kRepeatInitialDelay|.
  key_repeat_timer_.Start(FROM_HERE, kRepeatInitialDelay, this,
                          &KeyRepeatFilter::FireKeyRepeatEvent);
}

void KeyRepeatFilter::HandleKeyUp(
    const scoped_refptr<dom::KeyboardEvent>& keyboard_event) {
  keyboard_event_callback_.Run(keyboard_event);

  // If it is a key up event and it matches the previous one, stop the key
  // repeat timer.
  if (keyboard_event_->key_code() == keyboard_event->key_code()) {
    key_repeat_timer_.Stop();
  }
}

void KeyRepeatFilter::FireKeyRepeatEvent() {
  DCHECK(keyboard_event_);
  keyboard_event_callback_.Run(keyboard_event_);

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
