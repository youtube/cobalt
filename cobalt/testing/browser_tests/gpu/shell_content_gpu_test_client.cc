// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/testing/browser_tests/gpu/shell_content_gpu_test_client.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "cobalt/media/service/mojom/video_geometry_setter.mojom.h"
#include "cobalt/media/service/video_geometry_setter_service.h"
#include "cobalt/testing/browser_tests/common/power_monitor_test_impl.h"
#include "components/viz/service/display/starboard/video_geometry_setter.h"
#include "content/public/child/child_thread.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

ShellContentGpuTestClient::ShellContentGpuTestClient()
    : video_geometry_setter_service_(
          std::unique_ptr<cobalt::media::VideoGeometrySetterService,
                          base::OnTaskRunnerDeleter>(
              nullptr,
              base::OnTaskRunnerDeleter(nullptr))) {}

ShellContentGpuTestClient::~ShellContentGpuTestClient() = default;

void ShellContentGpuTestClient::ExposeInterfacesToBrowser(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    mojo::BinderMap* binders) {
  binders->Add<mojom::PowerMonitorTest>(
      base::BindRepeating(&PowerMonitorTestImpl::MakeSelfOwnedReceiver),
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

void ShellContentGpuTestClient::PostCompositorThreadCreated(
    base::SingleThreadTaskRunner* task_runner) {
  mojo::PendingRemote<cobalt::media::mojom::VideoGeometrySetter>
      video_geometry_setter;
  content::ChildThread::Get()->BindHostReceiver(
      video_geometry_setter.InitWithNewPipeAndPassReceiver());

  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&viz::ConnectVideoGeometrySetter,
                                       std::move(video_geometry_setter)));
}

cobalt::media::VideoGeometrySetterService*
ShellContentGpuTestClient::GetVideoGeometrySetterService() {
  if (!video_geometry_setter_service_) {
    CreateVideoGeometrySetterService();
  }
  return video_geometry_setter_service_.get();
}

void ShellContentGpuTestClient::CreateVideoGeometrySetterService() {
  DCHECK(!video_geometry_setter_service_);
  video_geometry_setter_service_ =
      std::unique_ptr<cobalt::media::VideoGeometrySetterService,
                      base::OnTaskRunnerDeleter>(
          new cobalt::media::VideoGeometrySetterService,
          base::OnTaskRunnerDeleter(
              base::SingleThreadTaskRunner::GetCurrentDefault()));
}

}  // namespace content
