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

#include "cobalt/worker/worker_settings.h"

#include "base/logging.h"
#include "cobalt/base/debugger_hooks.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/web/context.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/web/window_or_worker_global_scope.h"
#include "cobalt/worker/worker_global_scope.h"
#include "url/gurl.h"

namespace cobalt {
namespace worker {

WorkerSettings::WorkerSettings() : web::EnvironmentSettings() {}

WorkerSettings::WorkerSettings(web::MessagePort* message_port)
    : web::EnvironmentSettings(), message_port_(message_port) {}

const GURL& WorkerSettings::base_url() const {
  // From algorithm for to setup up a worker environment settings object:
  //   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#set-up-a-worker-environment-settings-object
  // 3. Let settings object be a new environment settings object whose
  //    algorithms are defined as follows:
  //    The API base URL
  //    Return worker global scope's url.
  DCHECK(context()->GetWindowOrWorkerGlobalScope()->IsWorker());
  return context()->GetWindowOrWorkerGlobalScope()->AsWorker()->Url();
}

loader::Origin WorkerSettings::GetOrigin() const {
  // From algorithm for to setup up a worker environment settings object:
  //   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#set-up-a-worker-environment-settings-object
  // 3. Let settings object be a new environment settings object whose
  //    algorithms are defined as follows:
  //    The origin
  //    Return a unique opaque origin if worker global scope's url's scheme is
  //    "data", and inherited origin otherwise.
  DCHECK(context()->GetWindowOrWorkerGlobalScope()->IsWorker());
  const GURL& url =
      context()->GetWindowOrWorkerGlobalScope()->AsWorker()->Url();
  // TODO(b/244368134): Replace with url::Origin::CreateUniqueOpaque().
  // Note: This does not have to be specialized for service workers, since
  // their URL can not be a data URL.
  if (url.SchemeIs("data")) return loader::Origin();
  return origin_;
}

}  // namespace worker
}  // namespace cobalt
