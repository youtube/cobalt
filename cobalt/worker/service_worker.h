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

#ifndef COBALT_WORKER_SERVICE_WORKER_H_
#define COBALT_WORKER_SERVICE_WORKER_H_

#include <memory>
#include <string>
#include <utility>

#include "cobalt/script/environment_settings.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/event_target.h"
#include "cobalt/web/event_target_listener_info.h"
#include "cobalt/web/message_port.h"
#include "cobalt/worker/abstract_worker.h"
#include "cobalt/worker/service_worker_object.h"
#include "cobalt/worker/service_worker_state.h"

namespace cobalt {
namespace worker {

// The ServiceWorker interface represents a service worker within a service
// worker client realm.
//   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#serviceworker-interface
class ServiceWorker : public AbstractWorker, public web::EventTarget {
 public:
  ServiceWorker(script::EnvironmentSettings* settings,
                ServiceWorkerObject* worker);
  ServiceWorker(const ServiceWorker&) = delete;
  ServiceWorker& operator=(const ServiceWorker&) = delete;

  // Web API: ServiceWorker
  //
  void PostMessage(const script::ValueHandleHolder& message);

  // The scriptURL getter steps are to return the
  // service worker's serialized script url.
  std::string script_url() const { return worker_->script_url().spec(); }

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dom-serviceworker-state
  void set_state(ServiceWorkerState state) {
    if (worker_) worker_->set_state(state);
  }
  ServiceWorkerState state() const {
    return worker_ ? worker_->state() : kServiceWorkerStateParsed;
  }

  const EventListenerScriptValue* onstatechange() const {
    return GetAttributeEventListener(base::Tokens::statechange());
  }
  void set_onstatechange(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::statechange(), event_listener);
  }

  // Web API: Abstract Worker
  //
  const EventListenerScriptValue* onerror() const override {
    return GetAttributeEventListener(base::Tokens::error());
  }
  void set_onerror(const EventListenerScriptValue& event_listener) override {
    SetAttributeEventListener(base::Tokens::error(), event_listener);
  }

  const scoped_refptr<ServiceWorkerObject>& service_worker_object() {
    return worker_;
  }

  DEFINE_WRAPPABLE_TYPE(ServiceWorker);

 private:
  ~ServiceWorker() override { worker_ = nullptr; }

  scoped_refptr<ServiceWorkerObject> worker_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_SERVICE_WORKER_H_
