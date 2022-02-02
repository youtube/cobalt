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

#ifndef COBALT_WORKER_SERVICE_WORKER_H_
#define COBALT_WORKER_SERVICE_WORKER_H_

#include <memory>
#include <string>
#include <utility>

#include "cobalt/dom/event_target.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/worker/service_worker_state.h"
#include "url/gurl.h"

namespace cobalt {
namespace worker {

class ServiceWorker : public dom::EventTarget {
 public:
  struct Options {
    explicit Options(ServiceWorkerState state, GURL url)
        : state(state), url(std::move(url)) {}
    ServiceWorkerState state;
    GURL url;
  };

  explicit ServiceWorker(script::EnvironmentSettings* settings,
                         const Options& options);
  ~ServiceWorker() override = default;
  std::string script_url() const;
  ServiceWorkerState state() const { return state_; }

  DEFINE_WRAPPABLE_TYPE(ServiceWorker);

 private:
  const GURL url_;
  ServiceWorkerState state_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_SERVICE_WORKER_H_
