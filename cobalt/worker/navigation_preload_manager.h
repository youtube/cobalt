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

#ifndef COBALT_WORKER_NAVIGATION_PRELOAD_MANAGER_H_
#define COBALT_WORKER_NAVIGATION_PRELOAD_MANAGER_H_

#include <memory>
#include <vector>

#include "cobalt/script/exception_state.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/worker/navigation_preload_state.h"


namespace cobalt {
namespace worker {

class ServiceWorkerRegistration;

class NavigationPreloadManager final : public script::Wrappable {
 public:
  using NavigationPreloadStatePromise =
      script::Promise<scoped_refptr<script::Wrappable>>;
  NavigationPreloadManager(ServiceWorkerRegistration* registration,
                           script::ScriptValueFactory* script_value_factory);

  script::Handle<script::Promise<void>> Enable();
  script::Handle<script::Promise<void>> Disable();
  script::Handle<script::Promise<void>> SetHeaderValue(
      std::vector<uint8_t> value);
  script::Handle<NavigationPreloadStatePromise> GetState();

  DEFINE_WRAPPABLE_TYPE(NavigationPreloadManager);

 private:
  ServiceWorkerRegistration* registration_;
  script::Handle<script::Promise<void>> SetEnabled(bool enable);
  script::ScriptValueFactory* script_value_factory_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_NAVIGATION_PRELOAD_MANAGER_H_
