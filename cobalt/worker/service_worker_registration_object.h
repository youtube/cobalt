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

#ifndef COBALT_WORKER_SERVICE_WORKER_REGISTRATION_OBJECT_H_
#define COBALT_WORKER_SERVICE_WORKER_REGISTRATION_OBJECT_H_
namespace cobalt {
namespace worker {

// This class represents the 'service worker registration'.
//   https://w3c.github.io/ServiceWorker/#dfn-service-worker-registration
// Not to be confused with the ServiceWorkerRegistration JavaScript object,
// this represents the registration of the service worker in the browser,
// independent from the JavaScript realm. The lifetime of this object is beyond
// that of the ServiceWorkerRegistration JavaScript object(s) that represent
// this object in their service worker clients.
//   https://w3c.github.io/ServiceWorker/#service-worker-registration-lifetime
class ServiceWorkerRegistrationObject {
 public:
  ServiceWorkerRegistrationObject();
  ~ServiceWorkerRegistrationObject() {}
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_SERVICE_WORKER_REGISTRATION_OBJECT_H_
