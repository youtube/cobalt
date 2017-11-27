// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_INPUT_KEY_REPEAT_FILTER_H_
#define COBALT_INPUT_KEY_REPEAT_FILTER_H_

#include <string>

#include "base/optional.h"
#include "base/timer.h"
#include "cobalt/dom/keyboard_event.h"
#include "cobalt/input/key_event_handler.h"

namespace cobalt {
namespace input {

class KeyRepeatFilter : public KeyEventHandler {
 public:
  explicit KeyRepeatFilter(const KeyboardEventCallback& callback);

  explicit KeyRepeatFilter(KeyEventHandler* filter);

  void HandleKeyboardEvent(
      base::Token type, const dom::KeyboardEventInit& keyboard_event) override;

 private:
  void HandleKeyDown(base::Token type,
                     const dom::KeyboardEventInit& keyboard_event);
  void HandleKeyUp(base::Token type,
                   const dom::KeyboardEventInit& keyboard_event);
  void FireKeyRepeatEvent();

  base::optional<dom::KeyboardEventInit> last_event_data_;

  base::RepeatingTimer<KeyRepeatFilter> key_repeat_timer_;
};

}  // namespace input
}  // namespace cobalt

#endif  // COBALT_INPUT_KEY_REPEAT_FILTER_H_
