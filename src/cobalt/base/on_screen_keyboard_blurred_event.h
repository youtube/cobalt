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

#ifndef COBALT_BASE_ON_SCREEN_KEYBOARD_BLURRED_EVENT_H_
#define COBALT_BASE_ON_SCREEN_KEYBOARD_BLURRED_EVENT_H_

#include "cobalt/base/event.h"
#include "starboard/event.h"

namespace base {

class OnScreenKeyboardBlurredEvent : public Event {
 public:
  explicit OnScreenKeyboardBlurredEvent(int ticket) : ticket_(ticket) {}

  int ticket() const { return ticket_; }

  BASE_EVENT_SUBCLASS(OnScreenKeyboardBlurredEvent);

 private:
  int ticket_;
};

}  // namespace base

#endif  // COBALT_BASE_ON_SCREEN_KEYBOARD_BLURRED_EVENT_H_
