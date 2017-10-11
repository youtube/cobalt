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

#include "cobalt/dom/input_event.h"

#include <string>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/base/token.h"
#include "cobalt/base/tokens.h"

namespace cobalt {
namespace dom {

InputEvent::InputEvent(const std::string& type)
    : UIEvent(base::Token(type), kBubbles, kCancelable, NULL),
      data_(""),
      is_composing_(false) {}

InputEvent::InputEvent(const std::string& type, const InputEventInit& init_dict)
    : UIEvent(base::Token(type), kBubbles, kCancelable, init_dict.view(),
              init_dict),
      data_(init_dict.data()),
      is_composing_(init_dict.is_composing()) {}

InputEvent::InputEvent(base::Token type, const scoped_refptr<Window>& view,
                       const InputEventInit& init_dict)
    : UIEvent(type, kBubbles, kCancelable, view, init_dict),
      data_(init_dict.data()),
      is_composing_(init_dict.is_composing()) {}

void InputEvent::InitInputEvent(const std::string& type, bool bubbles,
                                bool cancelable,
                                const scoped_refptr<Window>& view,
                                const std::string& data, bool is_composing) {
  InitUIEvent(type, bubbles, cancelable, view, 0);
  data_ = data;
  is_composing_ = is_composing;
}

}  // namespace dom
}  // namespace cobalt
