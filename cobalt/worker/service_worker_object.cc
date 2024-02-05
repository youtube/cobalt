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

#include "cobalt/worker/service_worker_object.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/execution_state.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/script_runner.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/web/agent.h"
#include "cobalt/web/context.h"
#include "cobalt/worker/service_worker_global_scope.h"
#include "cobalt/worker/service_worker_state.h"
#include "cobalt/worker/worker_global_scope.h"
#include "cobalt/worker/worker_settings.h"
#include "url/gurl.h"

namespace cobalt {
namespace worker {

ServiceWorkerObject::ServiceWorkerObject(const Options& options)
    : state_(kServiceWorkerStateParsed), options_(options) {
  DCHECK(options.containing_service_worker_registration);
}

ServiceWorkerObject::~ServiceWorkerObject() {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerObject::~ServiceWorkerObject()");
  Abort();
}

void ServiceWorkerObject::Abort() {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerObject::Abort()");
  if (web_agent_) {
    DCHECK(message_loop());
    DCHECK(web_context_);
    std::unique_ptr<web::Agent> web_agent(std::move(web_agent_));
    DCHECK(web_agent);
    DCHECK(!web_agent_);
    web_agent->WaitUntilDone();
    web_context_ = nullptr;
    web_agent->Stop();
    web_agent.reset();
  }
}

void ServiceWorkerObject::SetScriptResource(const GURL& url,
                                            std::string* resource) {
  // The exact given resource may already be in the map, if that is the case,
  // don't update the map at all, otherwise make a copy of the resource for
  // storing in the map.
  auto entry = script_resource_map_.find(url);
  if (entry != script_resource_map_.end()) {
    if (entry->second.content.get() != resource) {
      // The map has an entry, but it's different than the given one, make a
      // copy and replace.
      entry->second.content.reset(new std::string(*resource));
    }
    return;
  }

  auto result = script_resource_map_.emplace(std::make_pair(
      url,
      ScriptResource(std::make_unique<std::string>(std::string(*resource)))));
  // Assert that the insert was successful.
  DCHECK(result.second);
}

bool ServiceWorkerObject::HasScriptResource() const {
  return script_url_.is_valid() &&
         script_resource_map_.end() != script_resource_map_.find(script_url_);
}

const ScriptResource* ServiceWorkerObject::LookupScriptResource(
    const GURL& url) const {
  auto entry = script_resource_map_.find(url);
  return entry != script_resource_map_.end() ? &entry->second : nullptr;
}

void ServiceWorkerObject::SetScriptResourceHasEverBeenEvaluated(
    const GURL& url) {
  auto entry = script_resource_map_.find(url);
  if (entry != script_resource_map_.end()) {
    entry->second.has_ever_been_evaluated = true;
  }
}

void ServiceWorkerObject::PurgeScriptResourceMap() {
  // Steps 13-15 of Algorithm for Install:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#installation-algorithm
  // 13. Let map be registration’s installing worker's script resource map.
  // 14. Let usedSet be registration’s installing worker's set of used scripts.
  // 15. For each url of map:
  for (auto item = script_resource_map_.begin(), next_item = item;
       item != script_resource_map_.end(); item = next_item) {
    // Get next item here because erasing 'item' from the map will invalidate
    // the iterator.
    ++next_item;
    // 15.1. If usedSet does not contain url, then remove map[url].
    if (set_of_used_scripts_.find(item->first) == set_of_used_scripts_.end()) {
      script_resource_map_.erase(item);
    }
  }
}

void ServiceWorkerObject::WillDestroyCurrentMessageLoop() {
  TRACE_EVENT0("cobalt::worker",
               "ServiceWorkerObject::WillDestroyCurrentMessageLoop()");
// Destroy members that were constructed in the worker thread.
#if defined(ENABLE_DEBUGGER)
  debug_module_.reset();
#endif  // ENABLE_DEBUGGER
  worker_global_scope_ = nullptr;
}

void ServiceWorkerObject::ObtainWebAgentAndWaitUntilDone() {
  TRACE_EVENT0("cobalt::worker",
               "ServiceWorkerObject::ObtainWebAgentAndWaitUntilDone()");
  web_agent_.reset(new web::Agent(options_.name));
  web_agent_->Run(
      options_.web_options,
      base::Bind(&ServiceWorkerObject::Initialize, base::Unretained(this)),
      this);
  web_agent_->WaitUntilDone();
}

bool ServiceWorkerObject::ShouldSkipEvent(base_token::Token event_name) {
  // Algorithm for Should Skip Event:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#should-skip-event-algorithm
  // 1. If serviceWorker’s set of event types to handle does not contain
  // eventName, then the user agent may return true.
  return (set_of_event_types_to_handle_.find(event_name) ==
          set_of_event_types_to_handle_.end());
}

void ServiceWorkerObject::Initialize(web::Context* context) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerObject::Initialize()");
  // Algorithm for "Run Service Worker"
  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#run-service-worker-algorithm

  // 8.1. Let realmExecutionContext be the result of creating a new JavaScript
  //      realm given agent and the following customizations:
  //        For the global object, create a new ServiceWorkerGlobalScope object.
  //        Let workerGlobalScope be the created object.
  web_context_ = context;
  // 8.2. Set serviceWorker’s global object to workerGlobalScope.
  // 8.3. Let settingsObject be a new environment settings object whose
  //      algorithms are defined as follows:
  //      The realm execution context
  //        Return realmExecutionContext.
  //      The module map
  //        Return workerGlobalScope’s module map.
  //      The API URL character encoding
  //        Return UTF-8.
  //      The API base URL
  //        Return serviceWorker’s script url.
  //      The origin
  //        Return its registering service worker client's origin.
  WorkerSettings* worker_settings = new WorkerSettings();
  worker_settings->set_origin(
      loader::Origin(containing_service_worker_registration()->scope_url()));
  //      The policy container
  //        Return workerGlobalScope’s policy container.
  //      The time origin
  //        Return the result of coarsening unsafeCreationTime given
  //        workerGlobalScope’s cross-origin isolated capability.
  // 8.4. Set settingsObject’s id to a new unique opaque string, creation URL to
  //      serviceWorker’s script url, top-level creation URL to null, top-level
  //      origin to an implementation-defined value, target browsing context to
  //      null, and active service worker to null.

  web_context_->SetupEnvironmentSettings(worker_settings);
  web_context_->environment_settings()->set_creation_url(script_url_);
  scoped_refptr<ServiceWorkerGlobalScope> service_worker_global_scope =
      new ServiceWorkerGlobalScope(web_context_->environment_settings(),
                                   options_.global_scope_options, this);
  worker_global_scope_ = service_worker_global_scope;
  web_context_->global_environment()->CreateGlobalObject(
      service_worker_global_scope, web_context_->environment_settings());
  DCHECK(!web_context_->GetWindowOrWorkerGlobalScope()->IsWindow());
  DCHECK(!web_context_->GetWindowOrWorkerGlobalScope()->IsDedicatedWorker());
  DCHECK(web_context_->GetWindowOrWorkerGlobalScope()->IsServiceWorker());
  DCHECK(web_context_->GetWindowOrWorkerGlobalScope()->GetWrappableType() ==
         base::GetTypeId<ServiceWorkerGlobalScope>());
  DCHECK_EQ(service_worker_global_scope,
            base::polymorphic_downcast<ServiceWorkerGlobalScope*>(
                web_context_->GetWindowOrWorkerGlobalScope()));

#if defined(ENABLE_DEBUGGER)
  debug_module_.reset(new debug::backend::DebugModule(
      nullptr /* debugger_hooks */, web_context_->global_environment(),
      nullptr /* render_overlay */, nullptr /* resource_provider */,
      nullptr /* window */, nullptr /* debugger_state */));
#endif  // ENABLE_DEBUGGER

  // 8.5. Set workerGlobalScope’s url to serviceWorker’s script url.
  worker_global_scope_->set_url(script_url_);
  // 8.6. Set workerGlobalScope’s policy container to serviceWorker’s script
  //      resource’s policy container.
  // 8.7. Set workerGlobalScope’s type to serviceWorker’s type.
  // 8.8. Set workerGlobalScope’s force bypass cache for import scripts flag if
  //      forceBypassCache is true.
  // 8.9. Create a new WorkerLocation object and associate it with
  //      workerGlobalScope.
  // 8.10. If the run CSP initialization for a global object algorithm returns
  //       "Blocked" when executed upon workerGlobalScope, set startFailed to
  //       true and abort these steps.
  const ScriptResource* script_resource = LookupScriptResource(script_url_);
  DCHECK(script_resource);
  csp::ResponseHeaders csp_headers(script_resource->headers);
  DCHECK(service_worker_global_scope);
  DCHECK(service_worker_global_scope->csp_delegate());
  if (!service_worker_global_scope->csp_delegate()->OnReceiveHeaders(
          csp_headers)) {
    // https://www.w3.org/TR/service-workers/#content-security-policy
    LOG(WARNING) << "Warning: No Content Security Header received for the "
                    "service worker.";
  }
  web_context_->SetupFinished();
  // 8.11. If serviceWorker is an active worker, and there are any tasks queued
  //       in serviceWorker’s containing service worker registration’s task
  //       queues, queue them to serviceWorker’s event loop’s task queues in the
  //       same order using their original task sources.
  // TODO(b/234787641): Queue tasks from the registration.
  // 8.12. Let evaluationStatus be null.
  // 8.13. If script is a classic script, then:
  // 8.13.1. Set evaluationStatus to the result of running the classic script
  //         script.

  bool mute_errors = false;
  bool succeeded = false;
  DCHECK(script_resource->content.get());
  base::SourceLocation script_location(script_url().spec(), 1, 1);
  std::string retval = web_context_->script_runner()->Execute(
      *script_resource->content.get(), script_location, mute_errors,
      &succeeded);
  // 8.13.2. If evaluationStatus.[[Value]] is empty, this means the script was
  //         not evaluated. Set startFailed to true and abort these steps.
  // We don't actually have access to an 'evaluationStatus' from ScriptRunner,
  // so here we have to use the returned 'succeeded' boolean as a proxy for this
  // step.
  if (!succeeded) {
    store_start_failed(true);
    return;
  }
  // 8.14. Otherwise, if script is a module script, then:
  // 8.14.1. Let evaluationPromise be the result of running the module script
  //         script, with report errors set to false.
  // 8.14.2. Assert: evaluationPromise.[[PromiseState]] is not "pending".
  // 8.14.3. If evaluationPromise.[[PromiseState]] is "rejected":
  // 8.14.3.1. Set evaluationStatus to
  //           ThrowCompletion(evaluationPromise.[[PromiseResult]]).
  // 8.14.4. Otherwise:
  // 8.14.4.1. Set evaluationStatus to NormalCompletion(undefined).
  // 8.15. If the script was aborted by the Terminate Service Worker algorithm,
  //       set startFailed to true and abort these steps.
  // 8.16. Set serviceWorker’s start status to evaluationStatus.
  start_status_.reset(new std::string(retval));
  // 8.17. If script’s has ever been evaluated flag is unset, then:
  if (!script_resource->has_ever_been_evaluated) {
    // 8.17.1. For each eventType of settingsObject’s global object's associated
    //         list of event listeners' event types:
    auto event_types =
        service_worker_global_scope->event_listener_event_types();
    for (auto& event_type : event_types) {
      // 8.17.1.1. Append eventType to workerGlobalScope’s associated service
      //           worker's set of event types to handle.
      service_worker_global_scope->service_worker_object()
          ->set_of_event_types_to_handle()
          .insert(event_type);
    }
    // 8.17.2. Set script’s has ever been evaluated flag.
    SetScriptResourceHasEverBeenEvaluated(script_url_);

    if (event_types.empty()) {
      // NOTE: If the global object’s associated list of event listeners does
      // not have any event listener added at this moment, the service worker’s
      // set of event types to handle remains an empty set. The user agents are
      // encouraged to show a warning that the event listeners must be added on
      // the very first evaluation of the worker script.
      LOG(WARNING) << "ServiceWorkerGlobalScope's event listeners must be "
                      "added on the first evaluation of the worker script.";
    }
    event_types.clear();
  }
  // 8.18. Run the responsible event loop specified by settingsObject until it
  //       is destroyed.
  // 8.19. Empty workerGlobalScope’s list of active timers.
}

}  // namespace worker
}  // namespace cobalt
