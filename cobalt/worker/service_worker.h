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

#ifndef COBALT_WORKER_SERVICE_WORKER_H_
#define COBALT_WORKER_SERVICE_WORKER_H_

#include <memory>
#include <string>
#include <utility>

#include "cobalt/dom/event_target.h"
#include "cobalt/dom/event_target_listener_info.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/worker/abstract_worker.h"
#include "cobalt/worker/service_worker_state.h"

namespace cobalt {
namespace worker {

class ServiceWorker : public AbstractWorker, public dom::EventTarget {
 public:
  struct Options {
    explicit Options(ServiceWorkerState state, const std::string scriptURL)
        : state(state), script_url(std::move(scriptURL)) {}
    ServiceWorkerState state;
    const std::string script_url;
  };

  explicit ServiceWorker(script::EnvironmentSettings* settings,
                         const Options& options);

  // The scriptURL getter steps are to return the
  // service worker's serialized script url.
  std::string script_url() const { return script_url_; }
  ServiceWorkerState state() const { return state_; }

  const EventListenerScriptValue* onstatechange() const {
    return GetAttributeEventListener(base::Tokens::statechange());
  }
  void set_onstatechange(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::statechange(), event_listener);
  }

  // Web API: Abstract Worker
  //
  const EventListenerScriptValue* onerror() const override {
    return GetAttributeEventListener(base::Tokens::error());
  }
  void set_onerror(const EventListenerScriptValue& event_listener) override {
    SetAttributeEventListener(base::Tokens::error(), event_listener);
  }

  DEFINE_WRAPPABLE_TYPE(ServiceWorker);

 private:
  ~ServiceWorker() override = default;
  DISALLOW_COPY_AND_ASSIGN(ServiceWorker);

  const std::string script_url_;
  ServiceWorkerState state_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_SERVICE_WORKER_H_
