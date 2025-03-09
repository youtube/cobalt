// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/gpu/shell_content_gpu_client.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/service/display/starboard/overlay_strategy_underlay_starboard.h"
#include "content/shell/common/power_monitor_test_impl.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "content/public/child/child_thread.h"

namespace content {

ShellContentGpuClient::ShellContentGpuClient() = default;

ShellContentGpuClient::~ShellContentGpuClient() = default;

void ShellContentGpuClient::ExposeInterfacesToBrowser(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    mojo::BinderMap* binders) {
  binders->Add<mojom::PowerMonitorTest>(
      base::BindRepeating(&PowerMonitorTestImpl::MakeSelfOwnedReceiver),
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

void ShellContentGpuClient::PostCompositorThreadCreated(
  base::SingleThreadTaskRunner* task_runner) {
  // Initialize PendingRemote for VideoGeometrySetter and post it
  // to compositor thread (viz service). This is called on gpu thread
  // right after the compositor thread is created.
  mojo::PendingRemote<cobalt::media::mojom::VideoGeometrySetter>
      video_geometry_setter;
  content::ChildThread::Get()->BindHostReceiver(
      video_geometry_setter.InitWithNewPipeAndPassReceiver());

  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &viz::OverlayStrategyUnderlayStarboard::ConnectVideoGeometrySetter,
          std::move(video_geometry_setter)));
}

}  // namespace content
