// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_WORKER_SERVICE_WORKER_CONSTS_H_
#define COBALT_WORKER_SERVICE_WORKER_CONSTS_H_

namespace cobalt {
namespace worker {

struct ServiceWorkerConsts {
  // Constants for error messages.
  static const char kServiceWorkerRegisterBadMIMEError[];
  static const char kServiceWorkerRegisterNoMIMEError[];
  static const char kServiceWorkerRegisterScriptOriginNotSameError[];
  static const char kServiceWorkerRegisterScopeOriginNotSameError[];
  static const char kServiceWorkerRegisterBadScopeError[];
  static const char kServiceWorkerUnregisterScopeOriginNotSameError[];
  static const char kServiceWorkerAllowed[];

  // Constants for ServiceWorkerPersistentSettings
  static const char kSettingsJson[];
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_SERVICE_WORKER_CONSTS_H_
