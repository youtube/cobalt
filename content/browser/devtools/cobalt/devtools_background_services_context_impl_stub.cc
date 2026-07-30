// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "content/browser/devtools/cobalt/devtools_background_services_context_impl_stub.h"

namespace content {

DevToolsBackgroundServicesContextImpl::DevToolsBackgroundServicesContextImpl(
    BrowserContext* browser_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context) {}

DevToolsBackgroundServicesContextImpl::
    ~DevToolsBackgroundServicesContextImpl() = default;

bool DevToolsBackgroundServicesContextImpl::IsRecording(
    DevToolsBackgroundService service) {
  return false;
}

void DevToolsBackgroundServicesContextImpl::LogBackgroundServiceEvent(
    uint64_t service_worker_registration_id,
    blink::StorageKey storage_key,
    DevToolsBackgroundService service,
    const std::string& event_name,
    const std::string& instance_id,
    const std::map<std::string, std::string>& event_metadata) {}

}  // namespace content
