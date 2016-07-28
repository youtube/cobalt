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

#include "cobalt/dom/ui_event.h"

#include "base/compiler_specific.h"
#include "cobalt/base/token.h"

namespace cobalt {
namespace dom {

UIEvent::UIEvent(const std::string& type) : Event(type) {}

UIEvent::UIEvent(UninitializedFlag uninitialized_flag)
    : Event(uninitialized_flag) {}

UIEvent::UIEvent(base::Token type) : Event(type) {}

UIEvent::UIEvent(base::Token type, Bubbles bubbles, Cancelable cancelable)
    : Event(type, bubbles, cancelable) {}

void UIEvent::InitUIEvent(const std::string& type, bool bubbles,
                          bool cancelable, const scoped_refptr<Window>& view,
                          int32 detail) {
  UNREFERENCED_PARAMETER(detail);
  InitEvent(type, bubbles, cancelable);
  view_ = view;
}

}  // namespace dom
}  // namespace cobalt
