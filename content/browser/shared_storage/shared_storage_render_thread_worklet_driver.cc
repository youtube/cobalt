// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_render_thread_worklet_driver.h"

#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/common/renderer.mojom.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom.h"

namespace content {

SharedStorageRenderThreadWorkletDriver::SharedStorageRenderThreadWorkletDriver(
    AgentSchedulingGroupHost* agent_scheduling_group_host)
    : agent_scheduling_group_host_(agent_scheduling_group_host) {
  agent_scheduling_group_host_->GetProcess()->AddObserver(this);

  if (!agent_scheduling_group_host_->GetProcess()->AreRefCountsDisabled()) {
    agent_scheduling_group_host_->GetProcess()->IncrementWorkerRefCount();
  }
}

SharedStorageRenderThreadWorkletDriver::
    ~SharedStorageRenderThreadWorkletDriver() {
  // The render process is already destroyed. No further action is needed.
  if (!agent_scheduling_group_host_)
    return;

  agent_scheduling_group_host_->GetProcess()->RemoveObserver(this);

  if (!agent_scheduling_group_host_->GetProcess()->AreRefCountsDisabled()) {
    agent_scheduling_group_host_->GetProcess()->DecrementWorkerRefCount();
  }
}

void SharedStorageRenderThreadWorkletDriver::StartWorkletService(
    mojo::PendingReceiver<blink::mojom::SharedStorageWorkletService>
        pending_receiver) {
  // `StartWorkletService` will be called right after the driver is created when
  // the document is still alive, as the driver is created on-demand on the
  // first worklet operation. Thus, the `agent_scheduling_group_host_` should
  // always be valid at this point.
  DCHECK(agent_scheduling_group_host_);

  agent_scheduling_group_host_->CreateSharedStorageWorkletService(
      std::move(pending_receiver));
}

void SharedStorageRenderThreadWorkletDriver::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  // This could occur when the browser shuts down during the worklet's
  // keep-alive phase, or when the renderer process is terminated. Invalidate
  // the pointers to signal this state change.
  if (agent_scheduling_group_host_->GetProcess() == host) {
    agent_scheduling_group_host_->GetProcess()->RemoveObserver(this);

    // The destruction of RenderProcessHost implies that the
    // AgentSchedulingGroupHost is going to be destroyed as well.
    agent_scheduling_group_host_ = nullptr;
  }
}

}  // namespace content
