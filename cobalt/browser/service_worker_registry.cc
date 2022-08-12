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

#include "cobalt/browser/service_worker_registry.h"

#include <string>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/network/network_module.h"
#include "cobalt/worker/service_worker_jobs.h"

namespace cobalt {
namespace browser {

namespace {
// Signals the given WaitableEvent.
void SignalWaitableEvent(base::WaitableEvent* event) { event->Signal(); }
}  // namespace

void ServiceWorkerRegistry::WillDestroyCurrentMessageLoop() {
  // Clear all member variables allocated from the thread.
  service_worker_jobs_.reset();
}

ServiceWorkerRegistry::ServiceWorkerRegistry(
    web::WebSettings* web_settings, network::NetworkModule* network_module,
    web::UserAgentPlatformInfo* platform_info)
    : thread_("ServiceWorkerRegistry") {
  if (!thread_.Start()) return;
  DCHECK(message_loop());

  message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ServiceWorkerRegistry::Initialize, base::Unretained(this),
                 web_settings, network_module, platform_info));

  // Register as a destruction observer to shut down the Web Agent once all
  // pending tasks have been executed and the message loop is about to be
  // destroyed. This allows us to safely stop the thread, drain the task queue,
  // then destroy the internal components before the message loop is reset.
  // No posted tasks will be executed once the thread is stopped.
  message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&base::MessageLoop::AddDestructionObserver,
                 base::Unretained(message_loop()), base::Unretained(this)));

  // This works almost like a PostBlockingTask, except that any blocking that
  // may be necessary happens when Stop() is called instead of right now.
  message_loop()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&SignalWaitableEvent,
                            base::Unretained(&destruction_observer_added_)));
}

ServiceWorkerRegistry::~ServiceWorkerRegistry() {
  DCHECK(message_loop());
  DCHECK(thread_.IsRunning());

  // Ensure that the destruction observer got added before stopping the thread.
  // Stop the thread. This will cause the destruction observer to be notified.
  destruction_observer_added_.Wait();
  service_worker_jobs_->Stop();
  thread_.Stop();
}

worker::ServiceWorkerJobs* ServiceWorkerRegistry::service_worker_jobs() {
  // Ensure that the thread had a chance to allocate the object.
  destruction_observer_added_.Wait();
  return service_worker_jobs_.get();
}

void ServiceWorkerRegistry::Initialize(
    web::WebSettings* web_settings, network::NetworkModule* network_module,
    web::UserAgentPlatformInfo* platform_info) {
  TRACE_EVENT0("cobalt::browser", "ServiceWorkerRegistry::Initialize()");
  DCHECK_EQ(base::MessageLoop::current(), message_loop());
  service_worker_jobs_.reset(new worker::ServiceWorkerJobs(
      web_settings, network_module, platform_info, message_loop()));
}

}  // namespace browser
}  // namespace cobalt
