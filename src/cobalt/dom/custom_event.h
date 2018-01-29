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

#ifndef COBALT_DOM_CUSTOM_EVENT_H_
#define COBALT_DOM_CUSTOM_EVENT_H_

#include <string>

#include "cobalt/dom/custom_event_init.h"
#include "cobalt/dom/event.h"
#include "cobalt/script/value_handle.h"

namespace cobalt {
namespace dom {

// Events using the CustomEvent interface can be used to carry custom data.
//   https://www.w3.org/TR/2015/REC-dom-20151119/#customevent
class CustomEvent : public Event {
 public:
  explicit CustomEvent(const std::string& type) : Event(type) {}
  CustomEvent(const std::string& type, const CustomEventInit& init_dict)
      : Event(type, init_dict) {
    set_detail(init_dict.detail());
  }

  // Creates an event with its "initialized flag" unset.
  explicit CustomEvent(UninitializedFlag uninitialized_flag)
      : Event(uninitialized_flag) {}

  // Web API: CustomEvent
  //
  void InitCustomEvent(const std::string& type, bool bubbles, bool cancelable,
                       const script::ValueHandleHolder& detail) {
    InitEvent(type, bubbles, cancelable);
    set_detail(&detail);
  }

  void set_detail(const script::ValueHandleHolder* detail) {
    if (detail) {
      detail_.reset(new script::ValueHandleHolder::Reference(this, *detail));
    } else {
      detail_.reset();
    }
  }

  const script::ValueHandleHolder* detail() const {
    if (!detail_) {
      return NULL;
    }

    return &(detail_->referenced_value());
  }

  DEFINE_WRAPPABLE_TYPE(CustomEvent);

 protected:
  ~CustomEvent() override {}

  scoped_ptr<script::ValueHandleHolder::Reference> detail_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_CUSTOM_EVENT_H_
