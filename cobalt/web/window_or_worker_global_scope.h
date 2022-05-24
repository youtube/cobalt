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
#include "cobalt/web/event_target.h"
#include "cobalt/web/event_target_listener_info.h"

namespace cobalt {
namespace web {

// Implementation of the logic common to both Window WorkerGlobalScope
// interfaces.
class WindowOrWorkerGlobalScope : public EventTarget {
 public:
  explicit WindowOrWorkerGlobalScope(script::EnvironmentSettings* settings);
  WindowOrWorkerGlobalScope(const WindowOrWorkerGlobalScope&) = delete;
  WindowOrWorkerGlobalScope& operator=(const WindowOrWorkerGlobalScope&) =
      delete;

  DEFINE_WRAPPABLE_TYPE(WindowOrWorkerGlobalScope);

  bool IsWindow();
  bool IsDedicatedWorker();
  bool IsServiceWorker();

 protected:
  virtual ~WindowOrWorkerGlobalScope() {}

 private:
};

}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_WINDOW_OR_WORKER_GLOBAL_SCOPE_H_
