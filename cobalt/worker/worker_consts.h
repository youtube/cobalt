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

#ifndef COBALT_WORKER_WORKER_CONSTS_H_
#define COBALT_WORKER_WORKER_CONSTS_H_

namespace cobalt {
namespace worker {

namespace WorkerConsts {

// Constants for error messages.
constexpr const char kServiceWorkerRegisterBadMIMEError[] =
    "Service Worker Register failed: The script has an unsupported MIME type "
    "('%s').";
constexpr const char kServiceWorkerRegisterNoMIMEError[] =
    "Service Worker Register failed: The script does not have a MIME type.";
constexpr const char kServiceWorkerRegisterScriptOriginNotSameError[] =
    "Service Worker Register failed: Script URL ('%s') and referrer ('%s') "
    "origin are not the same.";
constexpr const char kServiceWorkerRegisterScopeOriginNotSameError[] =
    "Service Worker Register failed: Scope URL ('%s') and referrer ('%s') "
    "origin are not the same.";
constexpr const char kServiceWorkerRegisterBadScopeError[] =
    "Service Worker Register failed: Scope ('%s') is not allowed.";
constexpr const char kServiceWorkerUnregisterScopeOriginNotSameError[] =
    "Service Worker Unregister failed: Scope origin does not match.";

constexpr const char kServiceWorkerAllowed[] = "Service-Worker-Allowed";

// Constants for ServiceWorkerPersistentSettings
constexpr const char kSettingsJson[] = "service_worker_settings.json";

// Array of JavaScript mime types, according to the MIME Sniffinc spec:
//   https://mimesniff.spec.whatwg.org/#javascript-mime-type
constexpr const char* const kJavaScriptMimeTypes[16] = {
    "application/ecmascript",
    "application/javascript",
    "application/x-ecmascript",
    "application/x-javascript",
    "text/ecmascript",
    "text/javascript",
    "text/javascript1.0",
    "text/javascript1.1",
    "text/javascript1.2",
    "text/javascript1.3",
    "text/javascript1.4",
    "text/javascript1.5",
    "text/jscript",
    "text/livescript",
    "text/x-ecmascript",
    "text/x-javascript"};

// The name of a Service Worker thread.
constexpr const char kServiceWorkerName[] = "ServiceWorker";

// The name of a Service Worker Registry thread.
constexpr const char kServiceWorkerRegistryName[] = "ServiceWorkerRegistry";

// The name of a Dedicated Worker thread.
constexpr const char kDedicatedWorkerName[] = "DedicatedWorker";

};  // namespace WorkerConsts


}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_WORKER_CONSTS_H_
