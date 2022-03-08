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

#include "cobalt/dom/blob.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/network/network_module.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/execution_state.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/script_runner.h"
#include "cobalt/web/environment_settings.h"

namespace cobalt {
namespace web {

class Context {
 public:
  virtual ~Context() {}
  virtual void ShutDownJavaScriptEngine() = 0;
  virtual void set_fetcher_factory(loader::FetcherFactory* factory) = 0;
  virtual loader::FetcherFactory* fetcher_factory() const = 0;
  virtual script::JavaScriptEngine* javascript_engine() const = 0;
  virtual script::GlobalEnvironment* global_environment() const = 0;
  virtual script::ExecutionState* execution_state() const = 0;
  virtual script::ScriptRunner* script_runner() const = 0;
  virtual dom::Blob::Registry* blob_registry() const = 0;
  virtual network::NetworkModule* network_module() const = 0;

  virtual const std::string& name() const = 0;
  virtual void setup_environment_settings(EnvironmentSettings* settings) = 0;
  virtual EnvironmentSettings* environment_settings() const = 0;
};

}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_CONTEXT_H_
