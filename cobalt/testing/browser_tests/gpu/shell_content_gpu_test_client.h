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

#ifndef COBALT_TESTING_BROWSER_TESTS_GPU_SHELL_CONTENT_GPU_TEST_CLIENT_H_
#define COBALT_TESTING_BROWSER_TESTS_GPU_SHELL_CONTENT_GPU_TEST_CLIENT_H_

#include "content/public/gpu/content_gpu_client.h"
#include "services/network/public/mojom/network_service_test.mojom-forward.h"

namespace content {

class ShellContentGpuTestClient : public ContentGpuClient {
 public:
  ShellContentGpuTestClient();

  ShellContentGpuTestClient(const ShellContentGpuTestClient&) = delete;
  ShellContentGpuTestClient& operator=(const ShellContentGpuTestClient&) =
      delete;

  ~ShellContentGpuTestClient() override;

  // ContentGpuClient:
  void ExposeInterfacesToBrowser(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      mojo::BinderMap* binders) override;

  void PostCompositorThreadCreated(
      base::SingleThreadTaskRunner* task_runner) override;
};

}  // namespace content

#endif  // COBALT_TESTING_BROWSER_TESTS_GPU_SHELL_CONTENT_GPU_TEST_CLIENT_H_
