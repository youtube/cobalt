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
#include "cobalt/dom/window_timers.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/event_target.h"
#include "cobalt/web/event_target_listener_info.h"
#include "cobalt/web/window_or_worker_global_scope.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace cobalt {
namespace worker {
// Implementation of the WorkerGlobalScope common interface.
//   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#dedicated-workers-and-the-workerglobalscope-interface

class WorkerGlobalScope : public web::WindowOrWorkerGlobalScope {
 public:
  typedef dom::WindowTimers::TimerCallback TimerCallback;

  explicit WorkerGlobalScope(script::EnvironmentSettings* settings);
  WorkerGlobalScope(const WorkerGlobalScope&) = delete;
  WorkerGlobalScope& operator=(const WorkerGlobalScope&) = delete;

  virtual void Initialize() = 0;

  // Web API: WorkerGlobalScope
  //
  scoped_refptr<WorkerGlobalScope> self() { return this; }

  void ImportScripts(const std::vector<std::string>& urls) {}

  void set_url(const GURL& url) { url_ = url; }

  const GURL Url() const { return url_; }

  const web::EventTargetListenerInfo::EventListenerScriptValue*
  onlanguagechange() {
    return GetAttributeEventListener(base::Tokens::languagechange());
  }
  void set_onlanguagechange(
      const web::EventTargetListenerInfo::EventListenerScriptValue&
          event_listener) {
    SetAttributeEventListener(base::Tokens::languagechange(), event_listener);
  }

  const web::EventTargetListenerInfo::EventListenerScriptValue*
  onrejectionhandled() {
    return GetAttributeEventListener(base::Tokens::rejectionhandled());
  }
  void set_onrejectionhandled(
      const web::EventTargetListenerInfo::EventListenerScriptValue&
          event_listener) {
    SetAttributeEventListener(base::Tokens::rejectionhandled(), event_listener);
  }
  const web::EventTargetListenerInfo::EventListenerScriptValue*
  onunhandledrejection() {
    return GetAttributeEventListener(base::Tokens::unhandledrejection());
  }
  void set_onunhandledrejection(
      const web::EventTargetListenerInfo::EventListenerScriptValue&
          event_listener) {
    SetAttributeEventListener(base::Tokens::unhandledrejection(),
                              event_listener);
  }

  // Web API: WindowTimers (implements)
  //   https://www.w3.org/TR/html50/webappapis.html#timers
  //
  int SetTimeout(const dom::WindowTimers::TimerCallbackArg& handler) {
    return SetTimeout(handler, 0);
  }

  int SetTimeout(const dom::WindowTimers::TimerCallbackArg& handler,
                 int timeout);

  void ClearTimeout(int handle);

  int SetInterval(const dom::WindowTimers::TimerCallbackArg& handler) {
    return SetInterval(handler, 0);
  }

  int SetInterval(const dom::WindowTimers::TimerCallbackArg& handler,
                  int timeout);

  void ClearInterval(int handle);

  void DestroyTimers();


  DEFINE_WRAPPABLE_TYPE(WorkerGlobalScope);

 protected:
  virtual ~WorkerGlobalScope() {}

 private:
  // WorkerGlobalScope Attribute
  // https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#concept-workerglobalscope-url
  GURL url_;

  dom::WindowTimers window_timers_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_WORKER_GLOBAL_SCOPE_H_
