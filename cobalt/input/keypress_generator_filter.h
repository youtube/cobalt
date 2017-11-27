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

#ifndef COBALT_INPUT_KEYPRESS_GENERATOR_FILTER_H_
#define COBALT_INPUT_KEYPRESS_GENERATOR_FILTER_H_

#include "cobalt/base/token.h"
#include "cobalt/dom/keyboard_event_init.h"
#include "cobalt/input/key_event_handler.h"

namespace cobalt {
namespace input {

// Generates an additional keypress event for keydown events that correspond
// to a printable character or BS/Enter.
class KeypressGeneratorFilter : public KeyEventHandler {
 public:
  explicit KeypressGeneratorFilter(const KeyboardEventCallback& callback);

  explicit KeypressGeneratorFilter(KeyEventHandler* filter);

  // Conditionally generates an additional keypress event.
  // Passes on the new and original events for further processing/handling.
  void HandleKeyboardEvent(base::Token type,
                           const dom::KeyboardEventInit& event) override;

 protected:
  // Generates a keypress event, if:
  // 1. The original event is a keydown.
  // 2. The keycode corresponds to a printable character, or BS/Enter.
  bool ConditionallyGenerateKeypressEvent(
      base::Token type, const dom::KeyboardEventInit& orig_event);
};

}  // namespace input
}  // namespace cobalt

#endif  // COBALT_INPUT_KEYPRESS_GENERATOR_FILTER_H_
