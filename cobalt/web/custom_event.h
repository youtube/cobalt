// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_WEB_CUSTOM_EVENT_H_
#define COBALT_WEB_CUSTOM_EVENT_H_

#include <memory>
#include <string>

#include "cobalt/script/environment_settings.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/web/custom_event_init.h"
#include "cobalt/web/environment_settings_helper.h"
#include "cobalt/web/event.h"

namespace cobalt {
namespace web {

// Events using the CustomEvent interface can be used to carry custom data.
//   https://www.w3.org/TR/2015/REC-dom-20151119/#customevent
class CustomEvent : public web::Event {
 public:
  explicit CustomEvent(script::EnvironmentSettings* environment_settings,
                       const std::string& type)
      : Event(type) {}
  CustomEvent(script::EnvironmentSettings* environment_settings,
              const std::string& type, const CustomEventInit& init_dict)
      : Event(type, init_dict) {
    set_detail(environment_settings, init_dict.detail());
  }

  // Creates an event with its "initialized flag" unset.
  explicit CustomEvent(UninitializedFlag uninitialized_flag)
      : Event(uninitialized_flag) {}

  // Web API: CustomEvent
  //
  void InitCustomEvent(script::EnvironmentSettings* environment_settings,
                       const std::string& type, bool bubbles, bool cancelable,
                       const script::ValueHandleHolder& detail) {
    InitEvent(type, bubbles, cancelable);
    set_detail(environment_settings, &detail);
  }

  void set_detail(script::EnvironmentSettings* environment_settings,
                  const script::ValueHandleHolder* detail) {
    if (detail) {
      DCHECK(environment_settings);
      auto* wrappable = get_global_wrappable(environment_settings);
      detail_.reset(
          new script::ValueHandleHolder::Reference(wrappable, *detail));
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

  std::unique_ptr<script::ValueHandleHolder::Reference> detail_;
};

}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_CUSTOM_EVENT_H_
