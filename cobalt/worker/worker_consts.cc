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

#include "cobalt/worker/worker_consts.h"

namespace cobalt {
namespace worker {
const char WorkerConsts::kServiceWorkerRegisterBadMIMEError[] =
    "Service Worker Register failed: The script has an unsupported MIME type "
    "('%s').";

const char WorkerConsts::kServiceWorkerRegisterNoMIMEError[] =
    "Service Worker Register failed: The script does not have a MIME type.";

const char WorkerConsts::kServiceWorkerRegisterScriptOriginNotSameError[] =
    "Service Worker Register failed: Script URL ('%s') and referrer ('%s') "
    "origin are not the same.";

const char WorkerConsts::kServiceWorkerRegisterScopeOriginNotSameError[] =
    "Service Worker Register failed: Scope URL ('%s') and referrer ('%s') "
    "origin are not the same.";

const char WorkerConsts::kServiceWorkerRegisterBadScopeError[] =
    "Service Worker Register failed: Scope ('%s') is not allowed.";

const char WorkerConsts::kServiceWorkerUnregisterScopeOriginNotSameError[] =
    "Service Worker Unregister failed: Scope origin does not match.";

const char WorkerConsts::kServiceWorkerAllowed[] = "Service-Worker-Allowed";

const char WorkerConsts::kSettingsJson[] = "service_worker_settings.json";

const char* const WorkerConsts::kJavaScriptMimeTypes[16] = {
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

const char WorkerConsts::kServiceWorkerName[] = "ServiceWorker";

const char WorkerConsts::kDedicatedWorkerName[] = "DedicatedWorker";
}  // namespace worker
}  // namespace cobalt
