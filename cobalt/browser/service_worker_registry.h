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

#ifndef COBALT_BROWSER_SERVICE_WORKER_REGISTRY_H_
#define COBALT_BROWSER_SERVICE_WORKER_REGISTRY_H_

#include <memory>

#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "cobalt/network/network_module.h"
#include "cobalt/web/web_settings.h"
#include "cobalt/worker/service_worker_jobs.h"

namespace cobalt {
namespace browser {

// The Service Worker Registry tracks all registered service workers, processes
// mutations to the registry in a thread, and ensures that the scripts and
// metadata are stored persistently on disk.
class ServiceWorkerRegistry : public base::MessageLoop::DestructionObserver {
 public:
  ServiceWorkerRegistry(web::WebSettings* web_settings,
                        network::NetworkModule* network_module,
                        web::UserAgentPlatformInfo* platform_info);
  ~ServiceWorkerRegistry();

  // The message loop this object is running on.
  base::MessageLoop* message_loop() const { return thread_.message_loop(); }

  // From base::MessageLoop::DestructionObserver.
  void WillDestroyCurrentMessageLoop() override;

  worker::ServiceWorkerJobs* service_worker_jobs();

 private:
  // Called by the constructor to perform any other initialization required on
  // the dedicated thread.
  void Initialize(web::WebSettings* web_settings,
                  network::NetworkModule* network_module,
                  web::UserAgentPlatformInfo* platform_info);

  // The thread created and owned by the Service Worker Registry.
  // All registry mutations occur on this thread. The thread has to outlive all
  // web Agents that use service workers.,
  base::Thread thread_;

  // This event is used to signal when the destruction observers have been
  // added to the message loop. This is then used in Stop() to ensure that
  // processing doesn't continue until the thread is cleanly shutdown.
  base::WaitableEvent destruction_observer_added_ = {
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED};

  std::unique_ptr<worker::ServiceWorkerJobs> service_worker_jobs_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_SERVICE_WORKER_REGISTRY_H_
