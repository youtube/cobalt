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

#ifndef COBALT_GPU_COBALT_CONTENT_GPU_CLIENT_H_
#define COBALT_GPU_COBALT_CONTENT_GPU_CLIENT_H_

#include "base/task/single_thread_task_runner.h"
#include "content/public/gpu/content_gpu_client.h"

namespace cobalt {

namespace media {
class VideoGeometrySetterService;
}  // namespace media

// This class utilizes embedder API for participating in gpu logic.
// It allows Cobalt to interact with viz service and compositor thread.
class CobaltContentGpuClient : public content::ContentGpuClient {
 public:
  CobaltContentGpuClient();

  CobaltContentGpuClient(const CobaltContentGpuClient&) = delete;
  CobaltContentGpuClient& operator=(const CobaltContentGpuClient&) = delete;

  ~CobaltContentGpuClient() override;

  // content::ContentGpuClient:
  void PostCompositorThreadCreated(
      base::SingleThreadTaskRunner* task_runner) override;
  media::VideoGeometrySetterService* GetVideoGeometrySetterService() override;

 private:
  void CreateVideoGeometrySetterService();

  std::unique_ptr<media::VideoGeometrySetterService, base::OnTaskRunnerDeleter>
      video_geometry_setter_service_;
};

}  // namespace cobalt

#endif  // COBALT_GPU_COBALT_CONTENT_GPU_CLIENT_H_
