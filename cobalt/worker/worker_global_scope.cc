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

#include "cobalt/worker/worker_global_scope.h"

#include "cobalt/script/environment_settings.h"
#include "cobalt/web/window_or_worker_global_scope.h"

namespace cobalt {
namespace worker {
WorkerGlobalScope::WorkerGlobalScope(script::EnvironmentSettings* settings)
    : web::WindowOrWorkerGlobalScope(settings),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          window_timers_(this, /*dom_stat_tracker=*/NULL, debugger_hooks(),
                         // TODO (b/233788170): once application state is
                         // available, update this to use the actual state.
                         base::ApplicationState::kApplicationStateStarted)) {}

int WorkerGlobalScope::SetTimeout(
    const dom::WindowTimers::TimerCallbackArg& handler, int timeout) {
  return window_timers_.SetTimeout(handler, timeout);
}

void WorkerGlobalScope::ClearTimeout(int handle) {
  window_timers_.ClearTimeout(handle);
}

int WorkerGlobalScope::SetInterval(
    const dom::WindowTimers::TimerCallbackArg& handler, int timeout) {
  return window_timers_.SetInterval(handler, timeout);
}

void WorkerGlobalScope::ClearInterval(int handle) {
  window_timers_.ClearInterval(handle);
}

void WorkerGlobalScope::DestroyTimers() { window_timers_.DisableCallbacks(); }

}  // namespace worker
}  // namespace cobalt
