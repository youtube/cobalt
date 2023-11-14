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

#ifndef COBALT_WORKER_WORKER_H_
#define COBALT_WORKER_WORKER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_loop_current.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_runner.h"
#include "base/threading/thread.h"
#include "cobalt/base/source_location.h"
#include "cobalt/csp/content_security_policy.h"
#include "cobalt/loader/script_loader_factory.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/execution_state.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/script_runner.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/agent.h"
#include "cobalt/web/context.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/web/message_port.h"
#include "cobalt/worker/dedicated_worker_global_scope.h"
#include "cobalt/worker/worker_global_scope.h"
#include "cobalt/worker/worker_options.h"
#include "cobalt/worker/worker_settings.h"
#include "url/gurl.h"

#if defined(ENABLE_DEBUGGER)
#include "cobalt/debug/backend/debug_module.h"  // nogncheck
#endif                                          // defined(ENABLE_DEBUGGER)

namespace cobalt {
namespace worker {

class Worker : public base::MessageLoop::DestructionObserver {
 public:
  // Worker Options needed at thread run time.
  struct Options {
    web::Agent::Options web_options;
    web::WindowOrWorkerGlobalScope::Options global_scope_options;

    // Holds the source location where the worker was constructed.
    base::SourceLocation construction_location;

    // True if worker is a SharedWorker object, and false otherwise.
    bool is_shared = false;

    // Parameters from 'Run a worker' step 9.1 in the spec.
    //   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#dom-worker
    GURL url;
    web::Context* outside_context = nullptr;
    web::EventTarget* outside_event_target = nullptr;
    web::MessagePort* outside_port = nullptr;
    WorkerOptions options;
  };

  Worker(const char* name, const Options& options);
  ~Worker();
  Worker(const Worker&) = delete;
  Worker& operator=(const Worker&) = delete;

  void Terminate();

  // The task runner for this object.
  base::TaskRunner* task_runner() const {
    return web_agent_ ? web_agent_->message_loop()->task_runner() : nullptr;
  }

  void PostMessage(const script::ValueHandleHolder& message);

  // From base::MessageLoop::DestructionObserver.
  void WillDestroyCurrentMessageLoop() override;

 private:
  // Called by |Run| to perform initialization required on the dedicated
  // thread.
  void Initialize(web::Context* context);

  // Send an error event to the outside object.
  void SendErrorEventToOutside(const std::string& message);

  void OnContentProduced(const loader::Origin& last_url_origin,
                         std::unique_ptr<std::string> content);
  void OnLoadingComplete(const base::Optional<std::string>& error);
  void OnReadyToExecute();

  void Obtain();
  void Execute(const std::string& content,
               const base::SourceLocation& script_location);

  void Abort();

  web::Agent* web_agent() const { return web_agent_.get(); }

#if defined(ENABLE_DEBUGGER)
  // The core of the debugging system.
  std::unique_ptr<debug::backend::DebugModule> debug_module_;
#endif  // defined(ENABLE_DEBUGGER)

  // The Web Context includes the Script Agent and Realm.
  std::unique_ptr<web::Agent> web_agent_;

  web::Context* web_context_ = nullptr;

  Options options_;

  bool is_shared_ = false;

  scoped_refptr<WorkerGlobalScope> worker_global_scope_;

  // Inner message port.
  scoped_refptr<web::MessagePort> message_port_;

  // The loader that is used for asynchronous loads.
  std::unique_ptr<loader::Loader> loader_;

  // Content of the script. Released after Execute is called.
  std::unique_ptr<std::string> content_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_WORKER_H_
