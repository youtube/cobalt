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

#ifndef COBALT_DOM_INPUT_EVENT_H_
#define COBALT_DOM_INPUT_EVENT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "cobalt/dom/input_event_init.h"
#include "cobalt/dom/ui_event_with_key_state.h"

namespace cobalt {
namespace dom {

// The InputEvent provides specific contextual information associated with
// input devices.
// Input events are commonly directed at the element that has the focus.
//   https://www.w3.org/TR/2016/WD-uievents-20160804/#events-inputevents
class InputEvent : public UIEvent {
 public:
  // Web API: InputEvent
  //
  explicit InputEvent(const std::string& type);
  InputEvent(const std::string& type, const InputEventInit& init_dict);
  InputEvent(base::Token type, const scoped_refptr<Window>& view,
             const InputEventInit& init_dict);

  void InitInputEvent(const std::string& type, bool bubbles, bool cancelable,
                      const scoped_refptr<Window>& view,
                      const std::string& data, bool is_composing);

  // Returns the data.
  std::string data() const { return data_; }

  bool is_composing() const { return is_composing_; }

  DEFINE_WRAPPABLE_TYPE(InputEvent);

 private:
  ~InputEvent() override {}

  std::string data_;
  bool is_composing_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_INPUT_EVENT_H_
