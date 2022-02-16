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

#include "cobalt/dom/event_target.h"
#include "cobalt/dom/event_target_listener_info.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/worker/registration_options.h"
#include "cobalt/worker/service_worker_registration.h"

namespace cobalt {
namespace worker {

class ServiceWorkerContainer : public dom::EventTarget {
 public:
  ServiceWorkerContainer(script::EnvironmentSettings* settings,
                         script::ScriptValueFactory* script_value_factory);

  scoped_refptr<ServiceWorker> controller() { return controller_; }
  script::Handle<script::Promise<void>> ready();

  script::Handle<script::Promise<void>> Register(const std::string& url);
  script::Handle<script::Promise<void>> Register(
      const std::string& url, const RegistrationOptions& options);
  script::Handle<script::Promise<void>> GetRegistration(const std::string& url);
  script::Handle<script::Promise<void>> GetRegistrations();

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
  scoped_refptr<ServiceWorker> controller_;
  scoped_refptr<ServiceWorker> ready_;
  script::ScriptValueFactory* script_value_factory_;

  ~ServiceWorkerContainer() override = default;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_SERVICE_WORKER_CONTAINER_H_
