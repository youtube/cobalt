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

#ifndef COBALT_WORKER_SERVICE_WORKER_OBJECT_H_
#define COBALT_WORKER_SERVICE_WORKER_OBJECT_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_loop_current.h"
#include "cobalt/web/agent.h"
#include "cobalt/web/context.h"
#include "cobalt/worker/service_worker_state.h"
#include "cobalt/worker/worker_global_scope.h"
#include "starboard/atomic.h"
#include "url/gurl.h"

#if defined(ENABLE_DEBUGGER)
#include "cobalt/debug/backend/debug_module.h"  // nogncheck
#endif                                          // defined(ENABLE_DEBUGGER)

namespace cobalt {
namespace worker {
class ServiceWorkerRegistrationObject;

// This class represents the 'service worker'.
//   https://w3c.github.io/ServiceWorker/#dfn-service-worker
// Not to be confused with the ServiceWorker JavaScript object,  this represents
// the service worker in the browser, independent from the JavaScript realm.
// The lifetime of a service worker is tied to the execution lifetime of events
// and not references held by service worker clients to the ServiceWorker
// object. A user agent may terminate service workers at any time it has no
// event to handle, or detects abnormal operation.
//   https://w3c.github.io/ServiceWorker/#service-worker-lifetime
class ServiceWorkerObject
    : public base::RefCountedThreadSafe<ServiceWorkerObject>,
      public base::SupportsWeakPtr<ServiceWorkerObject>,
      public base::MessageLoop::DestructionObserver {
 public:
  // Worker Options needed at thread run time.
  struct Options {
    Options(
        const std::string& name, network::NetworkModule* network_module,
        ServiceWorkerRegistrationObject* containing_service_worker_registration)
        : name(name),
          containing_service_worker_registration(
              containing_service_worker_registration) {
      web_options.network_module = network_module;
    }

    std::string name;
    web::Agent::Options web_options;
    ServiceWorkerRegistrationObject* containing_service_worker_registration;
  };

  explicit ServiceWorkerObject(const Options& options);
  ~ServiceWorkerObject();
  ServiceWorkerObject(const ServiceWorkerObject&) = delete;
  ServiceWorkerObject& operator=(const ServiceWorkerObject&) = delete;

  // https://w3c.github.io/ServiceWorker/#dfn-state
  void set_state(ServiceWorkerState state) { state_ = state; }
  ServiceWorkerState state() const { return state_; }
  // https://w3c.github.io/ServiceWorker/#dfn-script-url
  void set_script_url(const GURL& script_url) { script_url_ = script_url; }
  const GURL& script_url() const { return script_url_; }

  // https://w3c.github.io/ServiceWorker/#dfn-containing-service-worker-registration
  ServiceWorkerRegistrationObject* containing_service_worker_registration() {
    return options_.containing_service_worker_registration;
  }
  // https://w3c.github.io/ServiceWorker/#dfn-skip-waiting-flag
  void set_skip_waiting() { skip_waiting_ = true; }
  bool skip_waiting() const { return skip_waiting_; }

  // https://w3c.github.io/ServiceWoScriptResourceMaprker/#dfn-classic-scripts-imported-flag
  void set_classic_scripts_imported() { classic_scripts_imported_ = true; }
  bool classic_scripts_imported() { return classic_scripts_imported_; }

  // https://w3c.github.io/ServiceWorker/#dfn-set-of-used-scripts
  void AppendToSetOfUsedScripts(const GURL& url) {
    set_of_used_scripts_.insert(url);
  }

  // https://w3c.github.io/ServiceWorker/#dfn-script-resource-map
  void set_script_resource_map(ScriptResourceMap&& resource_map) {
    script_resource_map_ = std::move(resource_map);
  }
  void SetScriptResource(const GURL& url, std::string* resource);
  bool HasScriptResource() const;
  std::string* LookupScriptResource(const GURL& url) const;

  // Steps 13-15 of Algorithm for Install.
  //   https://w3c.github.io/ServiceWorker/#installation-algorithm
  void PurgeScriptResourceMap();
  const ScriptResourceMap& script_resource_map() {
    return script_resource_map_;
  }

  // https://w3c.github.io/ServiceWorker/#service-worker-start-status
  void set_start_status(std::string* start_status) {
    start_status_.reset(start_status);
  }
  std::string* start_status() const { return start_status_.get(); }

  void Abort();
  bool is_running() { return web_agent_.get() != nullptr; }
  web::Agent* web_agent() const { return web_agent_.get(); }

  const scoped_refptr<WorkerGlobalScope>& worker_global_scope() const {
    return worker_global_scope_;
  }

  // From base::MessageLoop::DestructionObserver.
  void WillDestroyCurrentMessageLoop() override;

  void store_start_failed(bool value) { start_failed_.store(value); }
  bool load_start_failed() { return start_failed_.load(); }

  void ObtainWebAgentAndWaitUntilDone();

 private:
  // Called by ObtainWebAgentAndWaitUntilDone to perform initialization required
  // on the dedicated thread.
  //   https://w3c.github.io/ServiceWorker/#run-service-worker-algorithm
  void Initialize(web::Context* context);

  // The message loop this object is running on.
  base::MessageLoop* message_loop() const {
    return web_agent_ ? web_agent_->message_loop() : nullptr;
  }

#if defined(ENABLE_DEBUGGER)
  // The core of the debugging system.
  std::unique_ptr<debug::backend::DebugModule> debug_module_;
#endif  // defined(ENABLE_DEBUGGER)

  // The Web Context includes the Script Agent and Realm.
  std::unique_ptr<web::Agent> web_agent_;

  web::Context* web_context_ = nullptr;

  Options options_;

  // https://w3c.github.io/ServiceWorker/#dfn-state
  ServiceWorkerState state_;

  // https://w3c.github.io/ServiceWorker/#dfn-script-url
  GURL script_url_;

  // https://w3c.github.io/ServiceWorker/#dfn-script-resource-map
  ScriptResourceMap script_resource_map_;

  // https://w3c.github.io/ServiceWorker/#dfn-set-of-used-scripts
  std::set<GURL> set_of_used_scripts_;

  // https://w3c.github.io/ServiceWorker/#dfn-skip-waiting-flag
  bool skip_waiting_ = false;

  // https://w3c.github.io/ServiceWorker/#dfn-classic-scripts-imported-flag
  bool classic_scripts_imported_ = false;

  // https://w3c.github.io/ServiceWorker/#service-worker-start-status
  std::unique_ptr<std::string> start_status_;

  starboard::atomic_bool start_failed_;

  scoped_refptr<WorkerGlobalScope> worker_global_scope_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_SERVICE_WORKER_OBJECT_H_
