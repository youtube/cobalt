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

#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "cobalt/browser/stack_size_constants.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/worker/dedicated_worker_global_scope.h"
#include "cobalt/worker/message_port.h"
#include "cobalt/worker/worker_global_scope.h"
#include "cobalt/worker/worker_options.h"
#include "cobalt/worker/worker_settings.h"

namespace cobalt {
namespace worker {

Worker::Worker(const std::string& name) : thread_(name) {}

Worker::DestructionObserver::DestructionObserver(Worker* worker)
    : worker_(worker) {}

void Worker::DestructionObserver::WillDestroyCurrentMessageLoop() {
  worker_->Finalize();
}

Worker::~Worker() {
  DCHECK(message_loop());

  DestructionObserver destruction_observer(this);
  message_loop()->task_runner()->PostBlockingTask(
      FROM_HERE, base::Bind(&base::MessageLoop::AddDestructionObserver,
                            base::Unretained(message_loop()),
                            base::Unretained(&destruction_observer)));

  // This will cancel the timers for tasks, which help the thread exit
  ClearAllIntervalsAndTimeouts();

  // Stop the thread. This will cause the destruction observer to be notified.
  thread_.Stop();
}

void Worker::ClearAllIntervalsAndTimeouts() {
  // TODO
}

// Algorithm for 'run a worker'
//   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#run-a-worker
bool Worker::Run(const Options& options) {
  // 1. Let is shared be true if worker is a SharedWorker object, and false
  //    otherwise.
  is_shared_ = options.is_shared;
  // 2. Let owner be the relevant owner to add given outside settings.
  // 3. Let parent worker global scope be null.
  // 4. If owner is a WorkerGlobalScope object (i.e., we are creating a nested
  //    dedicated worker), then set parent worker global scope to owner.
  // 5. Let unsafeWorkerCreationTime be the unsafe shared current time.
  // 6. Let agent be the result of obtaining a dedicated/shared worker agent
  //    given outside settings and is shared. Run the rest of these steps in
  //    that agent.
  base::Thread::Options thread_options(base::MessageLoop::TYPE_DEFAULT,
                                       cobalt::browser::kWorkerStackSize);
  thread_options.priority = options.thread_priority;
  if (!thread_.StartWithOptions(thread_options)) return false;
  DCHECK(message_loop());

  message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&Worker::Initialize, base::Unretained(this), options));
  return true;
}

void Worker::Initialize(const Options& options) {
  LOG(INFO) << "Look at me, I'm running a worker thread";

  // 7. Let realm execution context be the result of creating a new
  // JavaScript
  //    realm given agent and the following customizations:
  //    . For the global object, if is shared is true, create a new
  //      SharedWorkerGlobalScope object. Otherwise, create a new
  //      DedicatedWorkerGlobalScope object.

  javascript_engine_ =
      script::JavaScriptEngine::CreateEngine(options.javascript_engine_options);
  DCHECK(javascript_engine_);

  global_environment_ = javascript_engine_->CreateGlobalEnvironment();
  DCHECK(global_environment_);

  script_runner_ =
      script::ScriptRunner::CreateScriptRunner(global_environment_);
  DCHECK(script_runner_);

  // 8. Let worker global scope be the global object of realm execution
  //    context's Realm component.
  // 9. Set up a worker environment settings object with realm execution
  //    context, outside settings, and unsafeWorkerCreationTime, and let
  //    inside settings be the result.

  environment_settings_.reset(
      new WorkerSettings(javascript_engine_.get(), global_environment_.get()));

  // 10. Set worker global scope's name to the value of options's name member.
  // 11. Append owner to worker global scope's owner set.
  // 12. If is shared is true, then:
  //     1. Set worker global scope's constructor origin to outside
  //     settings's
  //        origin.
  //     2. Set worker global scope's constructor url to url.
  //     3. Set worker global scope's type to the value of options's type
  //        member.
  //     4. Set worker global scope's credentials to the value of
  //     options's
  //        credentials member.
  // 13. Let destination be "sharedworker" if is shared is true, and
  // "worker"
  //     otherwise.
  // 14. Obtain script
  Obtain(options.url);
}

void Worker::Obtain(const std::string& url) {
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
  //     3. Set worker global scope's url to response's url.
  //     4. Initialize worker global scope's policy container given worker
  //        global scope, response, and inside settings.
  //     5. If the Run CSP initialization for a global object algorithm returns
  //        "Blocked" when executed upon worker global scope, set response to a
  //        network error. [CSP]
  //     6. If worker global scope's embedder policy's value is compatible with
  //        cross-origin isolation and is shared is true, then set agent's agent
  //        cluster's cross-origin isolation mode to "logical" or "concrete".
  //        The one chosen is implementation-defined.
  //     7. If the result of checking a global object's embedder policy with
  //        worker global scope, outside settings, and response is false, then
  //        set response to a network error.
  //     8. Set worker global scope's cross-origin isolated capability to true
  //        if agent's agent cluster's cross-origin isolation mode is
  //        "concrete".
  //     9. If is shared is false and owner's cross-origin isolated capability
  //        is false, then set worker global scope's cross-origin isolated
  //        capability to false.
  //     10. If is shared is false and response's url's scheme is "data", then
  //         set worker global scope's cross-origin isolated capability to
  //         false.
  //     11. Asynchronously complete the perform the fetch steps with response.

  //     If the algorithm asynchronously completes with null or with a script
  //     whose error to rethrow is non-null, then:
  //     1. Queue a global task on the DOM manipulation task source given
  //        worker's relevant global object to fire an event named error at
  //        worker.
  //     2. Run the environment discarding steps for inside settings.
  //     3. Return.

  //     Otherwise, continue the rest of these steps after the algorithm's
  //     asynchronous completion, with script being the asynchronous completion
  //     value.
  Execute(url, base::SourceLocation("[object Worker::RunLoop]", 1, 1));
}

void Worker::Execute(const std::string& content,
                     const base::SourceLocation& script_location) {
  // 15. Associate worker with worker global scope.
  // 16. Let inside port be a new MessagePort object in inside settings's Realm.
  // Note: This will need the EventTarget and the EnvironmentSettings of the
  // Worker thread.
  // 17. Associate inside port with worker global scope.
  // TODO: Actual type here should depend on derived class (e.g. dedicated,
  // shared, service)
  worker_global_scope_ =
      new DedicatedWorkerGlobalScope(environment_settings_.get());
  message_port_ =
      new MessagePort(worker_global_scope_, environment_settings_.get());
  global_environment_->CreateGlobalObject(worker_global_scope_,
                                          environment_settings_.get());
  // 18. Entangle outside port and inside port.
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
  LOG(INFO) << "Script Executing \"" << content << "\".";
  std::string retval = script_runner_->Execute(content, script_location,
                                               mute_errors, &succeeded);
  LOG(INFO) << "Script Executed " << (succeeded ? "and" : ", but not")
            << " succeeded: \"" << retval << "\"";

  // 24. Enable outside port's port message queue.
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
  // 28. Event loop: Run the responsible event loop specified by inside settings
  //     until it is destroyed.
  // Finalize();
}

void Worker::Finalize() {
  // 29. Clear the worker global scope's map of active timers.
  // 30. Disentangle all the ports in the list of the worker's ports.
  // 31. Empty worker global scope's owner set.

  global_environment_->SetReportEvalCallback(base::Closure());
  global_environment_->SetReportErrorCallback(
      script::GlobalEnvironment::ReportErrorCallback());

  script_runner_.reset();
  global_environment_ = NULL;
  javascript_engine_.reset();
}

// Algorithm for 'Terminate a worker'
//   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#terminate-a-worker
void Worker::Terminate() {
  // 1. Set the worker's WorkerGlobalScope object's closing flag to true.
  // 2. If there are any tasks queued in the WorkerGlobalScope object's relevant
  //    agent's event loop's task queues, discard them without processing them.
  // 3. Abort the script currently running in the worker.
  // 4. If the worker's WorkerGlobalScope object is actually a
  //    DedicatedWorkerGlobalScope object (i.e. the worker is a dedicated
  //    worker), then empty the port message queue of the port that the worker's
  //    implicit port is entangled with.
}

}  // namespace worker
}  // namespace cobalt
