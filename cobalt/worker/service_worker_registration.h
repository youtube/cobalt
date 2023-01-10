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

#ifndef COBALT_WORKER_SERVICE_WORKER_REGISTRATION_H_
#define COBALT_WORKER_SERVICE_WORKER_REGISTRATION_H_

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/event_target.h"
#include "cobalt/worker/navigation_preload_manager.h"
#include "cobalt/worker/service_worker.h"
#include "cobalt/worker/service_worker_registration_object.h"
#include "cobalt/worker/service_worker_state.h"
#include "cobalt/worker/service_worker_update_via_cache.h"
#include "url/gurl.h"

namespace cobalt {
namespace worker {

// The ServiceWorkerRegistration interface represents a service worker
// registration within a service worker client realm.
//   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#serviceworker-interface
class ServiceWorkerRegistration : public web::EventTarget {
 public:
  ServiceWorkerRegistration(
      script::EnvironmentSettings* settings,
      worker::ServiceWorkerRegistrationObject* registration);
  ~ServiceWorkerRegistration() override = default;

  void set_installing(scoped_refptr<ServiceWorker> worker) {
    installing_ = worker;
  }
  scoped_refptr<ServiceWorker> installing() const { return installing_; }
  void set_waiting(scoped_refptr<ServiceWorker> worker) { waiting_ = worker; }
  scoped_refptr<ServiceWorker> waiting() const { return waiting_; }
  void set_active(scoped_refptr<ServiceWorker> worker) { active_ = worker; }
  scoped_refptr<ServiceWorker> active() const { return active_; }
  scoped_refptr<NavigationPreloadManager> navigation_preload() {
    return navigation_preload_;
  }

  std::string scope() const;
  ServiceWorkerUpdateViaCache update_via_cache() const;

  void EnableNavigationPreload(bool enable);
  void SetNavigationPreloadHeader();

  script::HandlePromiseWrappable Update();
  script::HandlePromiseBool Unregister();

  const EventListenerScriptValue* onupdatefound() const {
    return GetAttributeEventListener(base::Tokens::updatefound());
  }

  void set_onupdatefound(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::updatefound(), event_listener);
  }

  DEFINE_WRAPPABLE_TYPE(ServiceWorkerRegistration);

 private:
  void UpdateTask(std::unique_ptr<script::ValuePromiseWrappable::Reference>
                      promise_reference);

  scoped_refptr<worker::ServiceWorkerRegistrationObject> registration_;
  scoped_refptr<ServiceWorker> installing_;
  scoped_refptr<ServiceWorker> waiting_;
  scoped_refptr<ServiceWorker> active_;
  scoped_refptr<NavigationPreloadManager> navigation_preload_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_SERVICE_WORKER_REGISTRATION_H_
