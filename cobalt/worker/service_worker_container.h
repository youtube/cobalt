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
#include "cobalt/worker/service_worker_context.h"
#include "cobalt/worker/service_worker_registration.h"

namespace cobalt {
namespace worker {

// The ServiceWorkerContainer interface represents the interface to register and
// access service workers from a service worker client realm.
//   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#serviceworkercontainer-interface
class ServiceWorkerContainer : public web::EventTarget {
 public:
  explicit ServiceWorkerContainer(script::EnvironmentSettings* settings);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#navigator-service-worker-controller
  scoped_refptr<ServiceWorker> controller();
  script::HandlePromiseWrappable ready();

  script::HandlePromiseWrappable Register(const std::string& url);
  script::HandlePromiseWrappable Register(const std::string& url,
                                          const RegistrationOptions& options);
  script::HandlePromiseWrappable GetRegistration(const std::string& url);
  script::HandlePromiseSequenceWrappable GetRegistrations();

  void StartMessages();

  const EventListenerScriptValue* oncontrollerchange() const {
    return GetAttributeEventListener(base::Tokens::controllerchange());
  }
  void set_oncontrollerchange(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::controllerchange(), event_listener);
  }

  void MaybeResolveReadyPromise(
      scoped_refptr<ServiceWorkerRegistrationObject> registration);

  DEFINE_WRAPPABLE_TYPE(ServiceWorkerContainer);

 private:
  ~ServiceWorkerContainer() override = default;

  void ReadyPromiseTask();

  void GetRegistrationTask(
      const std::string& url,
      std::unique_ptr<script::ValuePromiseWrappable::Reference>
          promise_reference);

  void GetRegistrationsTask(
      std::unique_ptr<script::ValuePromiseSequenceWrappable::Reference>
          promise_reference);

  script::HandlePromiseWrappable ready_promise_;
  std::unique_ptr<script::ValuePromiseWrappable::Reference> promise_reference_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_SERVICE_WORKER_CONTAINER_H_
