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

#ifndef COBALT_WORKER_SERVICE_WORKER_OBJECT_H_
#define COBALT_WORKER_SERVICE_WORKER_OBJECT_H_

#include "cobalt/worker/service_worker_state.h"
#include "url/gurl.h"

namespace cobalt {
namespace worker {

// This class represents the 'service worker'.
//   https://w3c.github.io/ServiceWorker/#dfn-service-worker
// Not to be confused with the ServiceWorker JavaScript object,  this represents
// the service worker in the browser, independent from the JavaScript realm.
// The lifetime of a service worker is tied to the execution lifetime of events
// and not references held by service worker clients to the ServiceWorker
// object. A user agent may terminate service workers at any time it has no
// event to handle, or detects abnormal operation.
//   https://w3c.github.io/ServiceWorker/#service-worker-lifetime
class ServiceWorkerObject {
 public:
  ServiceWorkerObject();
  ~ServiceWorkerObject() {}

  ServiceWorkerState state() { return state_; }

 private:
  ServiceWorkerState state_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_SERVICE_WORKER_OBJECT_H_
