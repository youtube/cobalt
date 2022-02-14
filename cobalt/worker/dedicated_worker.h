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

#ifndef COBALT_WORKER_DEDICATED_WORKER_H_
#define COBALT_WORKER_DEDICATED_WORKER_H_

#include <memory>
#include <string>

#include "cobalt/dom/event_target.h"
#include "cobalt/dom/event_target_listener_info.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/worker/abstract_worker.h"
#include "cobalt/worker/message_port.h"
#include "cobalt/worker/worker.h"
#include "cobalt/worker/worker_options.h"

namespace cobalt {
namespace worker {


// Implementation of Dedicated workers and the Worker interface.
//   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#dedicated-workers-and-the-worker-interface
class DedicatedWorker : public AbstractWorker, public dom::EventTarget {
 public:
  DedicatedWorker(script::EnvironmentSettings* settings,
                  const std::string& scriptURL);
  DedicatedWorker(script::EnvironmentSettings* settings,
                  const std::string& scriptURL, const WorkerOptions& options);

  // Web API: Worker
  //
  // This may help for adding support of 'object'
  // void postMessage(any message, object transfer);
  // -> void PostMessage(const script::ValueHandleHolder& message,
  //                     script::Sequence<script::ValueHandle*> transfer) {}
  void PostMessage(const script::ValueHandleHolder& message,
                   const script::Sequence<std::string>& options);

  const EventListenerScriptValue* onmessage() const {
    return GetAttributeEventListener(base::Tokens::message());
  }
  void set_onmessage(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::message(), event_listener);
  }

  const EventListenerScriptValue* onmessageerror() const {
    return GetAttributeEventListener(base::Tokens::messageerror());
  }
  void set_onmessageerror(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::messageerror(), event_listener);
  }

  // Web API: Abstract Worker
  //
  const EventListenerScriptValue* onerror() const override {
    return GetAttributeEventListener(base::Tokens::error());
  }
  void set_onerror(const EventListenerScriptValue& event_listener) override {
    SetAttributeEventListener(base::Tokens::error(), event_listener);
  }

  void Terminate();
  DEFINE_WRAPPABLE_TYPE(DedicatedWorker);

 private:
  ~DedicatedWorker() override;
  void Initialize();
  DISALLOW_COPY_AND_ASSIGN(DedicatedWorker);

  script::EnvironmentSettings* settings_;
  const std::string script_url_;
  const WorkerOptions options_;
  std::unique_ptr<MessagePort> outside_port_;
  std::unique_ptr<Worker> worker_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_DEDICATED_WORKER_H_
