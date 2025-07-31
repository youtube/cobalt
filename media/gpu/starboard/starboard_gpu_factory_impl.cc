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

#include "media/gpu/starboard/starboard_gpu_factory_impl.h"

#include "base/memory/scoped_refptr.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"
#include "ui/gl/scoped_restore_texture.h"

namespace media {

StarboardGpuFactoryImpl::StarboardGpuFactoryImpl(GetStubCB get_stub_cb)
    : get_stub_cb_(std::move(get_stub_cb)) {
  DETACH_FROM_THREAD(thread_checker_);
}

StarboardGpuFactoryImpl::~StarboardGpuFactoryImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (stub_) {
    stub_->RemoveDestructionObserver(this);
  }
}

void StarboardGpuFactoryImpl::Initialize(base::UnguessableToken channel_token,
                                         int32_t route_id,
                                         base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  stub_ = get_stub_cb_.Run(channel_token, route_id);
  if (stub_) {
    stub_->AddDestructionObserver(this);
  }
  std::move(callback).Run();
}

void StarboardGpuFactoryImpl::OnWillDestroyStub(bool /*have_context*/) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(stub_);
  stub_ = nullptr;
}

}  // namespace media
