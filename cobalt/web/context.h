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

#ifndef COBALT_WEB_CONTEXT_H_
#define COBALT_WEB_CONTEXT_H_

#include <string>

#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/script_loader_factory.h"
#include "cobalt/network/network_module.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/execution_state.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/script_runner.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/blob.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/web/user_agent_platform_info.h"
#include "cobalt/web/web_settings.h"

namespace cobalt {
namespace worker {
class ServiceWorkerRegistration;
class ServiceWorkerRegistrationObject;
class ServiceWorker;
class ServiceWorkerJobs;
class ServiceWorkerObject;
}  // namespace worker
namespace web {
class WindowOrWorkerGlobalScope;

class Context {
 public:
  virtual ~Context() {}

  class EnvironmentSettingsChangeObserver {
   public:
    virtual void OnEnvironmentSettingsChanged(bool context_valid) = 0;

   protected:
    virtual ~EnvironmentSettingsChangeObserver() = default;
  };

  virtual base::MessageLoop* message_loop() const = 0;
  virtual void ShutDownJavaScriptEngine() = 0;
  virtual loader::FetcherFactory* fetcher_factory() const = 0;
  virtual loader::ScriptLoaderFactory* script_loader_factory() const = 0;
  virtual script::JavaScriptEngine* javascript_engine() const = 0;
  virtual script::GlobalEnvironment* global_environment() const = 0;
  virtual script::ExecutionState* execution_state() const = 0;
  virtual script::ScriptRunner* script_runner() const = 0;
  virtual Blob::Registry* blob_registry() const = 0;
  virtual web::WebSettings* web_settings() const = 0;
  virtual network::NetworkModule* network_module() const = 0;
  virtual worker::ServiceWorkerJobs* service_worker_jobs() const = 0;

  virtual const std::string& name() const = 0;
  virtual void setup_environment_settings(EnvironmentSettings* settings) = 0;
  virtual EnvironmentSettings* environment_settings() const = 0;

  virtual scoped_refptr<worker::ServiceWorkerRegistration>
  LookupServiceWorkerRegistration(
      worker::ServiceWorkerRegistrationObject* registration) = 0;
  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#get-the-service-worker-registration-object
  virtual scoped_refptr<worker::ServiceWorkerRegistration>
  GetServiceWorkerRegistration(
      worker::ServiceWorkerRegistrationObject* registration) = 0;

  virtual void RemoveServiceWorker(worker::ServiceWorkerObject* worker) = 0;
  virtual scoped_refptr<worker::ServiceWorker> LookupServiceWorker(
      worker::ServiceWorkerObject* worker) = 0;
  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#get-the-service-worker-object
  virtual scoped_refptr<worker::ServiceWorker> GetServiceWorker(
      worker::ServiceWorkerObject* worker) = 0;

  virtual WindowOrWorkerGlobalScope* GetWindowOrWorkerGlobalScope() = 0;

  virtual const UserAgentPlatformInfo* platform_info() const = 0;

  virtual std::string GetUserAgent() const = 0;
  virtual std::string GetPreferredLanguage() const = 0;

  virtual void AddEnvironmentSettingsChangeObserver(
      EnvironmentSettingsChangeObserver* observer) = 0;
  virtual void RemoveEnvironmentSettingsChangeObserver(
      EnvironmentSettingsChangeObserver* observer) = 0;

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dfn-control
  virtual bool is_controlled_by(worker::ServiceWorkerObject* worker) const = 0;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-environment-active-service-worker
  virtual void set_active_service_worker(
      const scoped_refptr<worker::ServiceWorkerObject>& worker) = 0;
  virtual scoped_refptr<worker::ServiceWorkerObject>&
  active_service_worker() = 0;
  virtual const scoped_refptr<worker::ServiceWorkerObject>&
  active_service_worker() const = 0;
};

}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_CONTEXT_H_
