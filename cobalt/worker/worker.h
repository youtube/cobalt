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

#include "base/memory/scoped_refptr.h"
#include "base/threading/thread.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/execution_state.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/script_runner.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/worker/dedicated_worker_global_scope.h"
#include "cobalt/worker/message_port.h"
#include "cobalt/worker/worker_global_scope.h"
#include "cobalt/worker/worker_options.h"
#include "cobalt/worker/worker_settings.h"

namespace cobalt {
namespace worker {

class Worker {
 public:
  // Worker Options needed at thread run time.
  struct Options {
    // True if worker is a SharedWorker object, and false otherwise.
    bool is_shared;

    // Parameters from 'Run a worker' step 9.1 in the spec.
    //   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#dom-worker
    std::string url;
    script::EnvironmentSettings* outside_settings;
    MessagePort* outside_port;
    WorkerOptions options;

    script::JavaScriptEngine::Options javascript_engine_options;

    // The priority of the Worker thread.
    base::ThreadPriority thread_priority = base::ThreadPriority::NORMAL;
  };

  class DestructionObserver : public base::MessageLoop::DestructionObserver {
   public:
    explicit DestructionObserver(Worker* worker);
    void WillDestroyCurrentMessageLoop() override;

   private:
    Worker* worker_;
  };


  explicit Worker(const std::string& name);
  ~Worker();

  // Start the worker thread. Returns true if successful.
  bool Run(const Options& options);

  void Terminate();

  void ClearAllIntervalsAndTimeouts();

  MessagePort* message_port() const { return message_port_.get(); }

  // The message loop this object is running on.
  base::MessageLoop* message_loop() const { return thread_.message_loop(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(Worker);
  // Called by |Run| to perform initialization required on the dedicated
  // thread.
  void Initialize(const Options& options);

  void Obtain(const std::string& url);
  void Execute(const std::string& content,
               const base::SourceLocation& script_location);
  void Finalize();

  bool is_shared_;

  std::unique_ptr<WorkerSettings> environment_settings_;

  // The thread created and owned by this Worker. This is the thread
  // that is responsible for executing the worker JavaScript.
  base::Thread thread_;

  // JavaScript engine for the browser.
  std::unique_ptr<script::JavaScriptEngine> javascript_engine_;

  // TODO: Actual type here should depend on derived class (e.g. dedicated,
  // shared, service)
  scoped_refptr<DedicatedWorkerGlobalScope> worker_global_scope_;

  // JavaScript Global Object for the worker.
  scoped_refptr<script::GlobalEnvironment> global_environment_;

  // Interface for the worker to execute JavaScript code.
  std::unique_ptr<script::ScriptRunner> script_runner_;

  // Inner message port.
  scoped_refptr<MessagePort> message_port_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_WORKER_H_
