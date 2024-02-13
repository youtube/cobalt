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

#include "cobalt/worker/service_worker_context.h"

#include <list>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/web/dom_exception.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/worker/extendable_event.h"
#include "cobalt/worker/extendable_message_event.h"
#include "cobalt/worker/service_worker_container.h"
#include "cobalt/worker/service_worker_jobs.h"
#include "cobalt/worker/window_client.h"

namespace cobalt {
namespace worker {

namespace {

const base::TimeDelta kWaitForAsynchronousExtensionsTimeout =
    base::TimeDelta::FromSeconds(3);

const base::TimeDelta kShutdownWaitTimeoutSecs =
    base::TimeDelta::FromSeconds(5);

bool PathContainsEscapedSlash(const GURL& url) {
  const std::string path = url.path();
  return (path.find("%2f") != std::string::npos ||
          path.find("%2F") != std::string::npos ||
          path.find("%5c") != std::string::npos ||
          path.find("%5C") != std::string::npos);
}

void ResolveGetClientPromise(
    web::Context* client, web::Context* worker_context,
    std::unique_ptr<script::ValuePromiseWrappable::Reference>
        promise_reference) {
  TRACE_EVENT0("cobalt::worker", "ResolveGetClientPromise()");
  // Algorithm for Resolve Get Client Promise:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#resolve-get-client-promise

  // 1. If client is an environment settings object, then:
  // 1.1. If client is not a secure context, queue a task to reject promise with
  //      a "SecurityError" DOMException, on promise’s relevant settings
  //      object's responsible event loop using the DOM manipulation task
  //      source, and abort these steps.
  // 2. Else:
  // 2.1. If client’s creation URL is not a potentially trustworthy URL, queue
  //      a task to reject promise with a "SecurityError" DOMException, on
  //      promise’s relevant settings object's responsible event loop using the
  //      DOM manipulation task source, and abort these steps.
  // In production, Cobalt requires https, therefore all clients are secure
  // contexts.

  // 3. If client is an environment settings object and is not a window client,
  //    then:
  if (!client->GetWindowOrWorkerGlobalScope()->IsWindow()) {
    // 3.1. Let clientObject be the result of running Create Client algorithm
    //      with client as the argument.
    scoped_refptr<Client> client_object =
        Client::Create(client->environment_settings());

    // 3.2. Queue a task to resolve promise with clientObject, on promise’s
    //      relevant settings object's responsible event loop using the DOM
    //      manipulation task source, and abort these steps.
    worker_context->message_loop()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::unique_ptr<script::ValuePromiseWrappable::Reference>
                   promise_reference,
               scoped_refptr<Client> client_object) {
              TRACE_EVENT0("cobalt::worker",
                           "ResolveGetClientPromise() Resolve");
              promise_reference->value().Resolve(client_object);
            },
            std::move(promise_reference), client_object));
    return;
  }
  // 4. Else:
  // 4.1. Let browsingContext be null.
  // 4.2. If client is an environment settings object, set browsingContext to
  //      client’s global object's browsing context.
  // 4.3. Else, set browsingContext to client’s target browsing context.
  // Note: Cobalt does not implement a distinction between environments and
  // environment settings objects.
  // 4.4. Queue a task to run the following steps on browsingContext’s event
  //      loop using the user interaction task source:
  // Note: The task below does not currently perform any actual
  // functionality in the client context. It is included however to help future
  // implementation for fetching values for WindowClient properties, with
  // similar logic existing in ClientsMatchAllSubSteps.
  client->message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](web::Context* client, web::Context* worker_context,
             std::unique_ptr<script::ValuePromiseWrappable::Reference>
                 promise_reference) {
            // 4.4.1. Let frameType be the result of running Get Frame Type with
            //        browsingContext.
            // Cobalt does not support nested or auxiliary browsing contexts.
            // 4.4.2. Let visibilityState be browsingContext’s active document's
            //        visibilityState attribute value.
            // 4.4.3. Let focusState be the result of running the has focus
            //        steps with browsingContext’s active document as the
            //        argument.
            // Handled in the WindowData constructor.
            std::unique_ptr<WindowData> window_data(
                new WindowData(client->environment_settings()));

            // 4.4.4. Let ancestorOriginsList be the empty list.
            // 4.4.5. If client is a window client, set ancestorOriginsList to
            //        browsingContext’s active document's relevant global
            //        object's Location object’s ancestor origins list's
            //        associated list.
            // Cobalt does not implement Location.ancestorOrigins.

            // 4.4.6. Queue a task to run the following steps on promise’s
            //        relevant settings object's responsible event loop using
            //        the DOM manipulation task source:
            worker_context->message_loop()->task_runner()->PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](std::unique_ptr<script::ValuePromiseWrappable::Reference>
                           promise_reference,
                       std::unique_ptr<WindowData> window_data) {
                      // 4.4.6.1. If client’s discarded flag is set, resolve
                      //          promise with undefined and abort these
                      //          steps.
                      // 4.4.6.2. Let windowClient be the result of running
                      //          Create Window Client with client,
                      //          frameType, visibilityState, focusState,
                      //          and ancestorOriginsList.
                      scoped_refptr<Client> window_client =
                          WindowClient::Create(*window_data);
                      // 4.4.6.3. Resolve promise with windowClient.
                      promise_reference->value().Resolve(window_client);
                    },
                    std::move(promise_reference), std::move(window_data)));
          },
          client, worker_context, std::move(promise_reference)));
  DCHECK_EQ(nullptr, promise_reference.get());
}

}  // namespace

ServiceWorkerContext::ServiceWorkerContext(
    web::WebSettings* web_settings, network::NetworkModule* network_module,
    web::UserAgentPlatformInfo* platform_info, base::MessageLoop* message_loop,
    const GURL& url)
    : message_loop_(message_loop) {
  DCHECK_EQ(message_loop_, base::MessageLoop::current());
  jobs_ =
      std::make_unique<ServiceWorkerJobs>(this, network_module, message_loop);

  ServiceWorkerPersistentSettings::Options options(web_settings, network_module,
                                                   platform_info, this, url);
  scope_to_registration_map_.reset(new ServiceWorkerRegistrationMap(options));
  DCHECK(scope_to_registration_map_);
}

ServiceWorkerContext::~ServiceWorkerContext() {
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  scope_to_registration_map_->HandleUserAgentShutdown(this);
  scope_to_registration_map_->AbortAllActive();
  scope_to_registration_map_.reset();
  if (!web_context_registrations_.empty()) {
    // Abort any Service Workers that remain.
    for (auto& context : web_context_registrations_) {
      DCHECK(context);
      if (context->GetWindowOrWorkerGlobalScope()->IsServiceWorker()) {
        ServiceWorkerGlobalScope* service_worker =
            context->GetWindowOrWorkerGlobalScope()->AsServiceWorker();
        if (service_worker && service_worker->service_worker_object()) {
          service_worker->service_worker_object()->Abort();
        }
      }
    }

    // Wait for web context registrations to be cleared.
    web_context_registrations_cleared_.TimedWait(kShutdownWaitTimeoutSecs);
  }
}

void ServiceWorkerContext::StartRegister(
    const base::Optional<GURL>& maybe_scope_url,
    const GURL& script_url_with_fragment,
    std::unique_ptr<script::ValuePromiseWrappable::Reference> promise_reference,
    web::Context* client, const WorkerType& type,
    const ServiceWorkerUpdateViaCache& update_via_cache) {
  TRACE_EVENT2("cobalt::worker", "ServiceWorkerContext::StartRegister()",
               "scope", maybe_scope_url.value_or(GURL()).spec(), "script",
               script_url_with_fragment.spec());
  DCHECK_NE(message_loop(), base::MessageLoop::current());
  DCHECK_EQ(client->message_loop(), base::MessageLoop::current());
  // Algorithm for Start Register:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#start-register-algorithm
  // 1. If scriptURL is failure, reject promise with a TypeError and abort these
  //    steps.
  if (script_url_with_fragment.is_empty()) {
    promise_reference->value().Reject(script::kTypeError);
    return;
  }

  // 2. Set scriptURL’s fragment to null.
  GURL::Replacements replacements;
  replacements.ClearRef();
  GURL script_url = script_url_with_fragment.ReplaceComponents(replacements);
  DCHECK(!script_url.has_ref() || script_url.ref().empty());
  DCHECK(!script_url.is_empty());

  // 3. If scriptURL’s scheme is not one of "http" and "https", reject promise
  //    with a TypeError and abort these steps.
  if (!script_url.SchemeIsHTTPOrHTTPS()) {
    promise_reference->value().Reject(script::kTypeError);
    return;
  }

  // 4. If any of the strings in scriptURL’s path contains either ASCII
  //    case-insensitive "%2f" or ASCII case-insensitive "%5c", reject promise
  //    with a TypeError and abort these steps.
  if (PathContainsEscapedSlash(script_url)) {
    promise_reference->value().Reject(script::kTypeError);
    return;
  }

  DCHECK(client);
  web::WindowOrWorkerGlobalScope* window_or_worker_global_scope =
      client->GetWindowOrWorkerGlobalScope();
  DCHECK(window_or_worker_global_scope);
  web::CspDelegate* csp_delegate =
      window_or_worker_global_scope->csp_delegate();
  DCHECK(csp_delegate);
  if (!csp_delegate->CanLoad(web::CspDelegate::kWorker, script_url,
                             /* did_redirect*/ false)) {
    promise_reference->value().Reject(new web::DOMException(
        web::DOMException::kSecurityErr,
        "Failed to register a ServiceWorker: The provided scriptURL ('" +
            script_url.spec() + "') violates the Content Security Policy."));
    return;
  }

  // 5. If scopeURL is null, set scopeURL to the result of parsing the string
  //    "./" with scriptURL.
  GURL scope_url = maybe_scope_url.value_or(script_url.Resolve("./"));

  // 6. If scopeURL is failure, reject promise with a TypeError and abort these
  //    steps.
  if (scope_url.is_empty()) {
    promise_reference->value().Reject(script::kTypeError);
    return;
  }

  // 7. Set scopeURL’s fragment to null.
  scope_url = scope_url.ReplaceComponents(replacements);
  DCHECK(!scope_url.has_ref() || scope_url.ref().empty());
  DCHECK(!scope_url.is_empty());

  // 8. If scopeURL’s scheme is not one of "http" and "https", reject promise
  //    with a TypeError and abort these steps.
  if (!scope_url.SchemeIsHTTPOrHTTPS()) {
    promise_reference->value().Reject(script::kTypeError);
    return;
  }

  // 9. If any of the strings in scopeURL’s path contains either ASCII
  //    case-insensitive "%2f" or ASCII case-insensitive "%5c", reject promise
  //    with a TypeError and abort these steps.
  if (PathContainsEscapedSlash(scope_url)) {
    promise_reference->value().Reject(script::kTypeError);
    return;
  }

  // 10. Let storage key be the result of running obtain a storage key given
  //     client.
  url::Origin storage_key = client->environment_settings()->ObtainStorageKey();

  // 11. Let job be the result of running Create Job with register, storage key,
  //     scopeURL, scriptURL, promise, and client.
  std::unique_ptr<ServiceWorkerJobs::Job> job = jobs_->CreateJob(
      ServiceWorkerJobs::kRegister, storage_key, scope_url, script_url,
      ServiceWorkerJobs::JobPromiseType::Create(std::move(promise_reference)),
      client);

  // 12. Set job’s worker type to workerType.
  // Cobalt only supports 'classic' worker type.

  // 13. Set job’s update via cache mode to updateViaCache.
  job->update_via_cache = update_via_cache;

  // 14. Set job’s referrer to referrer.
  // This is the same value as set in CreateJob().

  // 15. Invoke Schedule Job with job.
  jobs_->ScheduleJob(std::move(job));
}

void ServiceWorkerContext::SoftUpdate(
    ServiceWorkerRegistrationObject* registration, bool force_bypass_cache) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerContext::SoftUpdate()");
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  DCHECK(registration);
  // Algorithm for SoftUpdate:
  //    https://www.w3.org/TR/2022/CRD-service-workers-20220712/#soft-update
  // 1. Let newestWorker be the result of running Get Newest Worker algorithm
  // passing registration as its argument.
  ServiceWorkerObject* newest_worker = registration->GetNewestWorker();

  // 2. If newestWorker is null, abort these steps.
  if (newest_worker == nullptr) {
    return;
  }

  // 3. Let job be the result of running Create Job with update, registration’s
  // storage key, registration’s scope url, newestWorker’s script url, null, and
  // null.
  std::unique_ptr<ServiceWorkerJobs::Job> job = jobs_->CreateJobWithoutPromise(
      ServiceWorkerJobs::kUpdate, registration->storage_key(),
      registration->scope_url(), newest_worker->script_url());

  // 4. Set job’s worker type to newestWorker’s type.
  // Cobalt only supports 'classic' worker type.

  // 5. Set job’s force bypass cache flag if forceBypassCache is true.
  job->force_bypass_cache_flag = force_bypass_cache;

  // 6. Invoke Schedule Job with job.
  message_loop()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ServiceWorkerJobs::ScheduleJob,
                                base::Unretained(jobs_.get()), std::move(job)));
  DCHECK(!job.get());
}

void ServiceWorkerContext::EnsureServiceWorkerStarted(
    const url::Origin& storage_key, const GURL& client_url,
    base::WaitableEvent* done_event) {
  if (message_loop() != base::MessageLoop::current()) {
    message_loop()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceWorkerContext::EnsureServiceWorkerStarted,
                       base::Unretained(this), storage_key, client_url,
                       done_event));
    return;
  }
  base::ScopedClosureRunner signal_done(base::BindOnce(
      [](base::WaitableEvent* done_event) { done_event->Signal(); },
      done_event));
  base::TimeTicks start = base::TimeTicks::Now();
  auto registration =
      scope_to_registration_map_->MatchServiceWorkerRegistration(storage_key,
                                                                 client_url);
  if (!registration) {
    return;
  }
  auto service_worker_object = registration->active_worker();
  if (!service_worker_object || service_worker_object->is_running()) {
    return;
  }
  service_worker_object->ObtainWebAgentAndWaitUntilDone();
}

void ServiceWorkerContext::EraseRegistrationMap() {
  if (message_loop() != base::MessageLoop::current()) {
    message_loop()->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&ServiceWorkerContext::EraseRegistrationMap,
                                  base::Unretained(this)));
    return;
  }
  scope_to_registration_map_->DeletePersistentSettings();
}

std::string* ServiceWorkerContext::RunServiceWorker(ServiceWorkerObject* worker,
                                                    bool force_bypass_cache) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerContext::RunServiceWorker()");

  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  DCHECK(worker);
  // Algorithm for "Run Service Worker"
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#run-service-worker-algorithm

  // 1. Let unsafeCreationTime be the unsafe shared current time.
  auto unsafe_creation_time = base::TimeTicks::Now();
  // 2. If serviceWorker is running, then return serviceWorker’s start status.
  if (worker->is_running()) {
    return worker->start_status();
  }
  // 3. If serviceWorker’s state is "redundant", then return failure.
  if (worker->state() == kServiceWorkerStateRedundant) {
    return nullptr;
  }
  // 4. Assert: serviceWorker’s start status is null.
  DCHECK(worker->start_status() == nullptr);
  // 5. Let script be serviceWorker’s script resource.
  // 6. Assert: script is not null.
  DCHECK(worker->HasScriptResource());
  // 7. Let startFailed be false.
  worker->store_start_failed(false);
  // 8. Let agent be the result of obtaining a service worker agent, and run the
  //    following steps in that context:
  // 9. Wait for serviceWorker to be running, or for startFailed to be true.
  worker->ObtainWebAgentAndWaitUntilDone();
  // 10. If startFailed is true, then return failure.
  if (worker->load_start_failed()) {
    return nullptr;
  }
  // 11. Return serviceWorker’s start status.
  return worker->start_status();
}

bool ServiceWorkerContext::WaitForAsynchronousExtensions(
    const scoped_refptr<ServiceWorkerRegistrationObject>& registration) {
  // TODO(b/240164388): Investigate a better approach for combining waiting
  // for the ExtendableEvent while also allowing use of algorithms that run
  // on the same thread from the event handler.
  base::TimeTicks wait_start_time = base::TimeTicks::Now();
  do {
    if (registration->done_event()->TimedWait(
            base::TimeDelta::FromMilliseconds(100)))
      break;
#ifndef USE_HACKY_COBALT_CHANGES
    base::MessageLoopCurrent::ScopedNestableTaskAllower allow;
#endif
    base::RunLoop().RunUntilIdle();
  } while ((base::TimeTicks::Now() - wait_start_time) <
           kWaitForAsynchronousExtensionsTimeout);
  return registration->done_event()->IsSignaled();
}

bool ServiceWorkerContext::IsAnyClientUsingRegistration(
    ServiceWorkerRegistrationObject* registration) {
  bool any_client_is_using = false;
  for (auto& context : web_context_registrations_) {
    // When a service worker client is controlled by a service worker, it is
    // said that the service worker client is using the service worker’s
    // containing service worker registration.
    //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dfn-control
    if (context->GetWindowOrWorkerGlobalScope()->IsServiceWorker()) continue;
    if (context->is_controlled_by(registration->active_worker())) {
      any_client_is_using = true;
      break;
    }
  }
  return any_client_is_using;
}

void ServiceWorkerContext::TryActivate(
    ServiceWorkerRegistrationObject* registration) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerContext::TryActivate()");
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  // Algorithm for Try Activate:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#try-activate-algorithm

  // 1. If registration’s waiting worker is null, return.
  if (!registration) return;
  if (!registration->waiting_worker()) return;

  // 2. If registration’s active worker is not null and registration’s active
  //    worker's state is "activating", return.
  if (registration->active_worker() &&
      (registration->active_worker()->state() == kServiceWorkerStateActivating))
    return;

  // 3. Invoke Activate with registration if either of the following is true:

  //    - registration’s active worker is null.
  bool invoke_activate = registration->active_worker() == nullptr;

  if (!invoke_activate) {
    //    - The result of running Service Worker Has No Pending Events with
    //      registration’s active worker is true...
    if (ServiceWorkerHasNoPendingEvents(registration->active_worker())) {
      //      ... and no service worker client is using registration...
      bool any_client_using = IsAnyClientUsingRegistration(registration);
      invoke_activate = !any_client_using;
      //      ... or registration’s waiting worker's skip waiting flag is
      //      set.
      if (!invoke_activate && registration->waiting_worker()->skip_waiting())
        invoke_activate = true;
    }
  }

  if (invoke_activate) Activate(registration);
}

void ServiceWorkerContext::Activate(
    scoped_refptr<ServiceWorkerRegistrationObject> registration) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerContext::Activate()");
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  // Algorithm for Activate:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#activation-algorithm

  // 1. If registration’s waiting worker is null, abort these steps.
  if (registration->waiting_worker() == nullptr) return;
  // 2. If registration’s active worker is not null, then:
  if (registration->active_worker()) {
    // 2.1. Terminate registration’s active worker.
    TerminateServiceWorker(registration->active_worker());
    // 2.2. Run the Update Worker State algorithm passing registration’s active
    //      worker and "redundant" as the arguments.
    UpdateWorkerState(registration->active_worker(),
                      kServiceWorkerStateRedundant);
  }
  // 3. Run the Update Registration State algorithm passing registration,
  //    "active" and registration’s waiting worker as the arguments.
  UpdateRegistrationState(registration, kActive,
                          registration->waiting_worker());
  // 4. Run the Update Registration State algorithm passing registration,
  //    "waiting" and null as the arguments.
  UpdateRegistrationState(registration, kWaiting, nullptr);
  // 5. Run the Update Worker State algorithm passing registration’s active
  //    worker and "activating" as the arguments.
  UpdateWorkerState(registration->active_worker(),
                    kServiceWorkerStateActivating);
  // 6. Let matchedClients be a list of service worker clients whose creation
  //    URL matches registration’s storage key and registration’s scope url.
  std::list<web::Context*> matched_clients;
  for (auto& context : web_context_registrations_) {
    url::Origin context_storage_key =
        url::Origin::Create(context->environment_settings()->creation_url());
    scoped_refptr<ServiceWorkerRegistrationObject> matched_registration =
        scope_to_registration_map_->MatchServiceWorkerRegistration(
            context_storage_key, registration->scope_url());
    if (matched_registration == registration) {
      matched_clients.push_back(context);
    }
  }
  // 7. For each client of matchedClients, queue a task on client’s  responsible
  //    event loop, using the DOM manipulation task source, to run the following
  //    substeps:
  for (auto& client : matched_clients) {
    // 7.1. Let readyPromise be client’s global object's
    //      ServiceWorkerContainer object’s ready
    //      promise.
    // 7.2. If readyPromise is null, then continue.
    // 7.3. If readyPromise is pending, resolve
    //      readyPromise with the the result of getting
    //      the service worker registration object that
    //      represents registration in readyPromise’s
    //      relevant settings object.
    client->message_loop()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceWorkerContainer::MaybeResolveReadyPromise,
                       base::Unretained(client->GetWindowOrWorkerGlobalScope()
                                            ->navigator_base()
                                            ->service_worker()
                                            .get()),
                       base::Unretained(registration.get())));
  }
  // 8. For each client of matchedClients:
  // 8.1. If client is a window client, unassociate client’s responsible
  //      document from its application cache, if it has one.
  // 8.2. Else if client is a shared worker client, unassociate client’s
  //      global object from its application cache, if it has one.
  // Cobalt doesn't implement 'application cache':
  //   https://www.w3.org/TR/2011/WD-html5-20110525/offline.html#applicationcache
  // 9. For each service worker client client who is using registration:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dfn-use
  for (const auto& client : web_context_registrations_) {
    // When a service worker client is controlled by a service worker, it is
    // said that the service worker client is using the service worker’s
    // containing service worker registration.
    //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dfn-control
    if (client->active_service_worker() &&
        client->active_service_worker()
                ->containing_service_worker_registration() == registration) {
      // 9.1. Set client’s active worker to registration’s active worker.
      client->set_active_service_worker(registration->active_worker());
      // 9.2. Invoke Notify Controller Change algorithm with client as the
      //      argument.
      NotifyControllerChange(client);
    }
  }
  // 10. Let activeWorker be registration’s active worker.
  ServiceWorkerObject* active_worker = registration->active_worker();
  bool activated = true;
  // 11. If the result of running the Should Skip Event algorithm with
  //     activeWorker and "activate" is false, then:
  DCHECK(active_worker);
  if (!active_worker->ShouldSkipEvent(base::Tokens::activate())) {
    // 11.1. If the result of running the Run Service Worker algorithm with
    //       activeWorker is not failure, then:
    auto* run_result = RunServiceWorker(active_worker);
    if (run_result) {
      // 11.1.1. Queue a task task on activeWorker’s event loop using the DOM
      //         manipulation task source to run the following steps:
      DCHECK_EQ(active_worker->web_agent()->context(),
                active_worker->worker_global_scope()
                    ->environment_settings()
                    ->context());
      DCHECK(registration->done_event()->IsSignaled());
      registration->done_event()->Reset();
      active_worker->web_agent()
          ->context()
          ->message_loop()
          ->task_runner()
          ->PostBlockingTask(
              FROM_HERE,
              base::Bind(
                  [](ServiceWorkerObject* active_worker,
                     base::WaitableEvent* done_event) {
                    auto done_callback =
                        base::BindOnce([](base::WaitableEvent* done_event,
                                          bool) { done_event->Signal(); },
                                       done_event);
                    auto* settings = active_worker->web_agent()
                                         ->context()
                                         ->environment_settings();
                    scoped_refptr<ExtendableEvent> event(
                        new ExtendableEvent(settings, base::Tokens::activate(),
                                            std::move(done_callback)));
                    // 11.1.1.1. Let e be the result of creating an event with
                    //           ExtendableEvent.
                    // 11.1.1.2. Initialize e’s type attribute to activate.
                    // 11.1.1.3. Dispatch e at activeWorker’s global object.
                    active_worker->worker_global_scope()->DispatchEvent(event);
                    // 11.1.1.4. WaitForAsynchronousExtensions: Wait, in
                    //           parallel, until e is not active.
                    if (!event->IsActive()) {
                      // If the event handler doesn't use waitUntil(), it will
                      // already no longer be active, and there will never be a
                      // callback to signal the done event.
                      done_event->Signal();
                    }
                  },
                  base::Unretained(active_worker), registration->done_event()));
      // 11.1.2. Wait for task to have executed or been discarded.
      // This waiting is done inside PostBlockingTask above.
      // 11.1.3. Wait for the step labeled WaitForAsynchronousExtensions to
      //         complete.
      // TODO(b/240164388): Investigate a better approach for combining waiting
      // for the ExtendableEvent while also allowing use of algorithms that run
      // on the same thread from the event handler.
      if (!WaitForAsynchronousExtensions(registration)) {
        // Timeout
        activated = false;
      }
    } else {
      activated = false;
    }
  }
  // 12. Run the Update Worker State algorithm passing registration’s active
  //     worker and "activated" as the arguments.
  if (activated && registration->active_worker()) {
    UpdateWorkerState(registration->active_worker(),
                      kServiceWorkerStateActivated);

    // Persist registration since the waiting_worker has been updated to nullptr
    // and the active_worker has been updated to the previous waiting_worker.
    scope_to_registration_map_->PersistRegistration(registration->storage_key(),
                                                    registration->scope_url());
  }
}

void ServiceWorkerContext::NotifyControllerChange(web::Context* client) {
  // Algorithm for Notify Controller Change:
  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#notify-controller-change-algorithm
  // 1. Assert: client is not null.
  DCHECK(client);

  // 2. If client is an environment settings object, queue a task to fire an
  //    event named controllerchange at the ServiceWorkerContainer object that
  //    client is associated with.
  client->message_loop()->task_runner()->PostTask(
      FROM_HERE, base::Bind(
                     [](web::Context* client) {
                       client->GetWindowOrWorkerGlobalScope()
                           ->navigator_base()
                           ->service_worker()
                           ->DispatchEvent(new web::Event(
                               base::Tokens::controllerchange()));
                     },
                     client));
}

bool ServiceWorkerContext::ServiceWorkerHasNoPendingEvents(
    scoped_refptr<ServiceWorkerObject> worker) {
  // Algorithm for Service Worker Has No Pending Events
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#service-worker-has-no-pending-events
  // TODO(b/240174245): Implement this using the 'set of extended events'.
  NOTIMPLEMENTED();

  // 1. For each event of worker’s set of extended events:
  // 1.1. If event is active, return false.
  // 2. Return true.
  return true;
}

void ServiceWorkerContext::ClearRegistration(
    ServiceWorkerRegistrationObject* registration) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerContext::ClearRegistration()");
  // Algorithm for Clear Registration:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#clear-registration-algorithm
  // 1. Run the following steps atomically.
  DCHECK_EQ(message_loop(), base::MessageLoop::current());

  // 2. If registration’s installing worker is not null, then:
  ServiceWorkerObject* installing_worker = registration->installing_worker();
  if (installing_worker) {
    // 2.1. Terminate registration’s installing worker.
    TerminateServiceWorker(installing_worker);
    // 2.2. Run the Update Worker State algorithm passing registration’s
    //      installing worker and "redundant" as the arguments.
    UpdateWorkerState(installing_worker, kServiceWorkerStateRedundant);
    // 2.3. Run the Update Registration State algorithm passing registration,
    //      "installing" and null as the arguments.
    UpdateRegistrationState(registration, kInstalling, nullptr);
  }

  // 3. If registration’s waiting worker is not null, then:
  ServiceWorkerObject* waiting_worker = registration->waiting_worker();
  if (waiting_worker) {
    // 3.1. Terminate registration’s waiting worker.
    TerminateServiceWorker(waiting_worker);
    // 3.2. Run the Update Worker State algorithm passing registration’s
    //      waiting worker and "redundant" as the arguments.
    UpdateWorkerState(waiting_worker, kServiceWorkerStateRedundant);
    // 3.3. Run the Update Registration State algorithm passing registration,
    //      "waiting" and null as the arguments.
    UpdateRegistrationState(registration, kWaiting, nullptr);
  }

  // 4. If registration’s active worker is not null, then:
  ServiceWorkerObject* active_worker = registration->active_worker();
  if (active_worker) {
    // 4.1. Terminate registration’s active worker.
    TerminateServiceWorker(active_worker);
    // 4.2. Run the Update Worker State algorithm passing registration’s
    //      active worker and "redundant" as the arguments.
    UpdateWorkerState(active_worker, kServiceWorkerStateRedundant);
    // 4.3. Run the Update Registration State algorithm passing registration,
    //      "active" and null as the arguments.
    UpdateRegistrationState(registration, kActive, nullptr);
  }

  // Persist registration since the waiting_worker and active_worker have
  // been updated to nullptr. This will remove any persisted registration
  // if one exists.
  scope_to_registration_map_->PersistRegistration(registration->storage_key(),
                                                  registration->scope_url());
}

void ServiceWorkerContext::TryClearRegistration(
    ServiceWorkerRegistrationObject* registration) {
  TRACE_EVENT0("cobalt::worker",
               "ServiceWorkerContext::TryClearRegistration()");
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  // Algorithm for Try Clear Registration:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#try-clear-registration-algorithm

  // 1. Invoke Clear Registration with registration if no service worker client
  // is using registration and all of the following conditions are true:
  if (IsAnyClientUsingRegistration(registration)) return;

  //    . registration’s installing worker is null or the result of running
  //      Service Worker Has No Pending Events with registration’s installing
  //      worker is true.
  if (registration->installing_worker() &&
      !ServiceWorkerHasNoPendingEvents(registration->installing_worker()))
    return;

  //    . registration’s waiting worker is null or the result of running
  //      Service Worker Has No Pending Events with registration’s waiting
  //      worker is true.
  if (registration->waiting_worker() &&
      !ServiceWorkerHasNoPendingEvents(registration->waiting_worker()))
    return;

  //    . registration’s active worker is null or the result of running
  //      ServiceWorker Has No Pending Events with registration’s active worker
  //      is true.
  if (registration->active_worker() &&
      !ServiceWorkerHasNoPendingEvents(registration->active_worker()))
    return;

  ClearRegistration(registration);
}

void ServiceWorkerContext::UpdateRegistrationState(
    ServiceWorkerRegistrationObject* registration, RegistrationState target,
    const scoped_refptr<ServiceWorkerObject>& source) {
  TRACE_EVENT2("cobalt::worker",
               "ServiceWorkerContext::UpdateRegistrationState()", "target",
               target, "source", source);
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  DCHECK(registration);
  // Algorithm for Update Registration State:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#update-registration-state-algorithm

  // 1. Let registrationObjects be an array containing all the
  //    ServiceWorkerRegistration objects associated with registration.
  // This is implemented with a call to LookupServiceWorkerRegistration for each
  // registered web context.

  switch (target) {
    // 2. If target is "installing", then:
    case kInstalling: {
      // 2.1. Set registration’s installing worker to source.
      registration->set_installing_worker(source);
      // 2.2. For each registrationObject in registrationObjects:
      for (auto& context : web_context_registrations_) {
        // 2.2.1. Queue a task to...
        context->message_loop()->task_runner()->PostBlockingTask(
            FROM_HERE,
            base::Bind(
                [](web::Context* context,
                   ServiceWorkerRegistrationObject* registration) {
                  // 2.2.1. ... set the installing attribute of
                  //        registrationObject to null if registration’s
                  //        installing worker is null, or the result of getting
                  //        the service worker object that represents
                  //        registration’s installing worker in
                  //        registrationObject’s relevant settings object.
                  auto registration_object =
                      context->LookupServiceWorkerRegistration(registration);
                  if (registration_object) {
                    registration_object->set_installing(
                        context->GetServiceWorker(
                            registration->installing_worker()));
                  }
                },
                context, base::Unretained(registration)));
      }
      break;
    }
    // 3. Else if target is "waiting", then:
    case kWaiting: {
      // 3.1. Set registration’s waiting worker to source.
      registration->set_waiting_worker(source);
      // 3.2. For each registrationObject in registrationObjects:
      for (auto& context : web_context_registrations_) {
        // 3.2.1. Queue a task to...
        context->message_loop()->task_runner()->PostBlockingTask(
            FROM_HERE,
            base::Bind(
                [](web::Context* context,
                   ServiceWorkerRegistrationObject* registration) {
                  // 3.2.1. ... set the waiting attribute of registrationObject
                  //        to null if registration’s waiting worker is null, or
                  //        the result of getting the service worker object that
                  //        represents registration’s waiting worker in
                  //        registrationObject’s relevant settings object.
                  auto registration_object =
                      context->LookupServiceWorkerRegistration(registration);
                  if (registration_object) {
                    registration_object->set_waiting(context->GetServiceWorker(
                        registration->waiting_worker()));
                  }
                },
                context, base::Unretained(registration)));
      }
      break;
    }
    // 4. Else if target is "active", then:
    case kActive: {
      // 4.1. Set registration’s active worker to source.
      registration->set_active_worker(source);
      // 4.2. For each registrationObject in registrationObjects:
      for (auto& context : web_context_registrations_) {
        // 4.2.1. Queue a task to...
        context->message_loop()->task_runner()->PostBlockingTask(
            FROM_HERE,
            base::Bind(
                [](web::Context* context,
                   ServiceWorkerRegistrationObject* registration) {
                  // 4.2.1. ... set the active attribute of registrationObject
                  //        to null if registration’s active worker is null, or
                  //        the result of getting the service worker object that
                  //        represents registration’s active worker in
                  //        registrationObject’s relevant settings object.
                  auto registration_object =
                      context->LookupServiceWorkerRegistration(registration);
                  if (registration_object) {
                    registration_object->set_active(context->GetServiceWorker(
                        registration->active_worker()));
                  }
                },
                context, base::Unretained(registration)));
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void ServiceWorkerContext::UpdateWorkerState(
    scoped_refptr<ServiceWorkerObject> worker, ServiceWorkerState state) {
  TRACE_EVENT1("cobalt::worker", "ServiceWorkerContext::UpdateWorkerState()",
               "state", state);
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  DCHECK(worker);
  if (!worker) {
    return;
  }
  // Algorithm for Update Worker State:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#update-state-algorithm
  // 1. Assert: state is not "parsed".
  DCHECK_NE(kServiceWorkerStateParsed, state);
  // 2. Set worker's state to state.
  worker->set_state(state);
  auto worker_origin = loader::Origin(worker->script_url());
  // 3. Let settingsObjects be all environment settings objects whose origin is
  //    worker's script url's origin.
  // 4. For each settingsObject of settingsObjects...
  for (auto& context : web_context_registrations_) {
    if (context->environment_settings()->GetOrigin() == worker_origin) {
      // 4. ... queue a task on
      //    settingsObject's responsible event loop in the DOM manipulation task
      //    source to run the following steps:
      context->message_loop()->task_runner()->PostBlockingTask(
          FROM_HERE, base::Bind(
                         [](web::Context* context, ServiceWorkerObject* worker,
                            ServiceWorkerState state) {
                           DCHECK_EQ(context->message_loop(),
                                     base::MessageLoop::current());
                           // 4.1. Let objectMap be settingsObject's service
                           // worker object
                           //      map.
                           // 4.2. If objectMap[worker] does not exist, then
                           // abort these
                           //      steps.
                           // 4.3. Let  workerObj be objectMap[worker].
                           auto worker_obj =
                               context->LookupServiceWorker(worker);
                           if (worker_obj) {
                             // 4.4. Set workerObj's state to state.
                             worker_obj->set_state(state);
                             // 4.5. Fire an event named statechange at
                             // workerObj.
                             worker_obj->DispatchEvent(
                                 new web::Event(base::Tokens::statechange()));
                           }
                         },
                         context, base::Unretained(worker.get()), state));
    }
  }
}

void ServiceWorkerContext::HandleServiceWorkerClientUnload(
    web::Context* client) {
  TRACE_EVENT0("cobalt::worker",
               "ServiceWorkerContext::HandleServiceWorkerClientUnload()");
  // Algorithm for Handle Servicer Worker Client Unload:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#on-client-unload-algorithm
  DCHECK(client);
  // 1. Run the following steps atomically.
  DCHECK_EQ(message_loop(), base::MessageLoop::current());

  // 2. Let registration be the service worker registration used by client.
  // 3. If registration is null, abort these steps.
  ServiceWorkerObject* active_service_worker = client->active_service_worker();
  if (!active_service_worker) return;
  ServiceWorkerRegistrationObject* registration =
      active_service_worker->containing_service_worker_registration();
  if (!registration) return;

  // 4. If any other service worker client is using registration, abort these
  //    steps.
  if (IsAnyClientUsingRegistration(registration)) return;

  // 5. If registration is unregistered, invoke Try Clear Registration with
  //    registration.
  if (scope_to_registration_map_ &&
      scope_to_registration_map_->IsUnregistered(registration)) {
    TryClearRegistration(registration);
  }

  // 6. Invoke Try Activate with registration.
  TryActivate(registration);
}

void ServiceWorkerContext::TerminateServiceWorker(ServiceWorkerObject* worker) {
  TRACE_EVENT0("cobalt::worker",
               "ServiceWorkerContext::TerminateServiceWorker()");
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  // Algorithm for Terminate Service Worker:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#terminate-service-worker
  // 1. Run the following steps in parallel with serviceWorker’s main loop:
  // This runs in the ServiceWorkerRegistry thread.
  DCHECK_EQ(message_loop(), base::MessageLoop::current());

  // 1.1. Let serviceWorkerGlobalScope be serviceWorker’s global object.
  WorkerGlobalScope* service_worker_global_scope =
      worker->worker_global_scope();

  // 1.2. Set serviceWorkerGlobalScope’s closing flag to true.
  if (service_worker_global_scope != nullptr)
    service_worker_global_scope->set_closing_flag(true);

  // 1.3. Remove all the items from serviceWorker’s set of extended events.
  // TODO(b/240174245): Implement 'set of extended events'.

  // 1.4. If there are any tasks, whose task source is either the handle fetch
  //      task source or the handle functional event task source, queued in
  //      serviceWorkerGlobalScope’s event loop’s task queues, queue them to
  //      serviceWorker’s containing service worker registration’s corresponding
  //      task queues in the same order using their original task sources, and
  //      discard all the tasks (including tasks whose task source is neither
  //      the handle fetch task source nor the handle functional event task
  //      source) from serviceWorkerGlobalScope’s event loop’s task queues
  //      without processing them.
  // TODO(b/234787641): Queue tasks to the registration.

  // Note: This step is not in the spec, but without this step the service
  // worker object map will always keep an entry with a service worker instance
  // for the terminated service worker, which besides leaking memory can lead to
  // unexpected behavior when new service worker objects are created with the
  // same key for the service worker object map (which in Cobalt's case
  // happens when a new service worker object is constructed at the same
  // memory address).
  for (auto& context : web_context_registrations_) {
    context->message_loop()->task_runner()->PostBlockingTask(
        FROM_HERE, base::Bind(
                       [](web::Context* context, ServiceWorkerObject* worker) {
                         auto worker_obj = context->LookupServiceWorker(worker);
                         if (worker_obj) {
                           worker_obj->set_state(kServiceWorkerStateRedundant);
                           worker_obj->DispatchEvent(
                               new web::Event(base::Tokens::statechange()));
                         }
                         context->RemoveServiceWorker(worker);
                       },
                       context, base::Unretained(worker)));
  }

  // 1.5. Abort the script currently running in serviceWorker.
  if (worker->is_running()) {
    worker->Abort();
  }

  // 1.6. Set serviceWorker’s start status to null.
  worker->set_start_status(nullptr);
}

void ServiceWorkerContext::MaybeResolveReadyPromiseSubSteps(
    web::Context* client) {
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  // Algorithm for Sub steps of ServiceWorkerContainer.ready():
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#navigator-service-worker-ready

  //    3.1. Let client by this's service worker client.
  //    3.2. Let storage key be the result of running obtain a storage
  //         key given client.
  url::Origin storage_key = client->environment_settings()->ObtainStorageKey();
  //    3.3. Let registration be the result of running Match Service
  //         Worker Registration given storage key and client’s
  //         creation URL.
  // TODO(b/234659851): Investigate whether this should use the creation URL
  // directly instead.
  const GURL& base_url = client->environment_settings()->creation_url();
  GURL client_url = base_url.Resolve("");
  scoped_refptr<ServiceWorkerRegistrationObject> registration =
      scope_to_registration_map_->MatchServiceWorkerRegistration(storage_key,
                                                                 client_url);
  //    3.3. If registration is not null, and registration’s active
  //         worker is not null, queue a task on readyPromise’s
  //         relevant settings object's responsible event loop, using
  //         the DOM manipulation task source, to resolve readyPromise
  //         with the result of getting the service worker
  //         registration object that represents registration in
  //         readyPromise’s relevant settings object.
  if (registration && registration->active_worker()) {
    client->message_loop()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceWorkerContainer::MaybeResolveReadyPromise,
                       base::Unretained(client->GetWindowOrWorkerGlobalScope()
                                            ->navigator_base()
                                            ->service_worker()
                                            .get()),
                       registration));
  }
}

void ServiceWorkerContext::GetRegistrationSubSteps(
    const url::Origin& storage_key, const GURL& client_url,
    web::Context* client,
    std::unique_ptr<script::ValuePromiseWrappable::Reference>
        promise_reference) {
  TRACE_EVENT0("cobalt::worker",
               "ServiceWorkerContext::GetRegistrationSubSteps()");
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  // Algorithm for Sub steps of ServiceWorkerContainer.getRegistration():
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#navigator-service-worker-getRegistration

  // 8.1. Let registration be the result of running Match Service Worker
  //      Registration algorithm with clientURL as its argument.
  scoped_refptr<ServiceWorkerRegistrationObject> registration =
      scope_to_registration_map_->MatchServiceWorkerRegistration(storage_key,
                                                                 client_url);
  // 8.2. If registration is null, resolve promise with undefined and abort
  //      these steps.
  // 8.3. Resolve promise with the result of getting the service worker
  //      registration object that represents registration in promise’s
  //      relevant settings object.
  client->message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](web::Context* client,
             std::unique_ptr<script::ValuePromiseWrappable::Reference> promise,
             scoped_refptr<ServiceWorkerRegistrationObject> registration) {
            TRACE_EVENT0(
                "cobalt::worker",
                "ServiceWorkerContext::GetRegistrationSubSteps() Resolve");
            promise->value().Resolve(
                client->GetServiceWorkerRegistration(registration));
          },
          client, std::move(promise_reference), registration));
}

void ServiceWorkerContext::GetRegistrationsSubSteps(
    const url::Origin& storage_key, web::Context* client,
    std::unique_ptr<script::ValuePromiseSequenceWrappable::Reference>
        promise_reference) {
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  std::vector<scoped_refptr<ServiceWorkerRegistrationObject>>
      registration_objects =
          scope_to_registration_map_->GetRegistrations(storage_key);
  client->message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](web::Context* client,
             std::unique_ptr<script::ValuePromiseSequenceWrappable::Reference>
                 promise,
             std::vector<scoped_refptr<ServiceWorkerRegistrationObject>>
                 registration_objects) {
            TRACE_EVENT0(
                "cobalt::worker",
                "ServiceWorkerContext::GetRegistrationSubSteps() Resolve");
            script::Sequence<scoped_refptr<script::Wrappable>> registrations;
            for (auto registration_object : registration_objects) {
              registrations.push_back(scoped_refptr<script::Wrappable>(
                  client->GetServiceWorkerRegistration(registration_object)
                      .get()));
            }
            promise->value().Resolve(std::move(registrations));
          },
          client, std::move(promise_reference),
          std::move(registration_objects)));
}

void ServiceWorkerContext::SkipWaitingSubSteps(
    web::Context* worker_context, ServiceWorkerObject* service_worker,
    std::unique_ptr<script::ValuePromiseVoid::Reference> promise_reference) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerContext::SkipWaitingSubSteps()");
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  // Check if the client web context is still active. This may trigger if
  // skipWaiting() was called and service worker installation fails.
  if (!IsWebContextRegistered(worker_context)) {
    promise_reference.release();
    return;
  }

  // Algorithm for Sub steps of ServiceWorkerGlobalScope.skipWaiting():
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dom-serviceworkerglobalscope-skipwaiting

  // 2.1. Set service worker's skip waiting flag.
  service_worker->set_skip_waiting();

  // 2.2. Invoke Try Activate with service worker's containing service worker
  // registration.
  TryActivate(service_worker->containing_service_worker_registration());

  // 2.3. Resolve promise with undefined.
  worker_context->message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<script::ValuePromiseVoid::Reference> promise) {
            promise->value().Resolve();
          },
          std::move(promise_reference)));
}

void ServiceWorkerContext::WaitUntilSubSteps(
    ServiceWorkerRegistrationObject* registration) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerContext::WaitUntilSubSteps()");
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  // Sub steps for WaitUntil.
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dom-extendableevent-waituntil
  // 5.2.2. If registration is unregistered, invoke Try Clear Registration
  //        with registration.
  if (scope_to_registration_map_->IsUnregistered(registration)) {
    TryClearRegistration(registration);
  }
  // 5.2.3. If registration is not null, invoke Try Activate with
  //        registration.
  if (registration) {
    TryActivate(registration);
  }
}

void ServiceWorkerContext::ClientsGetSubSteps(
    web::Context* worker_context,
    ServiceWorkerObject* associated_service_worker,
    std::unique_ptr<script::ValuePromiseWrappable::Reference> promise_reference,
    const std::string& id) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerContext::ClientsGetSubSteps()");
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  // Check if the client web context is still active. This may trigger if
  // Clients.get() was called and service worker installation fails.
  if (!IsWebContextRegistered(worker_context)) {
    promise_reference.release();
    return;
  }
  // Parallel sub steps (2) for algorithm for Clients.get(id):
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#clients-get
  // 2.1. For each service worker client client where the result of running
  //      obtain a storage key given client equals the associated service
  //      worker's containing service worker registration's storage key:
  const url::Origin& storage_key =
      associated_service_worker->containing_service_worker_registration()
          ->storage_key();
  for (auto& client : web_context_registrations_) {
    url::Origin client_storage_key =
        client->environment_settings()->ObtainStorageKey();
    if (client_storage_key.IsSameOriginWith(storage_key)) {
      // 2.1.1. If client’s id is not id, continue.
      if (client->environment_settings()->id() != id) continue;

      // 2.1.2. Wait for either client’s execution ready flag to be set or for
      //        client’s discarded flag to be set.
      // Web Contexts exist only in the web_context_registrations_ set when they
      // are both execution ready and not discarded.

      // 2.1.3. If client’s execution ready flag is set, then invoke Resolve Get
      //        Client Promise with client and promise, and abort these steps.
      ResolveGetClientPromise(client, worker_context,
                              std::move(promise_reference));
      return;
    }
  }
  // 2.2. Resolve promise with undefined.
  worker_context->message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<script::ValuePromiseWrappable::Reference>
                 promise_reference) {
            TRACE_EVENT0("cobalt::worker",
                         "ServiceWorkerContext::ClientsGetSubSteps() Resolve");
            promise_reference->value().Resolve(scoped_refptr<Client>());
          },
          std::move(promise_reference)));
}

void ServiceWorkerContext::ClientsMatchAllSubSteps(
    web::Context* worker_context,
    ServiceWorkerObject* associated_service_worker,
    std::unique_ptr<script::ValuePromiseSequenceWrappable::Reference>
        promise_reference,
    bool include_uncontrolled, ClientType type) {
  TRACE_EVENT0("cobalt::worker",
               "ServiceWorkerContext::ClientsMatchAllSubSteps()");
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  // Check if the worker web context is still active. This may trigger if
  // Clients.matchAll() was called and service worker installation fails.
  if (!IsWebContextRegistered(worker_context)) {
    promise_reference.release();
    return;
  }

  // Parallel sub steps (2) for algorithm for Clients.matchAll():
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#clients-matchall
  // 2.1. Let targetClients be a new list.
  std::list<web::Context*> target_clients;

  // 2.2. For each service worker client client where the result of running
  //      obtain a storage key given client equals the associated service
  //      worker's containing service worker registration's storage key:
  const url::Origin& storage_key =
      associated_service_worker->containing_service_worker_registration()
          ->storage_key();
  for (auto& client : web_context_registrations_) {
    url::Origin client_storage_key =
        client->environment_settings()->ObtainStorageKey();
    if (client_storage_key.IsSameOriginWith(storage_key)) {
      // 2.2.1. If client’s execution ready flag is unset or client’s discarded
      //        flag is set, continue.
      // Web Contexts exist only in the web_context_registrations_ set when they
      // are both execution ready and not discarded.

      // 2.2.2. If client is not a secure context, continue.
      // In production, Cobalt requires https, therefore all workers and their
      // owners are secure contexts.

      // 2.2.3. If options["includeUncontrolled"] is false, and if client’s
      //        active service worker is not the associated service worker,
      //        continue.
      if (!include_uncontrolled &&
          (client->active_service_worker() != associated_service_worker)) {
        continue;
      }

      // 2.2.4. Add client to targetClients.
      target_clients.push_back(client);
    }
  }

  // 2.3. Let matchedWindowData be a new list.
  std::unique_ptr<std::vector<WindowData>> matched_window_data(
      new std::vector<WindowData>);

  // 2.4. Let matchedClients be a new list.
  std::unique_ptr<std::vector<web::Context*>> matched_clients(
      new std::vector<web::Context*>);

  // 2.5. For each service worker client client in targetClients:
  for (auto* client : target_clients) {
    auto* global_scope = client->GetWindowOrWorkerGlobalScope();

    if ((type == kClientTypeWindow || type == kClientTypeAll) &&
        (global_scope->IsWindow())) {
      // 2.5.1. If options["type"] is "window" or "all", and client is not an
      //        environment settings object or is a window client, then:

      // 2.5.1.1. Let windowData be [ "client" -> client, "ancestorOriginsList"
      //          -> a new list ].
      WindowData window_data(client->environment_settings());

      // 2.5.1.2. Let browsingContext be null.

      // 2.5.1.3. Let isClientEnumerable be true.
      // For Cobalt, isClientEnumerable is always true because the clauses that
      // would set it to false in 2.5.1.6. do not apply to Cobalt.

      // 2.5.1.4. If client is an environment settings object, set
      //          browsingContext to client’s global object's browsing context.
      // 2.5.1.5. Else, set browsingContext to client’s target browsing context.
      web::Context* browsing_context = client;

      // 2.5.1.6. Queue a task task to run the following substeps on
      //          browsingContext’s event loop using the user interaction task
      //          source:
      // Note: The task below does not currently perform any actual
      // functionality. It is included however to help future implementation for
      // fetching values for WindowClient properties, with similar logic
      // existing in ResolveGetClientPromise.
      browsing_context->message_loop()->task_runner()->PostBlockingTask(
          FROM_HERE, base::Bind(
                         [](WindowData* window_data) {
                           // 2.5.1.6.1. If browsingContext has been discarded,
                           //            then set isClientEnumerable to false
                           //            and abort these steps.
                           // 2.5.1.6.2. If client is a window client and
                           //            client’s responsible document is not
                           //            browsingContext’s active document, then
                           //            set isClientEnumerable to false and
                           //            abort these steps.
                           // In Cobalt, the document of a window browsing
                           // context doesn't change: When a new document is
                           // created, a new browsing context is created with
                           // it.

                           // 2.5.1.6.3. Set windowData["frameType"] to the
                           //            result of running Get Frame Type with
                           //            browsingContext.
                           // Cobalt does not support nested or auxiliary
                           // browsing contexts.
                           // 2.5.1.6.4. Set windowData["visibilityState"] to
                           //            browsingContext’s active document's
                           //            visibilityState attribute value.
                           // 2.5.1.6.5. Set windowData["focusState"] to the
                           //            result of running the has focus steps
                           //            with browsingContext’s active document
                           //            as the argument.

                           // 2.5.1.6.6. If client is a window client, then set
                           //            windowData["ancestorOriginsList"] to
                           //            browsingContext’s active document's
                           //            relevant global object's Location
                           //            object’s ancestor origins list's
                           //            associated list.
                           // Cobalt does not implement
                           // Location.ancestorOrigins.
                         },
                         &window_data));

      // 2.5.1.7. Wait for task to have executed.
      // The task above is posted as a blocking task.

      // 2.5.1.8. If isClientEnumerable is true, then:

      // 2.5.1.8.1. Add windowData to matchedWindowData.
      matched_window_data->emplace_back(window_data);

      // 2.5.2. Else if options["type"] is "worker" or "all" and client is a
      //        dedicated worker client, or options["type"] is "sharedworker" or
      //        "all" and client is a shared worker client, then:
    } else if (((type == kClientTypeWorker || type == kClientTypeAll) &&
                global_scope->IsDedicatedWorker())) {
      // Note: Cobalt does not support shared workers.
      // 2.5.2.1. Add client to matchedClients.
      matched_clients->emplace_back(client);
    }
  }

  // 2.6. Queue a task to run the following steps on promise’s relevant
  // settings object's responsible event loop using the DOM manipulation
  // task source:
  worker_context->message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<script::ValuePromiseSequenceWrappable::Reference>
                 promise_reference,
             std::unique_ptr<std::vector<WindowData>> matched_window_data,
             std::unique_ptr<std::vector<web::Context*>> matched_clients) {
            TRACE_EVENT0("cobalt::worker",
                         "ServiceWorkerContext::ClientsMatchAllSubSteps() "
                         "Resolve Promise");
            // 2.6.1. Let clientObjects be a new list.
            script::Sequence<scoped_refptr<script::Wrappable>> client_objects;

            // 2.6.2. For each windowData in matchedWindowData:
            for (auto& window_data : *matched_window_data) {
              // 2.6.2.1. Let WindowClient be the result of running
              //          Create Window Client algorithm with
              //          windowData["client"],
              //          windowData["frameType"],
              //          windowData["visibilityState"],
              //          windowData["focusState"], and
              //          windowData["ancestorOriginsList"] as the
              //          arguments.
              // TODO(b/235838698): Implement WindowClient methods.
              scoped_refptr<Client> window_client =
                  WindowClient::Create(window_data);

              // 2.6.2.2. Append WindowClient to clientObjects.
              client_objects.push_back(window_client);
            }

            // 2.6.3. For each client in matchedClients:
            for (auto& client : *matched_clients) {
              // 2.6.3.1. Let clientObject be the result of running
              //          Create Client algorithm with client as the
              //          argument.
              scoped_refptr<Client> client_object =
                  Client::Create(client->environment_settings());

              // 2.6.3.2. Append clientObject to clientObjects.
              client_objects.push_back(client_object);
            }
            // 2.6.4. Sort clientObjects such that:
            //        . WindowClient objects whose browsing context has been
            //          focused are placed first, sorted in the most recently
            //          focused order.
            //        . WindowClient objects whose browsing context has never
            //          been focused are placed next, sorted in their service
            //          worker client's creation order.
            //        . Client objects whose associated service worker client is
            //          a worker client are placed next, sorted in their service
            //          worker client's creation order.
            // TODO(b/235876598): Implement sorting of clientObjects.

            // 2.6.5. Resolve promise with a new frozen array of clientObjects
            //        in promise’s relevant Realm.
            promise_reference->value().Resolve(client_objects);
          },
          std::move(promise_reference), std::move(matched_window_data),
          std::move(matched_clients)));
}

void ServiceWorkerContext::ClaimSubSteps(
    web::Context* worker_context,
    ServiceWorkerObject* associated_service_worker,
    std::unique_ptr<script::ValuePromiseVoid::Reference> promise_reference) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerContext::ClaimSubSteps()");
  DCHECK_EQ(message_loop(), base::MessageLoop::current());

  // Check if the client web context is still active. This may trigger if
  // Clients.claim() was called and service worker installation fails.
  if (!IsWebContextRegistered(worker_context)) {
    promise_reference.release();
    return;
  }

  // Parallel sub steps (3) for algorithm for Clients.claim():
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dom-clients-claim
  std::list<web::Context*> target_clients;

  // 3.1. For each service worker client client where the result of running
  //      obtain a storage key given client equals the service worker's
  //      containing service worker registration's storage key:
  const url::Origin& storage_key =
      associated_service_worker->containing_service_worker_registration()
          ->storage_key();
  for (auto& client : web_context_registrations_) {
    // Don't claim to be our own service worker.
    if (client == worker_context) continue;
    url::Origin client_storage_key =
        client->environment_settings()->ObtainStorageKey();
    if (client_storage_key.IsSameOriginWith(storage_key)) {
      // 3.1.1. If client’s execution ready flag is unset or client’s discarded
      //        flag is set, continue.
      // Web Contexts exist only in the web_context_registrations_ set when they
      // are both execution ready and not discarded.

      // 3.1.2. If client is not a secure context, continue.
      // In production, Cobalt requires https, therefore all clients are secure
      // contexts.

      // 3.1.3. Let storage key be the result of running obtain a storage key
      //        given client.
      // 3.1.4. Let registration be the result of running Match Service Worker
      //        Registration given storage key and client’s creation URL.
      // TODO(b/234659851): Investigate whether this should use the creation
      // URL directly instead.
      const GURL& base_url = client->environment_settings()->creation_url();
      GURL client_url = base_url.Resolve("");
      scoped_refptr<ServiceWorkerRegistrationObject> registration =
          scope_to_registration_map_->MatchServiceWorkerRegistration(
              client_storage_key, client_url);

      // 3.1.5. If registration is not the service worker's containing service
      //        worker registration, continue.
      if (registration !=
          associated_service_worker->containing_service_worker_registration()) {
        continue;
      }

      // 3.1.6. If client’s active service worker is not the service worker,
      //        then:
      if (client->active_service_worker() != associated_service_worker) {
        // 3.1.6.1. Invoke Handle Service Worker Client Unload with client as
        //          the argument.
        HandleServiceWorkerClientUnload(client);

        // 3.1.6.2. Set client’s active service worker to service worker.
        client->set_active_service_worker(associated_service_worker);

        // 3.1.6.3. Invoke Notify Controller Change algorithm with client as the
        //          argument.
        NotifyControllerChange(client);
      }
    }
  }
  // 3.2. Resolve promise with undefined.
  worker_context->message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<script::ValuePromiseVoid::Reference> promise) {
            promise->value().Resolve();
          },
          std::move(promise_reference)));
}

void ServiceWorkerContext::ServiceWorkerPostMessageSubSteps(
    ServiceWorkerObject* service_worker, web::Context* incumbent_client,
    std::unique_ptr<script::StructuredClone> structured_clone) {
  // Parallel sub steps (6) for algorithm for ServiceWorker.postMessage():
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#service-worker-postmessage-options
  // 3. Let incumbentGlobal be incumbentSettings’s global object.
  // Note: The 'incumbent' is the sender of the message.
  // 6.1 If the result of running the Run Service Worker algorithm with
  //     serviceWorker is failure, then return.
  auto* run_result = RunServiceWorker(service_worker);
  if (!run_result) return;
  if (!structured_clone || structured_clone->failed()) return;

  // 6.2 Queue a task on the DOM manipulation task source to run the following
  //     steps:
  incumbent_client->message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](ServiceWorkerObject* service_worker,
             web::Context* incumbent_client,
             std::unique_ptr<script::StructuredClone> structured_clone) {
            web::EventTarget* event_target =
                service_worker->worker_global_scope();
            if (!event_target) return;

            web::WindowOrWorkerGlobalScope* incumbent_global =
                incumbent_client->GetWindowOrWorkerGlobalScope();
            DCHECK_EQ(incumbent_client->environment_settings(),
                      incumbent_global->environment_settings());
            base::TypeId incumbent_type = incumbent_global->GetWrappableType();
            ServiceWorkerObject* incumbent_worker =
                incumbent_global->IsServiceWorker()
                    ? incumbent_global->AsServiceWorker()
                          ->service_worker_object()
                    : nullptr;
            base::MessageLoop* message_loop =
                event_target->environment_settings()->context()->message_loop();
            if (!message_loop) {
              return;
            }
            message_loop->task_runner()->PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](const base::TypeId& incumbent_type,
                       ServiceWorkerObject* incumbent_worker,
                       web::Context* incumbent_client,
                       web::EventTarget* event_target,
                       std::unique_ptr<script::StructuredClone>
                           structured_clone) {
                      ExtendableMessageEventInit init_dict;
                      if (incumbent_type ==
                          base::GetTypeId<ServiceWorkerGlobalScope>()) {
                        // 6.2.1. Let source be determined by switching on the
                        //        type of incumbentGlobal:
                        //        . ServiceWorkerGlobalScope
                        //          The result of getting the service worker
                        //          object that represents incumbentGlobal’s
                        //          service worker in the relevant settings
                        //          object of serviceWorker’s global object.
                        init_dict.set_source(ExtendableMessageEvent::SourceType(
                            event_target->environment_settings()
                                ->context()
                                ->GetServiceWorker(incumbent_worker)));
                      } else if (incumbent_type ==
                                 base::GetTypeId<dom::Window>()) {
                        //        . Window
                        //          a new WindowClient object that represents
                        //          incumbentGlobal’s relevant settings object.
                        init_dict.set_source(ExtendableMessageEvent::SourceType(
                            WindowClient::Create(WindowData(
                                incumbent_client->environment_settings()))));
                      } else {
                        //        . Otherwise
                        //          a new Client object that represents
                        //          incumbentGlobal’s associated worker
                        init_dict.set_source(
                            ExtendableMessageEvent::SourceType(Client::Create(
                                incumbent_client->environment_settings())));
                      }

                      event_target->DispatchEvent(
                          new worker::ExtendableMessageEvent(
                              event_target->environment_settings(),
                              base::Tokens::message(), init_dict,
                              std::move(structured_clone)));
                    },
                    incumbent_type, base::Unretained(incumbent_worker),
                    // Note: These should probably be weak pointers for when
                    // the message sender disappears before the recipient
                    // processes the event, but since base::WeakPtr
                    // dereferencing isn't thread-safe, that can't actually be
                    // used here.
                    base::Unretained(incumbent_client),
                    base::Unretained(event_target),
                    std::move(structured_clone)));
          },
          base::Unretained(service_worker), base::Unretained(incumbent_client),
          std::move(structured_clone)));
}

void ServiceWorkerContext::RegisterWebContext(web::Context* context) {
  DCHECK_NE(nullptr, context);
  web_context_registrations_cleared_.Reset();
  if (base::MessageLoop::current() != message_loop()) {
    DCHECK(message_loop());
    message_loop()->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&ServiceWorkerContext::RegisterWebContext,
                                  base::Unretained(this), context));
    return;
  }
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  DCHECK_EQ(0, web_context_registrations_.count(context));
  web_context_registrations_.insert(context);
}

void ServiceWorkerContext::SetActiveWorker(web::EnvironmentSettings* client) {
  if (!client) return;
  if (base::MessageLoop::current() != message_loop()) {
    DCHECK(message_loop());
    message_loop()->task_runner()->PostTask(
        FROM_HERE, base::Bind(&ServiceWorkerContext::SetActiveWorker,
                              base::Unretained(this), client));
    return;
  }
  DCHECK(scope_to_registration_map_);
  scoped_refptr<ServiceWorkerRegistrationObject> client_registration =
      scope_to_registration_map_->MatchServiceWorkerRegistration(
          client->ObtainStorageKey(), client->creation_url());
  if (client_registration.get() && client_registration->active_worker()) {
    client->context()->set_active_service_worker(
        client_registration->active_worker());
  } else {
    client->context()->set_active_service_worker(nullptr);
  }
}

void ServiceWorkerContext::UnregisterWebContext(web::Context* context) {
  DCHECK_NE(nullptr, context);
  if (base::MessageLoop::current() != message_loop()) {
    // Block to ensure that the context is unregistered before it is destroyed.
    DCHECK(message_loop());
    message_loop()->task_runner()->PostBlockingTask(
        FROM_HERE, base::Bind(&ServiceWorkerContext::UnregisterWebContext,
                              base::Unretained(this), context));
    return;
  }
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  DCHECK_EQ(1, web_context_registrations_.count(context));
  web_context_registrations_.erase(context);
  HandleServiceWorkerClientUnload(context);
  PrepareForClientShutdown(context);
  if (web_context_registrations_.empty()) {
    web_context_registrations_cleared_.Signal();
  }
}

void ServiceWorkerContext::PrepareForClientShutdown(web::Context* client) {
  DCHECK(client);
  if (!client) return;
  DCHECK(base::MessageLoop::current() == message_loop());
  // Note: This could be rewritten to use the decomposition declaration
  // 'const auto& [scope, queue]' after switching to C++17.
  jobs_->PrepareForClientShutdown(client);
}

}  // namespace worker
}  // namespace cobalt
