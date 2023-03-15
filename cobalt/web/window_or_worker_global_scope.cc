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

#include "cobalt/web/window_or_worker_global_scope.h"

#include <utility>

#include "base/bind.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/web/csp_delegate_factory.h"
#include "cobalt/web/event_target.h"

namespace cobalt {
namespace dom {
class Window;
}  // namespace dom
namespace worker {
class DedicatedWorkerGlobalScope;
class ServiceWorkerGlobalScope;
}  // namespace worker
namespace web {
WindowOrWorkerGlobalScope::WindowOrWorkerGlobalScope(
    script::EnvironmentSettings* settings, const Options& options)
    // Global scope object EventTargets require special handling for onerror
    // events, see EventTarget constructor for more details.
    : EventTarget(settings, kUnpackOnErrorEvents),
      options_(options),
      caches_(new CacheStorage()),
      crypto_(new Crypto()),
      window_timers_(this, options.stat_tracker, debugger_hooks(),
                     options.initial_state),
      preflight_cache_(new loader::CORSPreflightCache()) {
  csp_delegate_ = CspDelegateFactory::Create(
      this, options.csp_options,
      base::Bind(&WindowOrWorkerGlobalScope::OnCspPolicyChanged,
                 base::Unretained(this)));
}

WindowOrWorkerGlobalScope::~WindowOrWorkerGlobalScope() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

bool WindowOrWorkerGlobalScope::IsWindow() {
  return GetWrappableType() == base::GetTypeId<dom::Window>();
}

bool WindowOrWorkerGlobalScope::IsWorker() {
  return IsDedicatedWorker() || IsServiceWorker();
}

bool WindowOrWorkerGlobalScope::IsDedicatedWorker() {
  return GetWrappableType() ==
         base::GetTypeId<worker::DedicatedWorkerGlobalScope>();
}

bool WindowOrWorkerGlobalScope::IsServiceWorker() {
  return GetWrappableType() ==
         base::GetTypeId<worker::ServiceWorkerGlobalScope>();
}

void WindowOrWorkerGlobalScope::OnCspPolicyChanged() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(csp_delegate());
  DCHECK(environment_settings()->context()->global_environment());

  std::string eval_disabled_message;
  bool allow_eval = csp_delegate()->AllowEval(&eval_disabled_message);
  if (allow_eval) {
    environment_settings()->context()->global_environment()->EnableEval();
  } else {
    environment_settings()->context()->global_environment()->DisableEval(
        eval_disabled_message);
  }
}

int WindowOrWorkerGlobalScope::SetTimeout(
    const WindowTimers::TimerCallbackArg& handler, int timeout) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return window_timers_.SetTimeout(handler, timeout);
}

void WindowOrWorkerGlobalScope::ClearTimeout(int handle) {
  window_timers_.ClearTimeout(handle);
}

int WindowOrWorkerGlobalScope::SetInterval(
    const WindowTimers::TimerCallbackArg& handler, int timeout) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return window_timers_.SetInterval(handler, timeout);
}

void WindowOrWorkerGlobalScope::ClearInterval(int handle) {
  window_timers_.ClearInterval(handle);
}

void WindowOrWorkerGlobalScope::DestroyTimers() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  window_timers_.DisableCallbacks();
}

scoped_refptr<CacheStorage> WindowOrWorkerGlobalScope::caches() const {
  return caches_;
}

scoped_refptr<Crypto> WindowOrWorkerGlobalScope::crypto() const {
  return crypto_;
}

}  // namespace web
}  // namespace cobalt
