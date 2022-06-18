// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_WORKER_EXTENDABLE_EVENT_H_
#define COBALT_WORKER_EXTENDABLE_EVENT_H_

#include <string>

#include "cobalt/base/token.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/v8c/native_promise.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/event.h"
#include "cobalt/worker/extendable_event_init.h"

namespace cobalt {
namespace worker {

class ExtendableEvent : public web::Event {
 public:
  explicit ExtendableEvent(const std::string& type) : Event(type) {}
  explicit ExtendableEvent(base::Token type) : Event(type) {}
  ExtendableEvent(const std::string& type, const ExtendableEventInit& init_dict)
      : Event(type, init_dict) {}

  void WaitUntil(script::EnvironmentSettings* settings,
                 const script::Promise<script::ValueHandle>& promise) {
    // TODO(b/228976500): Implement WaitUntil().
    NOTIMPLEMENTED();
  }

  DEFINE_WRAPPABLE_TYPE(ExtendableEvent);

 protected:
  ~ExtendableEvent() override {}
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_EXTENDABLE_EVENT_H_
