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

#ifndef COBALT_WORKER_DEDICATED_WORKER_GLOBAL_SCOPE_H_
#define COBALT_WORKER_DEDICATED_WORKER_GLOBAL_SCOPE_H_

#include <string>

#include "cobalt/base/tokens.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/dom/event_target_listener_info.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/worker/worker_global_scope.h"

namespace cobalt {
namespace worker {

// Implementation of Dedicated workers and the Worker interface.
//   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#dedicated-workers-and-the-dedicatedworkerglobalscope-interface

class DedicatedWorkerGlobalScope : public WorkerGlobalScope {
 public:
  explicit DedicatedWorkerGlobalScope(
      script::EnvironmentSettings* settings,
      bool parent_cross_origin_isolated_capability);

  // Web API: DedicatedWorkerGlobalScope
  //
  void set_name(const std::string& name) { name_ = name; }
  std::string name() { return name_; }

  void Initialize() override;

  void PostMessage(const std::string& message);
  void Close() {}

  const dom::EventTargetListenerInfo::EventListenerScriptValue* onmessage() {
    return GetAttributeEventListener(base::Tokens::message());
  }
  void set_onmessage(
      const dom::EventTargetListenerInfo::EventListenerScriptValue&
          event_listener) {
    SetAttributeEventListener(base::Tokens::message(), event_listener);
  }
  const dom::EventTargetListenerInfo::EventListenerScriptValue*
  onmessageerror() {
    return GetAttributeEventListener(base::Tokens::messageerror());
  }
  void set_onmessageerror(
      const dom::EventTargetListenerInfo::EventListenerScriptValue&
          event_listener) {
    SetAttributeEventListener(base::Tokens::messageerror(), event_listener);
  }

  DEFINE_WRAPPABLE_TYPE(DedicatedWorkerGlobalScope);

 protected:
  ~DedicatedWorkerGlobalScope() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DedicatedWorkerGlobalScope);
  bool cross_origin_isolated_capability_;

  std::string name_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_DEDICATED_WORKER_GLOBAL_SCOPE_H_
