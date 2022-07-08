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

#ifndef COBALT_WEB_WINDOW_OR_WORKER_GLOBAL_SCOPE_H_
#define COBALT_WEB_WINDOW_OR_WORKER_GLOBAL_SCOPE_H_

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/web/crypto.h"
#include "cobalt/web/event_target.h"
#include "cobalt/web/event_target_listener_info.h"
#include "cobalt/web/navigator_base.h"
#include "cobalt/web/window_timers.h"

namespace cobalt {
namespace dom {
class Window;
}  // namespace dom
namespace worker {
class DedicatedWorkerGlobalScope;
class ServiceWorkerGlobalScope;
}  // namespace worker
namespace web {

// Implementation of the logic common to both Window WorkerGlobalScope
// interfaces.
class WindowOrWorkerGlobalScope : public EventTarget {
 public:
  explicit WindowOrWorkerGlobalScope(script::EnvironmentSettings* settings,
                                     StatTracker* stat_tracker,
                                     base::ApplicationState initial_state);
  WindowOrWorkerGlobalScope(const WindowOrWorkerGlobalScope&) = delete;
  WindowOrWorkerGlobalScope& operator=(const WindowOrWorkerGlobalScope&) =
      delete;

  void set_navigator_base(NavigatorBase* navigator_base) {
    navigator_base_ = navigator_base;
  }
  NavigatorBase* navigator_base() { return navigator_base_; }

  bool IsWindow();
  bool IsDedicatedWorker();
  bool IsServiceWorker();

  virtual dom::Window* AsWindow() { return nullptr; }
  virtual worker::DedicatedWorkerGlobalScope* AsDedicatedWorker() {
    return nullptr;
  }
  virtual worker::ServiceWorkerGlobalScope* AsServiceWorker() {
    return nullptr;
  }

  DEFINE_WRAPPABLE_TYPE(WindowOrWorkerGlobalScope);

  // Web API: WindowTimers (implements)
  //   https://www.w3.org/TR/html50/webappapis.html#timers
  //
  int SetTimeout(const web::WindowTimers::TimerCallbackArg& handler) {
    return SetTimeout(handler, 0);
  }

  int SetTimeout(const web::WindowTimers::TimerCallbackArg& handler,
                 int timeout);

  void ClearTimeout(int handle);

  int SetInterval(const web::WindowTimers::TimerCallbackArg& handler) {
    return SetInterval(handler, 0);
  }

  int SetInterval(const web::WindowTimers::TimerCallbackArg& handler,
                  int timeout);

  void ClearInterval(int handle);

  void DestroyTimers();

  // Web API: GlobalCrypto (implements)
  //   https://www.w3.org/TR/WebCryptoAPI/#crypto-interface
  scoped_refptr<Crypto> crypto() const;

 protected:
  virtual ~WindowOrWorkerGlobalScope() {}

  scoped_refptr<Crypto> crypto_;
  web::WindowTimers window_timers_;

 private:
  NavigatorBase* navigator_base_ = nullptr;
};

}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_WINDOW_OR_WORKER_GLOBAL_SCOPE_H_
