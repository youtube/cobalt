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
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"
#include "ui/gl/scoped_restore_texture.h"

#if BUILDFLAG(IS_ANDROID)
#include "gpu/command_buffer/service/shared_image/android_video_image_backing.h"
#include "media/gpu/android/starboard/starboard_codec_image.h"
#else  // BUILDFLAG(IS_ANDROID)
#include "gpu/command_buffer/service/shared_image/starboard/starboard_video_image_backing.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace media {
namespace {

bool MakeContextCurrent(gpu::CommandBufferStub* stub) {
  return stub && stub->decoder_context()->MakeCurrent();
}

}  // namespace

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

void StarboardGpuFactoryImpl::RunSbDecodeTargetFunctionOnGpu(
    SbDecodeTargetGlesContextRunnerTarget target_function,
    void* target_function_context,
    base::WaitableEvent* done_event) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!MakeContextCurrent(stub_)) {
    done_event->Signal();
    return;
  }
  target_function(target_function_context);
  done_event->Signal();
}

void StarboardGpuFactoryImpl::RunCallbackOnGpu(
    base::OnceCallback<void()> callback,
    base::WaitableEvent* done_event) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!MakeContextCurrent(stub_)) {
    done_event->Signal();
    return;
  }
  std::move(callback).Run();

  // Notify ANGLE that the External texture binding has changed
  if (gl::g_current_gl_driver->ext.b_GL_ANGLE_texture_external_update) {
    glInvalidateTextureANGLE(GL_TEXTURE_EXTERNAL_OES);
  }

  done_event->Signal();
}

void StarboardGpuFactoryImpl::CreateImageOnGpu(
    const gfx::Size& coded_size,
    const gfx::ColorSpace& color_space,
    int plane_count,
    std::vector<gpu::Mailbox>& mailboxes,
    std::vector<uint32_t>& texture_service_ids,
#if BUILDFLAG(IS_ANDROID)
    scoped_refptr<gpu::RefCountedLock> drdc_lock,
#endif  // BUILDFLAG(IS_ANDROID)
    base::WaitableEvent* done_event) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!MakeContextCurrent(stub_)) {
    done_event->Signal();
    return;
  }

  gpu::ContextResult result;
  auto shared_context =
      stub_->channel()->gpu_channel_manager()->GetSharedContextState(&result);
  if (result != gpu::ContextResult::kSuccess) {
    done_event->Signal();
    return;
  }

  DCHECK_EQ(plane_count, texture_service_ids.size());
  std::vector<scoped_refptr<gpu::gles2::TexturePassthrough>> textures;
  for (int plane = 0; plane < plane_count; plane++) {
    auto mailbox = gpu::Mailbox::GenerateForSharedImage();
    auto texture = base::MakeRefCounted<gpu::gles2::TexturePassthrough>(
        texture_service_ids[plane], GL_TEXTURE_EXTERNAL_OES);
    mailboxes.push_back(mailbox);
    textures.push_back(texture);
  }
#if BUILDFLAG(IS_ANDROID)
  DCHECK_EQ(mailboxes.size(), 1);
  DCHECK_EQ(textures.size(), 1);
  auto starboard_codec_image = base::MakeRefCounted<StarboardCodecImage>(
      std::move(textures[0]), drdc_lock);
  auto shared_image = gpu::AndroidVideoImageBacking::Create(
      mailboxes[0], coded_size, color_space, kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, std::move(starboard_codec_image),
      std::move(shared_context), std::move(drdc_lock));
#else   // BUILDFLAG(IS_ANDROID)
  auto shared_image = std::make_unique<gpu::StarboardVideoImageBacking>(
      mailboxes[0], coded_size, color_space, kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, std::move(textures), std::move(shared_context));
#endif  // BUILDFLAG(IS_ANDROID)
  DCHECK(stub_->channel()->gpu_channel_manager()->shared_image_manager());
  stub_->channel()->shared_image_stub()->factory()->RegisterBacking(
      std::move(shared_image));
  done_event->Signal();
}

void StarboardGpuFactoryImpl::OnWillDestroyStub(bool /*have_context*/) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(stub_);
  stub_ = nullptr;
}

}  // namespace media
