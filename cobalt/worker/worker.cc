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

#include "cobalt/worker/worker.h"

#include <string>
#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/task_runner.h"
#include "base/threading/thread.h"
#include "cobalt/browser/user_agent_platform_info.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/v8c/conversion_helpers.h"
#include "cobalt/script/v8c/v8c_exception_state.h"
#include "cobalt/script/v8c/v8c_value_handle.h"
#include "cobalt/web/error_event.h"
#include "cobalt/web/error_event_init.h"
#include "cobalt/web/message_port.h"
#include "cobalt/worker/dedicated_worker_global_scope.h"
#include "cobalt/worker/worker_global_scope.h"
#include "cobalt/worker/worker_options.h"
#include "cobalt/worker/worker_settings.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace worker {

Worker::Worker(const char* name, const Options& options) : options_(options) {
  message_port_ = new web::MessagePort();
  // Algorithm for 'run a worker'
  //   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#run-a-worker
  // 1. Let is shared be true if worker is a SharedWorker object, and false
  //    otherwise.
  is_shared_ = options.is_shared;
  // 2. Moved to below.
  // 3. Let parent worker global scope be null.
  // 4. If owner is a WorkerGlobalScope object (i.e., we are creating a nested
  //    dedicated worker), then set parent worker global scope to owner.
  // 5. Let unsafeWorkerCreationTime be the unsafe shared current time.
  // 6. Let agent be the result of obtaining a dedicated/shared worker agent
  //    given outside settings and is shared. Run the rest of these steps in
  //    that agent.
  std::string agent_name(name);
  if (!options.options.name().empty()) {
    agent_name.push_back(':');
    agent_name.append(options.options.name());
  }
  web_agent_.reset(new web::Agent(agent_name));
  web_agent_->Run(options.web_options,
                  base::Bind(&Worker::Initialize, base::Unretained(this)),
                  this);
}

void Worker::WillDestroyCurrentMessageLoop() {
#if defined(ENABLE_DEBUGGER)
  debug_module_.reset();
#endif  // ENABLE_DEBUGGER
  // Destroy members that were constructed in the worker thread.
  loader_.reset();
  worker_global_scope_ = nullptr;
  content_.reset();
}

Worker::~Worker() { Abort(); }

void Worker::Initialize(web::Context* context) {
  // Algorithm for 'run a worker':
  //   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#run-a-worker
  // 7. Let realm execution context be the result of creating a new
  //    JavaScript realm given agent and the following customizations:
  web_context_ = context;
  //    . For the global object, if is shared is true, create a new
  //      SharedWorkerGlobalScope object. Otherwise, create a new
  //      DedicatedWorkerGlobalScope object.
  WorkerSettings* worker_settings = new WorkerSettings(options_.outside_port);
  // From algorithm to set up a worker environment settings object
  // Let inherited origin be outside settings's origin.
  // The origin return a unique opaque origin if worker global scope's url's
  // scheme is "data", and inherited origin otherwise.
  //   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#set-up-a-worker-environment-settings-object
  worker_settings->set_origin(
      options_.outside_context->environment_settings()->GetOrigin());
  web_context_->SetupEnvironmentSettings(worker_settings);
  // From algorithm for to setup up a worker environment settings object:
  //   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#set-up-a-worker-environment-settings-object
  // 5. Set settings object's creation URL to worker global scope's url.
  web_context_->environment_settings()->set_creation_url(options_.url);
  // Continue algorithm for 'run a worker'.
  // 8. Let worker global scope be the global object of realm execution
  //    context's Realm component.
  scoped_refptr<DedicatedWorkerGlobalScope> dedicated_worker_global_scope =
      new DedicatedWorkerGlobalScope(
          web_context_->environment_settings(), options_.global_scope_options,
          /*parent_cross_origin_isolated_capability*/ false);
  worker_global_scope_ = dedicated_worker_global_scope;
  // 9. Set up a worker environment settings object with realm execution
  //    context, outside settings, and unsafeWorkerCreationTime, and let
  //    inside settings be the result.
  web_context_->global_environment()->CreateGlobalObject(
      dedicated_worker_global_scope, web_context_->environment_settings());
  DCHECK(!web_context_->GetWindowOrWorkerGlobalScope()->IsWindow());
  DCHECK(web_context_->GetWindowOrWorkerGlobalScope()->IsDedicatedWorker());
  DCHECK(!web_context_->GetWindowOrWorkerGlobalScope()->IsServiceWorker());
  DCHECK(web_context_->GetWindowOrWorkerGlobalScope()->GetWrappableType() ==
         base::GetTypeId<DedicatedWorkerGlobalScope>());
  DCHECK_EQ(dedicated_worker_global_scope,
            base::polymorphic_downcast<DedicatedWorkerGlobalScope*>(
                web_context_->GetWindowOrWorkerGlobalScope()));

#if defined(ENABLE_DEBUGGER)
  debug_module_.reset(new debug::backend::DebugModule(
      nullptr /* debugger_hooks */, web_context_->global_environment(),
      nullptr /* render_overlay */, nullptr /* resource_provider */,
      nullptr /* window */, nullptr /* debugger_state */));
#endif  // ENABLE_DEBUGGER

  // 10. Set worker global scope's name to the value of options's name member.
  dedicated_worker_global_scope->set_name(options_.options.name());
  web_context_->SetupFinished();
  // (Moved) 2. Let owner be the relevant owner to add given outside settings.
  web::WindowOrWorkerGlobalScope* owner =
      options_.outside_context->GetWindowOrWorkerGlobalScope();
  if (!owner) {
    // There is not a running owner.
    return;
  }
  // 11. Append owner to worker global scope's owner set.
  dedicated_worker_global_scope->owner_set()->insert(owner);
  // 12. If is shared is true, then:
  //     1. Set worker global scope's constructor origin to outside
  //     settings's
  //        origin.
  //     2. Set worker global scope's constructor url to url.
  //     3. Set worker global scope's type to the value of options's type
  //        member.
  //     4. Set worker global scope's credentials to the value of options's
  //        credentials member.
  // 13. Let destination be "sharedworker" if is shared is true, and
  // "worker" otherwise.
  // 14. Obtain script

  Obtain();
}

// Fetch and Run classic script
void Worker::Obtain() {
  // Algorithm for 'run a worker'
  //   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#run-a-worker
  // 14. Obtain script by switching on the value of options's type member:
  //     - "classic"
  //         Fetch a classic worker script given url, outside settings,
  //         destination, and inside settings.
  //     - "module"
  //         Fetch a module worker script graph given url, outside settings,
  //         destination, the value of the credentials member of options, and
  //         inside settings.

  //     In both cases, to perform the fetch given request, perform the
  //     following steps if the is top-level flag is set:
  //     1. Set request's reserved client to inside settings.
  //     2. Fetch request, and asynchronously wait to run the remaining steps as
  //        part of fetch's process response for the response response.
  DCHECK(web_context_);
  DCHECK(web_context_->environment_settings());
  const GURL& url = web_context_->environment_settings()->creation_url();
  DCHECK(!url.is_empty());
  loader::Origin origin = loader::Origin(url.GetOrigin());

  DCHECK(options_.outside_context);

  // Window thread may remove its global object before destroying worker.
  if (!options_.outside_context->GetWindowOrWorkerGlobalScope()) {
    return;
  }

  csp::SecurityCallback csp_callback = base::Bind(
      &web::CspDelegate::CanLoad,
      base::Unretained(options_.outside_context->GetWindowOrWorkerGlobalScope()
                           ->csp_delegate()),
      web::CspDelegate::kWorker);

  loader_ = web_context_->script_loader_factory()->CreateScriptLoader(
      url, origin, csp_callback,
      base::Bind(&Worker::OnContentProduced, base::Unretained(this)),
      base::Bind(&WorkerGlobalScope::InitializePolicyContainerCallback,
                 worker_global_scope_),
      base::Bind(&Worker::OnLoadingComplete, base::Unretained(this)));
}

void Worker::SendErrorEventToOutside(const std::string& message) {
  LOG(WARNING) << "Worker loading failed : " << message;
  options_.outside_context->message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(
          [](base::WeakPtr<web::EventTarget> event_target,
             const std::string& message, const std::string& filename) {
            web::ErrorEventInit error;
            error.set_message(message);
            error.set_filename(filename);
            event_target->DispatchEvent(new web::ErrorEvent(
                event_target->environment_settings(), error));
          },
          base::AsWeakPtr(options_.outside_event_target), message,
          web_context_->environment_settings()->creation_url().spec()));
}

void Worker::OnContentProduced(const loader::Origin& last_url_origin,
                               std::unique_ptr<std::string> content) {
  // Algorithm for 'run a worker'
  //   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#run-a-worker
  DCHECK(content);
  // 14.3. "Set worker global scope's url to response's url."
  worker_global_scope_->set_url(
      web_context_->environment_settings()->creation_url());
  // 14.4 - 14.10 initialize worker global scope
  worker_global_scope_->Initialize();
  // 14.11. Asynchronously complete the perform the fetch steps with response.
  content_ = std::move(content);
}

void Worker::OnLoadingComplete(const base::Optional<std::string>& error) {
  // Algorithm for 'run a worker'
  //   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#run-a-worker
  //     If the algorithm asynchronously completes with null or with a script
  //     whose error to rethrow is non-null, then:
  if (error || !content_) {
    //     1. Queue a global task on the DOM manipulation task source given
    //        worker's relevant global object to fire an event named error at
    //        worker.
    SendErrorEventToOutside(error.value_or("No content for worker."));
    //     2. Run the environment discarding steps for inside settings.
    //     3. Return.
    return;
  }
  OnReadyToExecute();
}

void Worker::OnReadyToExecute() {
  DCHECK(content_);
  Execute(*content_,
          base::SourceLocation(
              web_context_->environment_settings()->base_url().spec(), 1, 1));
  content_.reset();
}

void Worker::Execute(const std::string& content,
                     const base::SourceLocation& script_location) {
  // Algorithm for 'run a worker'
  //   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#run-a-worker
  // 15. Associate worker with worker global scope.
  // Done at step 8.
  // 16. Let inside port be a new MessagePort object in inside settings's Realm.
  // 17. Associate inside port with worker global scope.
  message_port_->EntangleWithEventTarget(worker_global_scope_);
  // 18. Entangle outside port and inside port.
  // TODO(b/226640425): Implement this when Message Ports can be entangled.
  // 19. Create a new WorkerLocation object and associate it with worker global
  //     scope.
  // 20. Closing orphan workers: Start monitoring the worker such that no sooner
  //     than it stops being a protected worker, and no later than it stops
  //     being a permissible worker, worker global scope's closing flag is set
  //     to true.
  // 21. Suspending workers: Start monitoring the worker, such that whenever
  //     worker global scope's closing flag is false and the worker is a
  //     suspendable worker, the user agent suspends execution of script in that
  //     worker until such time as either the closing flag switches to true or
  //     the worker stops being a suspendable worker.
  // 22. Set inside settings's execution ready flag.
  // 23. If script is a classic script, then run the classic script script.
  //     Otherwise, it is a module script; run the module script script.

  bool mute_errors = false;
  bool succeeded = false;
  std::string retval = web_context_->script_runner()->Execute(
      content, script_location, mute_errors, &succeeded);
  if (!succeeded) {
    SendErrorEventToOutside(retval);
  }

  // 24. Enable outside port's port message queue.
  // TODO(b/226640425): Implement this when Message Ports can be entangled.
  // 25. If is shared is false, enable the port message queue of the worker's
  //     implicit port.
  // 26. If is shared is true, then queue a global task on DOM manipulation task
  //     source given worker global scope to fire an event named connect at
  //     worker global scope, using MessageEvent, with the data attribute
  //     initialized to the empty string, the ports attribute initialized to a
  //     new frozen array containing inside port, and the source attribute
  //     initialized to inside port.
  // 27. Enable the client message queue of the ServiceWorkerContainer object
  //     whose associated service worker client is worker global scope's
  //     relevant settings object.
  // TODO(b/226640425): Implement this when Message Ports can be entangled.
  // 28. Event loop: Run the responsible event loop specified by inside settings
  //     until it is destroyed.
}

void Worker::Abort() {
  // Algorithm for 'run a worker'
  //   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#run-a-worker
  // 29. Clear the worker global scope's map of active timers.
  if (worker_global_scope_ && task_runner()) {
    task_runner()->PostTask(
        FROM_HERE, base::Bind(
                       [](web::WindowOrWorkerGlobalScope* global_scope) {
                         global_scope->DestroyTimers();
                       },
                       base::Unretained(worker_global_scope_.get())));
  }
  // 30. Disentangle all the ports in the list of the worker's ports.
  // 31. Empty worker global scope's owner set.
  if (worker_global_scope_) {
    worker_global_scope_->owner_set()->clear();
  }
  if (web_agent_) {
    std::unique_ptr<web::Agent> web_agent(std::move(web_agent_));
    DCHECK(web_agent);
    DCHECK(!web_agent_);
    web_agent->WaitUntilDone();
    web_context_ = nullptr;
    web_agent->Stop();
    web_agent.reset();
  }
}

// Algorithm for 'Terminate a worker'
//   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#terminate-a-worker
void Worker::Terminate() {
  // 1. Set the worker's WorkerGlobalScope object's closing flag to true.
  if (worker_global_scope_) {
    worker_global_scope_->set_closing_flag(true);
  }
  // 2. If there are any tasks queued in the WorkerGlobalScope object's relevant
  //    agent's event loop's task queues, discard them without processing them.
  // 3. Abort the script currently running in the worker.
  Abort();
  // 4. If the worker's WorkerGlobalScope object is actually a
  //    DedicatedWorkerGlobalScope object (i.e. the worker is a dedicated
  //    worker), then empty the port message queue of the port that the worker's
  //    implicit port is entangled with.
  // TODO(b/226640425): Implement this when Message Ports can be entangled.
}

void Worker::PostMessage(const script::ValueHandleHolder& message) {
  DCHECK(message_port_);
  message_port_->PostMessage(message);
}

}  // namespace worker
}  // namespace cobalt
