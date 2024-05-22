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

#ifndef COBALT_DOM_FOCUS_EVENT_H_
#define COBALT_DOM_FOCUS_EVENT_H_

#include <string>

#include "cobalt/base/token.h"
#include "cobalt/dom/ui_event.h"
#include "cobalt/web/event_target.h"

namespace cobalt {
namespace dom {

// The FocusEvent interface provides specific contextual information associated
// with Focus events.
//   https://www.w3.org/TR/uievents/#interface-FocusEvent
class FocusEvent : public UIEvent {
 public:
  explicit FocusEvent(const std::string& type);

  FocusEvent(base_token::Token type, Bubbles bubbles, Cancelable cancelable,
             const scoped_refptr<Window>& view,
             const scoped_refptr<web::EventTarget>& related_target);

  // Web API: FocusEvent
  //
  const scoped_refptr<web::EventTarget>& related_target() const {
    return related_target_;
  }

  DEFINE_WRAPPABLE_TYPE(FocusEvent);
  void TraceMembers(script::Tracer* tracer) override;

 protected:
  scoped_refptr<web::EventTarget> related_target_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_FOCUS_EVENT_H_
