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
#include "cobalt/script/wrappable.h"
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
    : state_(kServiceWorkerStateParsed), options_(options) {}

ServiceWorkerObject::~ServiceWorkerObject() {
  if (web_agent_) {
    DCHECK(message_loop());
    web_agent_->WaitUntilDone();
    web_agent_.reset();
  }
}

std::string* ServiceWorkerObject::LookupScriptResource() const {
  return LookupScriptResource(script_url_);
}

std::string* ServiceWorkerObject::LookupScriptResource(const GURL& url) const {
  auto entry = script_resource_map_.find(url);
  return entry != script_resource_map_.end() ? entry->second.get() : nullptr;
}

void ServiceWorkerObject::WillDestroyCurrentMessageLoop() {
// Destroy members that were constructed in the worker thread.
#if defined(ENABLE_DEBUGGER)
  debug_module_.reset();
#endif  // ENABLE_DEBUGGER

  worker_global_scope_ = nullptr;
}

void ServiceWorkerObject::ObtainWebAgentAndWaitUntilDone() {
  web_agent_.reset(new web::Agent(
      options_.web_options,
      base::Bind(&ServiceWorkerObject::Initialize, base::Unretained(this)),
      this));
  web_agent_->WaitUntilDone();
}

void ServiceWorkerObject::Initialize(web::Context* context) {
  // Algorithm for "Run Service Worker"
  // https://w3c.github.io/ServiceWorker/#run-service-worker-algorithm

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
  //      The policy container
  //        Return workerGlobalScope’s policy container.
  //      The time origin
  //        Return the result of coarsening unsafeCreationTime given
  //        workerGlobalScope’s cross-origin isolated capability.
  // 8.4. Set settingsObject’s id to a new unique opaque string, creation URL to
  //      serviceWorker’s script url, top-level creation URL to null, top-level
  //      origin to an implementation-defined value, target browsing context to
  //      null, and active service worker to null.
  web_context_->setup_environment_settings(new WorkerSettings(script_url_));
  scoped_refptr<ServiceWorkerGlobalScope> service_worker_global_scope =
      new ServiceWorkerGlobalScope(web_context_->environment_settings());
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
  worker_global_scope_->set_url(
      web_context_->environment_settings()->base_url());
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
  // 8.11. If serviceWorker is an active worker, and there are any tasks queued
  //       in serviceWorker’s containing service worker registration’s task
  //       queues, queue them to serviceWorker’s event loop’s task queues in the
  //       same order using their original task sources.
  // 8.12. Let evaluationStatus be null.
  // 8.13. If script is a classic script, then:
  // 8.13.1. Set evaluationStatus to the result of running the classic script
  //         script.

  bool mute_errors = false;
  bool succeeded = false;
  auto* content = LookupScriptResource();
  DCHECK(content);
  base::SourceLocation script_location(script_url().spec(), 1, 1);
  std::string retval = web_context_->script_runner()->Execute(
      *content, script_location, mute_errors, &succeeded);
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
  // 8.17.1. For each eventType of settingsObject’s global object's associated
  //         list of event listeners' event types:
  // 8.17.1.1. Append eventType to workerGlobalScope’s associated service
  //           worker's set of event types to handle.
  // 8.17.1.2. Set script’s has ever been evaluated flag.
  // 8.18. Run the responsible event loop specified by settingsObject until it
  //       is destroyed.
  // 8.19. Empty workerGlobalScope’s list of active timers.
}

}  // namespace worker
}  // namespace cobalt
