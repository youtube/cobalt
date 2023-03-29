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
#include "cobalt/web/error_event.h"
#include "cobalt/web/error_event_init.h"
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

  environment_settings()
      ->context()
      ->global_environment()
      ->SetReportEvalCallback(base::Bind(&web::CspDelegate::ReportEval,
                                         base::Unretained(csp_delegate())));

  environment_settings()
      ->context()
      ->global_environment()
      ->SetReportErrorCallback(
          base::Bind(&WindowOrWorkerGlobalScope::ReportScriptError,
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

bool WindowOrWorkerGlobalScope::ReportScriptError(
    const script::ErrorReport& error_report) {
  // Runtime script errors: when the user agent is required to report an error
  // for a particular script, it must run these steps, after which the error is
  // either handled or not handled:
  //   https://www.w3.org/TR/html50/webappapis.html#runtime-script-errors

  // 1. If target is in error reporting mode, then abort these steps; the error
  //    is not handled.
  if (is_reporting_script_error_) {
    return false;
  }

  // 2. Let target be in error reporting mode.
  is_reporting_script_error_ = true;

  // 7. Let event be a new trusted ErrorEvent object that does not bubble but is
  //    cancelable, and which has the event name error.
  // NOTE: Cobalt does not currently support trusted events.
  web::ErrorEventInit error;
  error.set_bubbles(false);
  error.set_cancelable(true);

  if (error_report.is_muted) {
    // 6. If script has muted errors, then set message to "Script error.", set
    //    location to the empty string, set line and col to 0, and set error
    //    object to null.
    error.set_message("Script error.");
    error.set_filename("");
    error.set_lineno(0);
    error.set_colno(0);
    error.set_error(NULL);
  } else {
    // 8. Initialize event's message attribute to message.
    error.set_message(error_report.message);
    // 9. Initialize event's filename attribute to location.
    error.set_filename(error_report.filename);
    // 10. Initialize event's lineno attribute to line.
    error.set_lineno(error_report.line_number);
    // 11. Initialize event's colno attribute to col.
    error.set_colno(error_report.column_number);
    // 12. Initialize event's error attribute to error object.
    error.set_error(error_report.error ? error_report.error.get() : NULL);
  }

  scoped_refptr<web::ErrorEvent> error_event(
      new web::ErrorEvent(environment_settings(), error));

  // 13. Dispatch event at target.
  DispatchEvent(error_event);

  // 14. Let target no longer be in error reporting mode.
  is_reporting_script_error_ = false;

  // 15. If event was canceled, then the error is handled. Otherwise, the error
  //     is not handled.
  return error_event->default_prevented();
}

}  // namespace web
}  // namespace cobalt
