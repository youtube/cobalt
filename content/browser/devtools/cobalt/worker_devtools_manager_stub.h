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

#ifndef CONTENT_BROWSER_DEVTOOLS_COBALT_WORKER_DEVTOOLS_MANAGER_STUB_H_
#define CONTENT_BROWSER_DEVTOOLS_COBALT_WORKER_DEVTOOLS_MANAGER_STUB_H_

#include "base/memory/scoped_refptr.h"
#include "content/public/browser/global_routing_id.h"

namespace content {

class DedicatedWorkerDevToolsAgentHost;
class DedicatedWorkerHost;
class DevToolsThrottleHandle;

class WorkerDevToolsManager {
 public:
  static WorkerDevToolsManager& GetInstance() {
    static WorkerDevToolsManager instance;
    return instance;
  }
  DedicatedWorkerDevToolsAgentHost* GetDevToolsHost(
      const DedicatedWorkerHost* host) {
    return nullptr;
  }
  void WorkerCreated(
      const DedicatedWorkerHost* host,
      int process_id,
      const GlobalRenderFrameHostId& ancestor_render_frame_host_id,
      scoped_refptr<DevToolsThrottleHandle> throttle_handle) {}
  void WorkerDestroyed(const DedicatedWorkerHost* host) {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_COBALT_WORKER_DEVTOOLS_MANAGER_STUB_H_
