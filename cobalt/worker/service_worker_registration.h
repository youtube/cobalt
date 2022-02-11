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
#include "cobalt/dom/event_target.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/worker/service_worker.h"
#include "cobalt/worker/service_worker_state.h"
#include "cobalt/worker/service_worker_update_via_cache.h"
#include "url/gurl.h"

namespace cobalt {
namespace worker {

class ServiceWorkerRegistration : public dom::EventTarget {
 public:
  struct Options {
    Options(int registration_id, GURL scope, ServiceWorker::Options installing,
            ServiceWorker::Options waiting, ServiceWorker::Options active)
        : registration_id(registration_id),
          scope(std::move(scope)),
          installing(std::move(installing)),
          waiting(std::move(waiting)),
          active(std::move(active)) {}

    int registration_id;
    GURL scope;
    ServiceWorker::Options installing;
    ServiceWorker::Options waiting;
    ServiceWorker::Options active;
  };

  explicit ServiceWorkerRegistration(
      script::EnvironmentSettings* settings,
      script::ScriptValueFactory* script_value_factory, const Options& options);
  ~ServiceWorkerRegistration() override = default;
  scoped_refptr<ServiceWorker> installing() { return installing_; }
  scoped_refptr<ServiceWorker> waiting() { return waiting_; }
  scoped_refptr<ServiceWorker> active() { return active_; }

  std::string scope() const;
  ServiceWorkerUpdateViaCache update_via_cache() const {
    return update_via_cache_;
  }

  script::Handle<script::Promise<void>> Update();
  script::Handle<script::Promise<void>> Unregister();

  const EventListenerScriptValue* onupdatefound() const {
    return GetAttributeEventListener(base::Tokens::updatefound());
  }

  void set_onupdatefound(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::updatefound(), event_listener);
  }

  DEFINE_WRAPPABLE_TYPE(ServiceWorkerRegistration);

 private:
  scoped_refptr<ServiceWorker> installing_;
  scoped_refptr<ServiceWorker> waiting_;
  scoped_refptr<ServiceWorker> active_;

  const int32_t registration_id_;
  const GURL scope_;
  ServiceWorkerUpdateViaCache update_via_cache_;

  script::ScriptValueFactory* script_value_factory_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_SERVICE_WORKER_REGISTRATION_H_
