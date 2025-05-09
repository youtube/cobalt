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

#include "media/mojo/services/starboard/starboard_renderer_wrapper.h"

#include <utility>

#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "media/base/starboard/starboard_rendering_mode.h"
#include "media/mojo/services/mojo_media_log.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "gpu/command_buffer/service/abstract_texture_android.h"
#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/config/gpu_finch_features.h"
#include "media/base/media_switches.h"
#include "media/gpu/android/direct_shared_image_video_provider.h"
#include "media/gpu/android/pooled_shared_image_video_provider.h"
#include "media/gpu/starboard/starboard_gpu_factory.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace media {

namespace {
#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<gpu::AbstractTextureAndroid> CreateTexture(
    gpu::SharedContextState* context_state) {
  DCHECK(context_state);

  gpu::gles2::FeatureInfo* feature_info = context_state->feature_info();
  if (feature_info && feature_info->is_passthrough_cmd_decoder()) {
    return gpu::AbstractTextureAndroid::CreateForPassthrough(gfx::Size());
  }

  return gpu::AbstractTextureAndroid::CreateForValidating(gfx::Size());
}

// Run on the GPU main thread to allocate the texture owner, and return it
// via |init_cb|.
static void AllocateTextureOwnerOnGpuThread(
    StarboardRendererWrapper::InitCB init_cb,
    base::UnguessableToken channel_token,
    int32_t route_id,
    scoped_refptr<gpu::SharedContextState> shared_context_state) {
  if (!shared_context_state) {
    return;
  }

  std::move(init_cb).Run(
      StarboardRendererWrapper::CreateTextureOwner(shared_context_state),
      channel_token, route_id);
}
#endif  // BUILDFLAG(IS_ANDROID)
}  // namespace

StarboardRendererWrapper::StarboardRendererWrapper(
    StarboardRendererTraits& traits)
    : renderer_extension_receiver_(
          this,
          std::move(traits.renderer_extension_receiver)),
      client_extension_remote_(std::move(traits.client_extension_remote),
                               traits.task_runner),
      renderer_(std::make_unique<StarboardRenderer>(
          std::move(traits.task_runner),
          std::make_unique<MojoMediaLog>(std::move(traits.media_log_remote),
                                         traits.task_runner),
          traits.overlay_plane_id,
          traits.audio_write_duration_local,
          traits.audio_write_duration_remote)),
#if BUILDFLAG(IS_ANDROID)
      get_starboard_command_buffer_stub_cb_(
          traits.get_starboard_command_buffer_stub_cb),
      gpu_factory_(traits.gpu_task_runner,
                   traits.get_starboard_command_buffer_stub_cb),
#endif  // BUILDFLAG(IS_ANDROID)
      gpu_task_runner_(std::move(traits.gpu_task_runner)) {
  DETACH_FROM_THREAD(thread_checker_);
#if BUILDFLAG(IS_ANDROID)
  if (features::NeedThreadSafeAndroidMedia()) {
    ref_counted_lock_ = base::MakeRefCounted<gpu::RefCountedLock>();
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

StarboardRendererWrapper::~StarboardRendererWrapper() {
#if BUILDFLAG(IS_ANDROID)
  if (texture_native_id_) {
    base::WaitableEvent done_event(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    gpu_factory_.AsyncCall(&StarboardGpuFactory::RunDeleteTexturesOnGpu)
        .WithArgs(texture_native_id_, &done_event);
    // Blocking is okay here because it ensures TextureOwner is released
    // before StarboardRendererWrapper() is deconstructed.
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    done_event.Wait();
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

void StarboardRendererWrapper::Initialize(MediaResource* media_resource,
                                          RendererClient* client,
                                          PipelineStatusCallback init_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // OnGpuChannelTokenReady() is called before Initialize()
  // in StarboardRendererClient, so it is safe to access
  // |command_buffer_id_| for posting gpu tasks.
  if (command_buffer_id_) {
#if BUILDFLAG(IS_ANDROID)
    auto get_command_buffer_stub_cb = base::BindRepeating(
        get_starboard_command_buffer_stub_cb_,
        command_buffer_id_->channel_token, command_buffer_id_->route_id);

    image_provider_ = std::make_unique<DirectSharedImageVideoProvider>(
        gpu_task_runner_, get_command_buffer_stub_cb, ref_counted_lock_);

    if (base::FeatureList::IsEnabled(kUsePooledSharedImageVideoProvider)) {
      // Wrap |image_provider| in a pool.
      image_provider_ = PooledSharedImageVideoProvider::Create(
          gpu_task_runner_, get_command_buffer_stub_cb,
          std::move(image_provider_), ref_counted_lock_);
    }

    auto gpu_init_cb = base::BindOnce(
        &AllocateTextureOwnerOnGpuThread,
        base::BindPostTaskToCurrentDefault(base::BindRepeating(
            &StarboardRendererWrapper::OnTextureOwnerInitialized,
            weak_factory_.GetWeakPtr())),
        command_buffer_id_->channel_token, command_buffer_id_->route_id);
    image_provider_->Initialize(std::move(gpu_init_cb));
#endif  // BUILDFLAG(IS_ANDROID)
  }

  renderer_->SetStarboardRendererCallbacks(
      base::BindRepeating(
          &StarboardRendererWrapper::OnPaintVideoHoleFrameByStarboard,
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &StarboardRendererWrapper::OnUpdateStarboardRenderingModeByStarboard,
          weak_factory_.GetWeakPtr()));

#if BUILDFLAG(IS_ANDROID)
  if (!is_gpu_factory_initialized_) {
    media_resource_ = media_resource;
    client_ = client;
    init_cb_ = std::move(init_cb);
    return;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  renderer_->Initialize(media_resource, client, std::move(init_cb));
}

void StarboardRendererWrapper::Flush(base::OnceClosure flush_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  renderer_->Flush(std::move(flush_cb));
}

void StarboardRendererWrapper::StartPlayingFrom(base::TimeDelta time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  renderer_->StartPlayingFrom(time);
}

void StarboardRendererWrapper::SetPlaybackRate(double playback_rate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  renderer_->SetPlaybackRate(playback_rate);
}

void StarboardRendererWrapper::SetVolume(float volume) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  renderer_->SetVolume(volume);
}

void StarboardRendererWrapper::SetCdm(CdmContext* cdm_context,
                                      CdmAttachedCB cdm_attached_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  renderer_->SetCdm(cdm_context, std::move(cdm_attached_cb));
}

void StarboardRendererWrapper::SetLatencyHint(
    absl::optional<base::TimeDelta> latency_hint) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  renderer_->SetLatencyHint(latency_hint);
}

base::TimeDelta StarboardRendererWrapper::GetMediaTime() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return renderer_->GetMediaTime();
}

RendererType StarboardRendererWrapper::GetRendererType() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return RendererType::kStarboard;
}

void StarboardRendererWrapper::OnVideoGeometryChange(
    const gfx::Rect& output_rect) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  renderer_->OnVideoGeometryChange(output_rect);
}

void StarboardRendererWrapper::OnGpuChannelTokenReady(
    mojom::CommandBufferIdPtr command_buffer_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  command_buffer_id_ = std::move(command_buffer_id);
}

#if BUILDFLAG(IS_ANDROID)
void StarboardRendererWrapper::OnGpuFactoryInitialized() {
  DCHECK(texture_native_id_ != -1);
  is_gpu_factory_initialized_ = true;
  decode_target_graphics_context_provider_.egl_context = &texture_native_id_;
  decode_target_graphics_context_provider_.gles_context_runner_context = this;
  // TODO(b/375070492): set GraphicsContextRunner to
  // decode_target_graphics_context_provider_.gles_context_runner
  renderer_->set_decode_target_graphics_context_provider(base::BindRepeating(
      &StarboardRendererWrapper::GetSbDecodeTargetGraphicsContextProvider,
      base::Unretained(this)));
  if (texture_owner_) {
    // TODO(b/375070492): create |codec_buffer_wait_coordinator_|
    // for CodecOutputBufferRenderer
  }

  if (media_resource_ != nullptr && client_ != nullptr && !init_cb_.is_null()) {
    renderer_->Initialize(media_resource_, client_, std::move(init_cb_));
  }
}

void StarboardRendererWrapper::OnTextureOwnerInitialized(
    scoped_refptr<gpu::TextureOwner> texture_owner,
    base::UnguessableToken channel_token,
    int32_t route_id) {
  if (!texture_owner) {
    LOG(ERROR) << "Could not allocated TextureOwner";
    return;
  }
  texture_owner_ = std::move(texture_owner);

  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  gpu_factory_.AsyncCall(&StarboardGpuFactory::Initialize)
      .WithArgs(channel_token, route_id, texture_owner_->GetTextureId(),
                &texture_native_id_, &done_event)
      .Then(base::BindOnce(&StarboardRendererWrapper::OnGpuFactoryInitialized,
                           weak_factory_.GetWeakPtr()));
  // Blocking is okay here because it ensures TextureOwner is created
  // with valid |texture_native_id_| before OnGpuFactoryInitialized().
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  done_event.Wait();
}

// static
scoped_refptr<gpu::TextureOwner> StarboardRendererWrapper::CreateTextureOwner(
    scoped_refptr<gpu::SharedContextState> context_state) {
  auto texture = CreateTexture(context_state.get());
  return new gpu::StarboardSurfaceTextureGLOwner(std::move(texture),
                                                 std::move(context_state));
}
#endif  // BUILDFLAG(IS_ANDROID)

void StarboardRendererWrapper::OnPaintVideoHoleFrameByStarboard(
    const gfx::Size& size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_extension_remote_->PaintVideoHoleFrame(size);
}

void StarboardRendererWrapper::OnUpdateStarboardRenderingModeByStarboard(
    const StarboardRenderingMode mode) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (mode == StarboardRenderingMode::kDecodeToTexture) {
    DCHECK(command_buffer_id_);
#if BUILDFLAG(IS_ANDROID)
    DCHECK(is_gpu_factory_initialized_);
#endif  // BUILDFLAG(IS_ANDROID)
  }
  client_extension_remote_->UpdateStarboardRenderingMode(mode);
}

SbDecodeTargetGraphicsContextProvider*
StarboardRendererWrapper::GetSbDecodeTargetGraphicsContextProvider() {
  return &decode_target_graphics_context_provider_;
}

}  // namespace media
