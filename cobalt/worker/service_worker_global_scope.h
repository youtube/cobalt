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

#ifndef COBALT_WORKER_SERVICE_WORKER_GLOBAL_SCOPE_H_
#define COBALT_WORKER_SERVICE_WORKER_GLOBAL_SCOPE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/base/tokens.h"
#include "cobalt/loader/fetch_interceptor_coordinator.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/cache_utils.h"
#include "cobalt/web/event_target.h"
#include "cobalt/web/event_target_listener_info.h"
#include "cobalt/worker/clients.h"
#include "cobalt/worker/service_worker.h"
#include "cobalt/worker/service_worker_object.h"
#include "cobalt/worker/service_worker_registration.h"
#include "cobalt/worker/worker_global_scope.h"
#include "net/base/load_timing_info.h"

namespace cobalt {
namespace worker {

// Implementation of Service Worker Global Scope.
//   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#serviceworkerglobalscope-interface

class ServiceWorkerGlobalScope : public WorkerGlobalScope,
                                 public loader::FetchInterceptor {
 public:
  explicit ServiceWorkerGlobalScope(script::EnvironmentSettings* settings,
                                    ServiceWorkerObject* service_worker);
  ServiceWorkerGlobalScope(const ServiceWorkerGlobalScope&) = delete;
  ServiceWorkerGlobalScope& operator=(const ServiceWorkerGlobalScope&) = delete;

  void Initialize() override;

  // From web::WindowOrWorkerGlobalScope
  //
  ServiceWorkerGlobalScope* AsServiceWorker() override { return this; }

  // Web API: WorkerGlobalScope
  //
  void ImportScripts(const std::vector<std::string>& urls,
                     script::ExceptionState* exception_state) final;

  // Web API: ServiceWorkerGlobalScope
  //
  const scoped_refptr<Clients>& clients() const { return clients_; }
  scoped_refptr<ServiceWorkerRegistration> registration() const;
  scoped_refptr<ServiceWorker> service_worker() const;
  script::HandlePromiseVoid SkipWaiting();

  void StartFetch(
      const GURL& url,
      base::OnceCallback<void(std::unique_ptr<std::string>)> callback,
      base::OnceCallback<void(const net::LoadTimingInfo&)>
          report_load_timing_info,
      base::OnceClosure fallback) override;

  const web::EventTargetListenerInfo::EventListenerScriptValue* oninstall() {
    return GetAttributeEventListener(base::Tokens::install());
  }
  void set_oninstall(
      const web::EventTargetListenerInfo::EventListenerScriptValue&
          event_listener) {
    SetAttributeEventListener(base::Tokens::install(), event_listener);
  }

  const web::EventTargetListenerInfo::EventListenerScriptValue* onactivate() {
    return GetAttributeEventListener(base::Tokens::activate());
  }
  void set_onactivate(
      const web::EventTargetListenerInfo::EventListenerScriptValue&
          event_listener) {
    SetAttributeEventListener(base::Tokens::activate(), event_listener);
  }

  const web::EventTargetListenerInfo::EventListenerScriptValue* onfetch() {
    return GetAttributeEventListener(base::Tokens::fetch());
  }
  void set_onfetch(const web::EventTargetListenerInfo::EventListenerScriptValue&
                       event_listener) {
    SetAttributeEventListener(base::Tokens::fetch(), event_listener);
  }

  const web::EventTargetListenerInfo::EventListenerScriptValue* onmessage() {
    return GetAttributeEventListener(base::Tokens::message());
  }
  void set_onmessage(
      const web::EventTargetListenerInfo::EventListenerScriptValue&
          event_listener) {
    SetAttributeEventListener(base::Tokens::message(), event_listener);
  }
  const web::EventTargetListenerInfo::EventListenerScriptValue*
  onmessageerror() {
    return GetAttributeEventListener(base::Tokens::messageerror());
  }
  void set_onmessageerror(
      const web::EventTargetListenerInfo::EventListenerScriptValue&
          event_listener) {
    SetAttributeEventListener(base::Tokens::messageerror(), event_listener);
  }

  // Custom, not in any spec.
  //
  ServiceWorkerObject* service_worker_object() const {
    return service_worker_object_;
  }

  DEFINE_WRAPPABLE_TYPE(ServiceWorkerGlobalScope);

 protected:
  ~ServiceWorkerGlobalScope() override;

 private:
  scoped_refptr<Clients> clients_;
  base::WeakPtr<ServiceWorkerObject> service_worker_object_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_SERVICE_WORKER_GLOBAL_SCOPE_H_
