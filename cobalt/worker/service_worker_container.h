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

#ifndef COBALT_WORKER_SERVICE_WORKER_CONTAINER_H_
#define COBALT_WORKER_SERVICE_WORKER_CONTAINER_H_

#include <memory>
#include <string>

#include "cobalt/script/promise.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/event_target.h"
#include "cobalt/web/event_target_listener_info.h"
#include "cobalt/worker/registration_options.h"
#include "cobalt/worker/service_worker_jobs.h"
#include "cobalt/worker/service_worker_registration.h"

namespace cobalt {
namespace worker {

// The ServiceWorkerContainer interface represents the interface to register and
// access service workers from a service worker client realm.
//   https://w3c.github.io/ServiceWorker/#serviceworkercontainer-interface
class ServiceWorkerContainer : public web::EventTarget {
 public:
  explicit ServiceWorkerContainer(script::EnvironmentSettings* settings);

  scoped_refptr<ServiceWorker> controller() { return controller_; }
  script::Handle<script::PromiseWrappable> ready();

  script::Handle<script::PromiseWrappable> Register(const std::string& url);
  script::Handle<script::PromiseWrappable> Register(
      const std::string& url, const RegistrationOptions& options);
  script::Handle<script::PromiseWrappable> GetRegistration(
      const std::string& url);
  script::Handle<script::PromiseSequenceWrappable> GetRegistrations();

  void StartMessages();

  const EventListenerScriptValue* oncontrollerchange() const {
    return GetAttributeEventListener(base::Tokens::controllerchange());
  }
  void set_oncontrollerchange(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::controllerchange(), event_listener);
  }

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

  DEFINE_WRAPPABLE_TYPE(ServiceWorkerContainer);

 private:
  void GetRegistrationTask(
      const std::string& url,
      std::unique_ptr<script::ValuePromiseWrappable::Reference>
          promise_reference);

  scoped_refptr<ServiceWorker> controller_;
  scoped_refptr<ServiceWorker> ready_;

  ~ServiceWorkerContainer() override = default;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_SERVICE_WORKER_CONTAINER_H_
