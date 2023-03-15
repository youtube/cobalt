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
#include "base/threading/thread_checker.h"
#include "cobalt/loader/cors_preflight_cache.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/web/cache_storage.h"
#include "cobalt/web/crypto.h"
#include "cobalt/web/csp_delegate.h"
#include "cobalt/web/event_target.h"
#include "cobalt/web/event_target_listener_info.h"
#include "cobalt/web/navigator_base.h"
#include "cobalt/web/stat_tracker.h"
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
    Options() = default;
    Options(base::ApplicationState initial_state,
            const web::CspDelegate::Options& csp_options,
            StatTracker* stat_tracker = nullptr)
        : initial_state(initial_state),
          csp_options(csp_options),
          stat_tracker(stat_tracker) {}

    base::ApplicationState initial_state = base::kApplicationStateStarted;
    web::CspDelegate::Options csp_options;
    StatTracker* stat_tracker = nullptr;
  };

  explicit WindowOrWorkerGlobalScope(script::EnvironmentSettings* settings,
                                     const Options& options);
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
  CspDelegate* csp_delegate() const {
    DCHECK(csp_delegate_);
    return csp_delegate_.get();
  }

  const scoped_refptr<loader::CORSPreflightCache> get_preflight_cache() {
    return preflight_cache_;
  }

  // Callback from the CspDelegate.
  void OnCspPolicyChanged();

  // Web API: WindowTimers (implements)
  //   https://www.w3.org/TR/html50/webappapis.html#timers
  //
  int SetTimeout(const WindowTimers::TimerCallbackArg& handler) {
    return SetTimeout(handler, 0);
  }

  int SetTimeout(const WindowTimers::TimerCallbackArg& handler, int timeout);

  void ClearTimeout(int handle);

  int SetInterval(const WindowTimers::TimerCallbackArg& handler) {
    return SetInterval(handler, 0);
  }

  int SetInterval(const WindowTimers::TimerCallbackArg& handler, int timeout);

  void ClearInterval(int handle);

  void DestroyTimers();

  // Web API: GlobalCacheStorage (implements)
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#cachestorage
  scoped_refptr<CacheStorage> caches() const;

  // Web API: GlobalCrypto (implements)
  //   https://www.w3.org/TR/WebCryptoAPI/#crypto-interface
  scoped_refptr<Crypto> crypto() const;

  const Options& options() { return options_; }

  DEFINE_WRAPPABLE_TYPE(WindowOrWorkerGlobalScope);

 protected:
  virtual ~WindowOrWorkerGlobalScope();

  Options options_;
  scoped_refptr<CacheStorage> caches_;
  scoped_refptr<Crypto> crypto_;
  WindowTimers window_timers_;

 private:
  std::unique_ptr<CspDelegate> csp_delegate_;
  NavigatorBase* navigator_base_ = nullptr;
  // Global preflight cache.
  scoped_refptr<loader::CORSPreflightCache> preflight_cache_;

  // Thread checker ensures all calls to the WebModule are made from the same
  // thread that it is created in.
  THREAD_CHECKER(thread_checker_);
};

}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_WINDOW_OR_WORKER_GLOBAL_SCOPE_H_
