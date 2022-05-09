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

#ifndef COBALT_WEB_STUB_WEB_CONTEXT_H_
#define COBALT_WEB_STUB_WEB_CONTEXT_H_

#include <memory>
#include <string>

#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/script_loader_factory.h"
#include "cobalt/network/network_module.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/web/context.h"
#include "cobalt/web/environment_settings.h"
#include "url/gurl.h"

namespace cobalt {
namespace web {
namespace test {

class StubSettings : public EnvironmentSettings {
 public:
  explicit StubSettings(const GURL& base) { set_base_url(base); }

 private:
};

class StubWebContext : public web::Context {
 public:
  StubWebContext() : web::Context(), name_("StubWebInstance") {
    javascript_engine_ = script::JavaScriptEngine::CreateEngine();
    global_environment_ = javascript_engine_->CreateGlobalEnvironment();
  }
  virtual ~StubWebContext() {}

  // WebInstance
  //
  base::MessageLoop* message_loop() const final {
    NOTREACHED();
    return nullptr;
  }
  void ShutDownJavaScriptEngine() final { NOTREACHED(); }
  void set_fetcher_factory(loader::FetcherFactory* factory) final {
    fetcher_factory_.reset(factory);
  }
  loader::FetcherFactory* fetcher_factory() const final {
    DCHECK(fetcher_factory_);
    return fetcher_factory_.get();
  }

  loader::ScriptLoaderFactory* script_loader_factory() const final {
    DCHECK(script_loader_factory_);
    return script_loader_factory_.get();
  }

  script::JavaScriptEngine* javascript_engine() const final {
    DCHECK(javascript_engine_);
    return javascript_engine_.get();
  }
  script::GlobalEnvironment* global_environment() const final {
    DCHECK(global_environment_);
    return global_environment_.get();
  }
  script::ExecutionState* execution_state() const final {
    NOTREACHED();
    return nullptr;
  }
  script::ScriptRunner* script_runner() const final {
    NOTREACHED();
    return nullptr;
  }
  dom::Blob::Registry* blob_registry() const final {
    NOTREACHED();
    return nullptr;
  }
  void set_network_module(network::NetworkModule* module) {
    network_module_.reset(module);
  }
  network::NetworkModule* network_module() const final {
    DCHECK(network_module_);
    return network_module_.get();
  }

  const std::string& name() const final { return name_; };
  void setup_environment_settings(
      EnvironmentSettings* environment_settings) final {
    environment_settings_.reset(environment_settings);
    if (environment_settings_) environment_settings_->set_context(this);
  }
  web::EnvironmentSettings* environment_settings() const final {
    return environment_settings_.get();
  }

  scoped_refptr<worker::ServiceWorkerRegistration> GetServiceWorkerRegistration(
      worker::ServiceWorkerRegistrationObject* registration) final {
    NOTIMPLEMENTED();
    return scoped_refptr<worker::ServiceWorkerRegistration>();
  }

  // Other
 private:
  // Name of the web instance.
  const std::string name_;

  std::unique_ptr<loader::FetcherFactory> fetcher_factory_;
  std::unique_ptr<loader::ScriptLoaderFactory> script_loader_factory_;
  std::unique_ptr<script::JavaScriptEngine> javascript_engine_;
  scoped_refptr<script::GlobalEnvironment> global_environment_;

  std::unique_ptr<network::NetworkModule> network_module_;
  // Environment Settings object
  std::unique_ptr<web::EnvironmentSettings> environment_settings_;
};

}  // namespace test
}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_STUB_WEB_CONTEXT_H_
