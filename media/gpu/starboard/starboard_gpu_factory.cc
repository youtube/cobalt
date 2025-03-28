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

#include "media/gpu/starboard/starboard_gpu_factory.h"

namespace media {
namespace {

bool MakeContextCurrent(gpu::CommandBufferStub* stub) {
  return stub && stub->decoder_context()->MakeCurrent();
}

}  // namespace

StarboardGpuFactory::StarboardGpuFactory(GetStubCB get_stub_cb) {
  DETACH_FROM_THREAD(thread_checker_);
  get_stub_cb_ = std::move(get_stub_cb);
}

StarboardGpuFactory::~StarboardGpuFactory() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (stub_) {
    LOG(ERROR) << "Cobalt: " << __func__;
    stub_->RemoveDestructionObserver(this);
  }
}

void StarboardGpuFactory::Initialize(base::UnguessableToken channel_token,
                                     int32_t route_id,
                                     base::WaitableEvent* done_event) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  stub_ = get_stub_cb_.Run(channel_token, route_id);
  if (stub_) {
    LOG(ERROR) << "Cobalt: " << __func__;
    stub_->AddDestructionObserver(this);
  }
  LOG(ERROR) << "Cobalt: " << __func__;
  done_event->Signal();
  LOG(ERROR) << "Cobalt: " << __func__;
}

void StarboardGpuFactory::RunTaskOnGpuThread(
    SbDecodeTargetGlesContextRunnerTarget target_function,
    void* target_function_context,
    base::WaitableEvent* done_event) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!MakeContextCurrent(stub_)) {
    LOG(ERROR) << "Cobalt: " << __func__;
    return;
  }
  LOG(ERROR) << "Cobalt: " << __func__;
  target_function(target_function_context);
  LOG(ERROR) << "Cobalt: " << __func__;
  done_event->Signal();
  LOG(ERROR) << "Cobalt: " << __func__;
}

void StarboardGpuFactory::RunTaskOnGpuThreadv2(
    base::RepeatingCallback<void()> callback,
    base::WaitableEvent* done_event) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!MakeContextCurrent(stub_)) {
    LOG(ERROR) << "Cobalt: " << __func__;
    return;
  }
  LOG(ERROR) << "Cobalt: " << __func__;
  std::move(callback).Run();
  LOG(ERROR) << "Cobalt: " << __func__;
  done_event->Signal();
  LOG(ERROR) << "Cobalt: " << __func__;
}

void StarboardGpuFactory::OnWillDestroyStub(bool have_context) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(stub_);
  stub_ = nullptr;
}

}  // namespace media
