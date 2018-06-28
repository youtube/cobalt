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

#include "cobalt/dom/ui_event.h"

#include "base/compiler_specific.h"
#include "cobalt/base/token.h"

namespace cobalt {
namespace dom {

UIEvent::UIEvent(const std::string& type) : Event(type), detail_(0) {}
UIEvent::UIEvent(const std::string& type, const UIEventInit& init_dict)
    : Event(type, init_dict),
      view_(init_dict.view()),
      detail_(init_dict.detail()),
      which_(init_dict.which()) {}

UIEvent::UIEvent(UninitializedFlag uninitialized_flag)
    : Event(uninitialized_flag), detail_(0), which_(0) {}

UIEvent::UIEvent(base::Token type, Bubbles bubbles, Cancelable cancelable,
                 const scoped_refptr<Window>& view)
    : Event(type, bubbles, cancelable), view_(view), detail_(0), which_(0) {}

void UIEvent::InitUIEvent(const std::string& type, bool bubbles,
                          bool cancelable, const scoped_refptr<Window>& view,
                          int32 detail) {
  InitEvent(type, bubbles, cancelable);
  view_ = view;
  detail_ = detail;
}

UIEvent::UIEvent(base::Token type) : Event(type), detail_(0), which_(0) {}
UIEvent::UIEvent(base::Token type, Bubbles bubbles, Cancelable cancelable,
                 const scoped_refptr<Window>& view,
                 const UIEventInit& init_dict)
    : Event(type, bubbles, cancelable),
      view_(view),
      detail_(init_dict.detail()),
      which_(init_dict.which()) {}

}  // namespace dom
}  // namespace cobalt
