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

#ifndef COBALT_WORKER_ABSTRACT_WORKER_H_
#define COBALT_WORKER_ABSTRACT_WORKER_H_

#include "cobalt/script/environment_settings.h"
#include "cobalt/web/event_target.h"
#include "cobalt/web/event_target_listener_info.h"

namespace cobalt {
namespace worker {

// Implementation of Abstract Worker.
//   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#abstractworker

class AbstractWorker {
 public:
  virtual const web::EventTargetListenerInfo::EventListenerScriptValue*
  onerror() const = 0;
  virtual void set_onerror(
      const web::EventTargetListenerInfo::EventListenerScriptValue&
          event_listener) = 0;

 protected:
  virtual ~AbstractWorker() {}
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_ABSTRACT_WORKER_H_
