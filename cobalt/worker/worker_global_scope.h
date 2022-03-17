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

#ifndef COBALT_WORKER_WORKER_GLOBAL_SCOPE_H_
#define COBALT_WORKER_WORKER_GLOBAL_SCOPE_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/dom/event_target_listener_info.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace cobalt {
namespace worker {

// Implementation of the WorkerGlobalScope common interface.
//   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#dedicated-workers-and-the-workerglobalscope-interface

class WorkerGlobalScope : public dom::EventTarget {
 public:
  explicit WorkerGlobalScope(script::EnvironmentSettings* settings);

  // Web API: WorkerGlobalScope
  //
  scoped_refptr<WorkerGlobalScope> self() { return this; }

  void ImportScripts(const std::vector<std::string>& urls) {}

  virtual void Initialize(const std::string& content) = 0;

  void InitializeURL(const std::string& url);

  const GURL Url() const { return url_; }

  const dom::EventTargetListenerInfo::EventListenerScriptValue*
  onlanguagechange() {
    return GetAttributeEventListener(base::Tokens::languagechange());
  }
  void set_onlanguagechange(
      const dom::EventTargetListenerInfo::EventListenerScriptValue&
          event_listener) {
    SetAttributeEventListener(base::Tokens::languagechange(), event_listener);
  }

  const dom::EventTargetListenerInfo::EventListenerScriptValue*
  onrejectionhandled() {
    return GetAttributeEventListener(base::Tokens::rejectionhandled());
  }
  void set_onrejectionhandled(
      const dom::EventTargetListenerInfo::EventListenerScriptValue&
          event_listener) {
    SetAttributeEventListener(base::Tokens::rejectionhandled(), event_listener);
  }
  const dom::EventTargetListenerInfo::EventListenerScriptValue*
  onunhandledrejection() {
    return GetAttributeEventListener(base::Tokens::unhandledrejection());
  }
  void set_onunhandledrejection(
      const dom::EventTargetListenerInfo::EventListenerScriptValue&
          event_listener) {
    SetAttributeEventListener(base::Tokens::unhandledrejection(),
                              event_listener);
  }

  DEFINE_WRAPPABLE_TYPE(WorkerGlobalScope);

 protected:
  virtual ~WorkerGlobalScope() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WorkerGlobalScope);

  // WorkerGlobalScope Attribute
  // https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#concept-workerglobalscope
  GURL url_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_WORKER_GLOBAL_SCOPE_H_
