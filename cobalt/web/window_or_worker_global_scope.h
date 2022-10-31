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

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "cobalt/loader/cors_preflight_cache.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/web/cache_storage.h"
#include "cobalt/web/crypto.h"
#include "cobalt/web/csp_delegate.h"
#include "cobalt/web/csp_delegate_type.h"
#include "cobalt/web/event_target.h"
#include "cobalt/web/event_target_listener_info.h"
#include "cobalt/web/navigator_base.h"
#include "cobalt/web/window_timers.h"

namespace cobalt {
namespace dom {
class Window;
}  // namespace dom
namespace worker {
class WorkerGlobalScope;
class DedicatedWorkerGlobalScope;
class ServiceWorkerGlobalScope;
}  // namespace worker
namespace web {

// Implementation of the logic common to both Window WorkerGlobalScope
// interfaces.
class WindowOrWorkerGlobalScope : public EventTarget {
 public:
  struct Options {
    explicit Options(base::ApplicationState initial_state)
        : initial_state(initial_state),
          csp_enforcement_mode(web::kCspEnforcementEnable) {}

    Options(base::ApplicationState initial_state,
            const network_bridge::PostSender& post_sender,
            csp::CSPHeaderPolicy require_csp,
            web::CspEnforcementType csp_enforcement_mode,
            base::Closure csp_policy_changed_callback,
            int csp_insecure_allowed_token = 0)
        : initial_state(initial_state),
          post_sender(post_sender),
          require_csp(require_csp),
          csp_enforcement_mode(csp_enforcement_mode),
          csp_policy_changed_callback(csp_policy_changed_callback),
          csp_insecure_allowed_token(csp_insecure_allowed_token) {}

    base::ApplicationState initial_state;
    network_bridge::PostSender post_sender;
    csp::CSPHeaderPolicy require_csp;
    web::CspEnforcementType csp_enforcement_mode;
    base::Closure csp_policy_changed_callback;
    int csp_insecure_allowed_token;
  };

  explicit WindowOrWorkerGlobalScope(script::EnvironmentSettings* settings,
                                     StatTracker* stat_tracker,
                                     Options options);
  WindowOrWorkerGlobalScope(const WindowOrWorkerGlobalScope&) = delete;
  WindowOrWorkerGlobalScope& operator=(const WindowOrWorkerGlobalScope&) =
      delete;

  void set_navigator_base(NavigatorBase* navigator_base) {
    navigator_base_ = navigator_base;
  }
  NavigatorBase* navigator_base() { return navigator_base_; }

  bool IsWindow();
  bool IsWorker();
  bool IsDedicatedWorker();
  bool IsServiceWorker();

  virtual dom::Window* AsWindow() { return nullptr; }
  virtual worker::WorkerGlobalScope* AsWorker() { return nullptr; }
  virtual worker::DedicatedWorkerGlobalScope* AsDedicatedWorker() {
    return nullptr;
  }
  virtual worker::ServiceWorkerGlobalScope* AsServiceWorker() {
    return nullptr;
  }

  // The CspDelegate gives access to the CSP list of the policy container
  //   https://html.spec.whatwg.org/commit-snapshots/ae3c91103aada3d6d346a6dd3c5356773195fc79/#policy-container
  web::CspDelegate* csp_delegate() const {
    DCHECK(csp_delegate_);
    return csp_delegate_.get();
  }

  const scoped_refptr<loader::CORSPreflightCache> get_preflight_cache() {
    return preflight_cache_;
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

  // Web API: GlobalCacheStorage (implements)
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#cachestorage
  scoped_refptr<CacheStorage> caches() const;

  // Web API: GlobalCrypto (implements)
  //   https://www.w3.org/TR/WebCryptoAPI/#crypto-interface
  scoped_refptr<Crypto> crypto() const;

 protected:
  virtual ~WindowOrWorkerGlobalScope() {}

  scoped_refptr<CacheStorage> caches_;
  scoped_refptr<Crypto> crypto_;
  web::WindowTimers window_timers_;

 private:
  std::unique_ptr<web::CspDelegate> csp_delegate_;
  NavigatorBase* navigator_base_ = nullptr;
  // Global preflight cache.
  scoped_refptr<loader::CORSPreflightCache> preflight_cache_;
};

}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_WINDOW_OR_WORKER_GLOBAL_SCOPE_H_
