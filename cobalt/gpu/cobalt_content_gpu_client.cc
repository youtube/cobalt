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
#include "components/viz/service/display/starboard/video_geometry_setter.h"
#include "content/public/child/child_thread.h"

namespace cobalt {

CobaltContentGpuClient::CobaltContentGpuClient() = default;
CobaltContentGpuClient::~CobaltContentGpuClient() = default;

void CobaltContentGpuClient::PostCompositorThreadCreated(
    base::SingleThreadTaskRunner* task_runner) {
  // Initialize PendingRemote for VideoGeometrySetter and post it
  // to compositor thread (viz service). This is called on gpu thread
  // right after the compositor thread is created.
  mojo::PendingRemote<cobalt::media::mojom::VideoGeometrySetter>
      video_geometry_setter;
  content::ChildThread::Get()->BindHostReceiver(
      video_geometry_setter.InitWithNewPipeAndPassReceiver());

  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&viz::ConnectVideoGeometrySetter,
                                       std::move(video_geometry_setter)));
}

}  // namespace cobalt
