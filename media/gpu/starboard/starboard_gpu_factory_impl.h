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

#ifndef MEDIA_GPU_STARBOARD_STARBOARD_GPU_FACTORY_IMPL_H_
#define MEDIA_GPU_STARBOARD_STARBOARD_GPU_FACTORY_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/gpu/starboard/starboard_gpu_factory.h"

namespace media {
// Implement StarboardGpuFactory class.
// TODO(b/375070492): wire the rest of the functionality with
// decode-to-texture.
class StarboardGpuFactoryImpl : public StarboardGpuFactory {
 public:
  explicit StarboardGpuFactoryImpl(GetStubCB get_stub_cb);

  StarboardGpuFactoryImpl(const StarboardGpuFactoryImpl&) = delete;
  StarboardGpuFactoryImpl& operator=(const StarboardGpuFactoryImpl&) = delete;

  ~StarboardGpuFactoryImpl() override;

  void Initialize(base::UnguessableToken channel_token,
                  int32_t route_id,
                  base::OnceClosure callback) override;

 private:
  void OnWillDestroyStub(bool have_context) override;

  GetStubCB get_stub_cb_;

  raw_ptr<gpu::CommandBufferStub> stub_ = nullptr;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace media

#endif  // MEDIA_GPU_STARBOARD_STARBOARD_GPU_FACTORY_IMPL_H_
