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

#ifndef CONTENT_BROWSER_DEVTOOLS_COBALT_DEVTOOLS_BACKGROUND_SERVICES_CONTEXT_IMPL_STUB_H_
#define CONTENT_BROWSER_DEVTOOLS_COBALT_DEVTOOLS_BACKGROUND_SERVICES_CONTEXT_IMPL_STUB_H_

#include "base/memory/scoped_refptr.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/devtools_background_services_context.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

class BrowserContext;
class ServiceWorkerContextWrapper;

class CONTENT_EXPORT DevToolsBackgroundServicesContextImpl
    : public DevToolsBackgroundServicesContext {
 public:
  DevToolsBackgroundServicesContextImpl(
      BrowserContext* browser_context,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);
  ~DevToolsBackgroundServicesContextImpl() override;

  DevToolsBackgroundServicesContextImpl(
      const DevToolsBackgroundServicesContextImpl&) = delete;
  DevToolsBackgroundServicesContextImpl& operator=(
      const DevToolsBackgroundServicesContextImpl&) = delete;

  // DevToolsBackgroundServicesContext overrides:
  bool IsRecording(DevToolsBackgroundService service) override;
  void LogBackgroundServiceEvent(
      uint64_t service_worker_registration_id,
      blink::StorageKey storage_key,
      DevToolsBackgroundService service,
      const std::string& event_name,
      const std::string& instance_id,
      const std::map<std::string, std::string>& event_metadata) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_COBALT_DEVTOOLS_BACKGROUND_SERVICES_CONTEXT_IMPL_STUB_H_
