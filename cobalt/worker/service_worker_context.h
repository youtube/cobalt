// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_WORKER_SERVICE_WORKER_CONTEXT_H_
#define COBALT_WORKER_SERVICE_WORKER_CONTEXT_H_

#include <memory>
#include <set>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "base/synchronization/waitable_event.h"
#include "cobalt/network/network_module.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/web/context.h"
#include "cobalt/web/web_settings.h"
#include "cobalt/worker/client_query_options.h"
#include "cobalt/worker/service_worker_object.h"
#include "cobalt/worker/service_worker_registration_map.h"
#include "cobalt/worker/service_worker_registration_object.h"
#include "cobalt/worker/worker_type.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace cobalt {
namespace worker {

class ServiceWorkerJobs;

// Algorithms for Service Workers.
//   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#algorithms
class ServiceWorkerContext {
 public:
  enum RegistrationState { kInstalling, kWaiting, kActive };

  ServiceWorkerContext(web::WebSettings* web_settings,
                       network::NetworkModule* network_module,
                       web::UserAgentPlatformInfo* platform_info,
                       base::MessageLoop* message_loop, const GURL& url);
  ~ServiceWorkerContext();

  base::MessageLoop* message_loop() { return message_loop_; }

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#start-register-algorithm
  void StartRegister(const base::Optional<GURL>& scope_url,
                     const GURL& script_url,
                     std::unique_ptr<script::ValuePromiseWrappable::Reference>
                         promise_reference,
                     web::Context* client, const WorkerType& type,
                     const ServiceWorkerUpdateViaCache& update_via_cache);

  void MaybeResolveReadyPromiseSubSteps(web::Context* client);

  // Sub steps (8) of ServiceWorkerContainer.getRegistration().
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#navigator-service-worker-getRegistration
  void GetRegistrationSubSteps(
      const url::Origin& storage_key, const GURL& client_url,
      web::Context* client,
      std::unique_ptr<script::ValuePromiseWrappable::Reference>
          promise_reference);

  void GetRegistrationsSubSteps(
      const url::Origin& storage_key, web::Context* client,
      std::unique_ptr<script::ValuePromiseSequenceWrappable::Reference>
          promise_reference);

  // Sub steps (2) of ServiceWorkerGlobalScope.skipWaiting().
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dom-serviceworkerglobalscope-skipwaiting
  void SkipWaitingSubSteps(
      web::Context* worker_context, ServiceWorkerObject* service_worker,
      std::unique_ptr<script::ValuePromiseVoid::Reference> promise_reference);

  // Sub steps for ExtendableEvent.WaitUntil().
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dom-extendableevent-waituntil
  void WaitUntilSubSteps(ServiceWorkerRegistrationObject* registration);

  // Parallel sub steps (2) for algorithm for Clients.get(id):
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#clients-get
  void ClientsGetSubSteps(
      web::Context* worker_context,
      ServiceWorkerObject* associated_service_worker,
      std::unique_ptr<script::ValuePromiseWrappable::Reference>
          promise_reference,
      const std::string& id);

  // Parallel sub steps (2) for algorithm for Clients.matchAll():
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#clients-matchall
  void ClientsMatchAllSubSteps(
      web::Context* worker_context,
      ServiceWorkerObject* associated_service_worker,
      std::unique_ptr<script::ValuePromiseSequenceWrappable::Reference>
          promise_reference,
      bool include_uncontrolled, ClientType type);

  // Parallel sub steps (3) for algorithm for Clients.claim():
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dom-clients-claim
  void ClaimSubSteps(
      web::Context* worker_context,
      ServiceWorkerObject* associated_service_worker,
      std::unique_ptr<script::ValuePromiseVoid::Reference> promise_reference);

  // Parallel sub steps (6) for algorithm for ServiceWorker.postMessage():
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#service-worker-postmessage-options
  void ServiceWorkerPostMessageSubSteps(
      ServiceWorkerObject* service_worker, web::Context* incumbent_client,
      std::unique_ptr<script::StructuredClone> structured_clone);

  // Registration of web contexts that may have service workers.
  void RegisterWebContext(web::Context* context);
  void UnregisterWebContext(web::Context* context);
  bool IsWebContextRegistered(web::Context* context) {
    DCHECK(base::MessageLoop::current() == message_loop());
    return web_context_registrations_.end() !=
           web_context_registrations_.find(context);
  }

  // Ensure no references are kept to JS objects for a client that is about to
  // be shutdown.
  void PrepareForClientShutdown(web::Context* client);

  // Set the active worker for a client if there is a matching service worker.
  void SetActiveWorker(web::EnvironmentSettings* client);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#activation-algorithm
  void Activate(scoped_refptr<ServiceWorkerRegistrationObject> registration);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#clear-registration-algorithm
  void ClearRegistration(ServiceWorkerRegistrationObject* registration);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#soft-update
  void SoftUpdate(ServiceWorkerRegistrationObject* registration,
                  bool force_bypass_cache);

  void EnsureServiceWorkerStarted(const url::Origin& storage_key,
                                  const GURL& client_url,
                                  base::WaitableEvent* done_event);

  void EraseRegistrationMap();

  ServiceWorkerJobs* jobs() { return jobs_.get(); }
  ServiceWorkerRegistrationMap* registration_map() {
    return scope_to_registration_map_.get();
  }
  const std::set<web::Context*>& web_context_registrations() const {
    return web_context_registrations_;
  }

 private:
  friend class ServiceWorkerJobs;

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#run-service-worker-algorithm
  // The return value is a 'Completion or failure'.
  // A failure is signaled by returning nullptr. Otherwise, the returned string
  // points to the value of the Completion returned by the script runner
  // abstraction.
  std::string* RunServiceWorker(ServiceWorkerObject* worker,
                                bool force_bypass_cache = false);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#try-activate-algorithm
  void TryActivate(ServiceWorkerRegistrationObject* registration);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#service-worker-has-no-pending-events
  bool ServiceWorkerHasNoPendingEvents(
      scoped_refptr<ServiceWorkerObject> worker);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#update-registration-state-algorithm
  void UpdateRegistrationState(
      ServiceWorkerRegistrationObject* registration, RegistrationState target,
      const scoped_refptr<ServiceWorkerObject>& source);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#update-state-algorithm
  void UpdateWorkerState(scoped_refptr<ServiceWorkerObject> worker,
                         ServiceWorkerState state);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#on-client-unload-algorithm
  void HandleServiceWorkerClientUnload(web::Context* client);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#terminate-service-worker
  void TerminateServiceWorker(ServiceWorkerObject* worker);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#notify-controller-change-algorithm
  void NotifyControllerChange(web::Context* client);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#try-clear-registration-algorithm
  void TryClearRegistration(ServiceWorkerRegistrationObject* registration);

  bool IsAnyClientUsingRegistration(
      ServiceWorkerRegistrationObject* registration);

  // Returns false when the timeout is reached.
  bool WaitForAsynchronousExtensions(
      const scoped_refptr<ServiceWorkerRegistrationObject>& registration);

  base::MessageLoop* message_loop_;

  std::unique_ptr<ServiceWorkerRegistrationMap> scope_to_registration_map_;

  std::unique_ptr<ServiceWorkerJobs> jobs_;

  std::set<web::Context*> web_context_registrations_;

  base::WaitableEvent web_context_registrations_cleared_ = {
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED};
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_SERVICE_WORKER_CONTEXT_H_
