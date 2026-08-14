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

#include "cobalt/gpu/cobalt_content_gpu_client.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "cobalt/media/service/mojom/video_geometry_setter.mojom.h"
#include "cobalt/media/service/video_geometry_setter_service.h"
#include "components/viz/service/display/starboard/video_geometry_setter.h"
#include "content/public/child/child_thread.h"

namespace cobalt {

CobaltContentGpuClient::CobaltContentGpuClient()
    : video_geometry_setter_service_(
          std::unique_ptr<cobalt::media::VideoGeometrySetterService,
                          base::OnTaskRunnerDeleter>(
              nullptr,
              base::OnTaskRunnerDeleter(nullptr))) {}

CobaltContentGpuClient::~CobaltContentGpuClient() = default;

void CobaltContentGpuClient::PostCompositorThreadCreated(
    base::SingleThreadTaskRunner* task_runner) {
  if (!video_geometry_setter_service_) {
    CreateVideoGeometrySetterService();
  }
  // Initialize PendingRemote for VideoGeometrySetter and post it
  // to compositor thread (viz service). This is called on gpu thread
  // right after the compositor thread is created.
  mojo::PendingRemote<cobalt::media::mojom::VideoGeometrySetter>
      video_geometry_setter;
  mojo::PendingReceiver<cobalt::media::mojom::VideoGeometrySetter>
      pending_receiver = video_geometry_setter.InitWithNewPipeAndPassReceiver();
  video_geometry_setter_service_->GetVideoGeometrySetter(
      std::move(pending_receiver));

  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&viz::ConnectVideoGeometrySetter,
                                       std::move(video_geometry_setter)));
}

media::VideoGeometrySetterService*
CobaltContentGpuClient::GetVideoGeometrySetterService() {
  if (!video_geometry_setter_service_) {
    CreateVideoGeometrySetterService();
  }
  return video_geometry_setter_service_.get();
}

void CobaltContentGpuClient::CreateVideoGeometrySetterService() {
  DCHECK(!video_geometry_setter_service_);
  video_geometry_setter_service_ =
      std::unique_ptr<cobalt::media::VideoGeometrySetterService,
                      base::OnTaskRunnerDeleter>(
          new media::VideoGeometrySetterService,
          base::OnTaskRunnerDeleter(
              base::SingleThreadTaskRunner::GetCurrentDefault()));
}

}  // namespace cobalt
