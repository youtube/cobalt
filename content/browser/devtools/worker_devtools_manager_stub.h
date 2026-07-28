// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_MANAGER_STUB_H_
#define CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_MANAGER_STUB_H_

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


#endif  // CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_MANAGER_STUB_H_
