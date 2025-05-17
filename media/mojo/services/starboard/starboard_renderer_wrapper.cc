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
#include "media/gpu/android/codec_output_buffer_renderer.h"
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

  // TODO(borongchen): run DecodeTarget::CreateOnContextRunner()
  // and get |texture| (i.e., service_id) via |info_.planes[0].texture|,
  // then pass it to AbstractTextureAndroid.

  gpu::gles2::FeatureInfo* feature_info = context_state->feature_info();
  if (feature_info && feature_info->is_passthrough_cmd_decoder()) {
    LOG(ERROR) << "Cobalt: " << __func__;
    return gpu::AbstractTextureAndroid::CreateForPassthrough(gfx::Size());
  }

  LOG(ERROR) << "Cobalt: " << __func__;
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
    LOG(ERROR) << "Cobalt: " << __func__;
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
  // TODO(b/326827007): set |is_decode_to_texture_preferred_| to true
  // to allocate TextureOwner() for secondary player using decode-to-texture
  // mode.
  is_decode_to_texture_preferred_ = true;
#if BUILDFLAG(IS_ANDROID)
  if (is_decode_to_texture_preferred_ &&
      features::NeedThreadSafeAndroidMedia()) {
    ref_counted_lock_ = base::MakeRefCounted<gpu::RefCountedLock>();
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

StarboardRendererWrapper::~StarboardRendererWrapper() {
#if BUILDFLAG(IS_ANDROID)
  LOG(ERROR) << "Cobalt: " << __func__;
  if (texture_native_id_) {
    LOG(ERROR) << "Cobalt: " << __func__;
    base::WaitableEvent done_event(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    LOG(ERROR) << "Cobalt: " << __func__;
    gpu_factory_.AsyncCall(&StarboardGpuFactory::RunDeleteTexturesOnGpu)
        .WithArgs(texture_native_id_, &done_event);
    LOG(ERROR) << "Cobalt: " << __func__;
    // Blocking is okay here because it ensures TextureOwner is released
    // before StarboardRendererWrapper() is deconstructed.
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    done_event.Wait();
    LOG(ERROR) << "Cobalt: " << __func__;
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
  if (is_decode_to_texture_preferred_ && command_buffer_id_) {
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
  if (is_decode_to_texture_preferred_ && !is_gpu_factory_initialized_) {
    LOG(ERROR) << "Cobalt: " << __func__;
    media_resource_ = media_resource;
    client_ = client;
    init_cb_ = std::move(init_cb);
    return;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  LOG(ERROR) << "Cobalt: " << __func__;
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

void StarboardRendererWrapper::Render(RenderCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if BUILDFLAG(IS_ANDROID)
  // Post renderer_->Render() to gpu thread.
  LOG(ERROR) << "Cobalt: " << __func__;
  base::RepeatingCallback<void()> get_current_decode_target_cb =
      base::BindRepeating(&StarboardRendererWrapper::GetCurrentDecodeTarget,
                          base::Unretained(this));
  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  LOG(ERROR) << "Cobalt: " << __func__;
  gpu_factory_.AsyncCall(&StarboardGpuFactory::GetCurrentDecodeTargetOnGpu)
      .WithArgs(std::move(get_current_decode_target_cb), &done_event);
  LOG(ERROR) << "Cobalt: " << __func__;
  if (SbDecodeTargetIsValid(decode_target_)) {
    auto info = std::make_unique<SbDecodeTargetInfo>();
    memset(info.get(), 0, sizeof(SbDecodeTargetInfo));
    CHECK(SbDecodeTargetGetInfo(decode_target_, info.get()));
    auto coded_size = gfx::Size(info.get()->width, info.get()->height);

    // TODO(borongchen): support decode-to-texture on android
    LOG(ERROR) << "Cobalt: " << __func__;
    const SbDecodeTargetInfoPlane& plane = info.get()->planes[0];
    LOG(ERROR) << "Cobalt: " << __func__ << " " << plane.content_region.left
               << " " << plane.content_region.right << " "
               << plane.content_region.top << " " << plane.content_region.bottom
               << " " << info.get()->height << " " << info.get()->width;
    auto visible_rect = gfx::Rect(
        gfx::Point(0, 0),
        gfx::Size(
            std::abs(plane.content_region.right - plane.content_region.left),
            std::abs(plane.content_region.top - plane.content_region.bottom)));
    auto natural_size = visible_rect.size();
    LOG(ERROR) << "Cobalt: " << __func__ << " " << coded_size.ToString() << " "
               << visible_rect.ToString();

    if (image_provider_) {
      LOG(ERROR) << "Cobalt: " << __func__;
      // The current image spec that we'll use to request images.
      SharedImageVideoProvider::ImageSpec image_spec;
      image_spec.coded_size = coded_size;
      image_spec.color_space = gfx::ColorSpace::CreateSRGB();
      auto image_ready_cb = base::BindOnce(
          &StarboardRendererWrapper::CreateVideoFrame_OnImageReady,
          weak_factory_.GetWeakPtr());
      auto cb =
          base::BindOnce(std::move(image_ready_cb), weak_factory_.GetWeakPtr(),
                         coded_size, visible_rect, natural_size);
      image_provider_->RequestImage(std::move(cb), image_spec);
    }
  }
  LOG(ERROR) << "Cobalt: " << __func__;
  // TODO(borongchen): Blocking is okay here because XYZ.
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  done_event.Wait();
  LOG(ERROR) << "Cobalt: " << __func__;
  if (current_frame_) {
    LOG(ERROR) << "Cobalt: " << __func__ << " "
               << current_frame_->AsHumanReadableString();
    std::move(callback).Run(std::move(current_frame_));
    return;
  }
#else   // BUILDFLAG(IS_ANDROID)
  decode_target_ = renderer_->GetSbDecodeTarget();
  if (SbDecodeTargetIsValid(decode_target_)) {
    auto info = std::make_unique<SbDecodeTargetInfo>();
    memset(info.get(), 0, sizeof(SbDecodeTargetInfo));
    CHECK(SbDecodeTargetGetInfo(decode_target_, info.get()));
    auto coded_size = gfx::Size(info.get()->width, info.get()->height);
    // PIXEL_FORMAT_I420: Decoded image from SbPlayer is 4:2:0 and 8bits.
    int uv_height = info.get()->height / 2 + info.get()->height % 2;
    int y_plane_size_in_bytes = info.get()->height * info.get()->y_stride;
    int uv_plane_size_in_bytes = uv_height * info.get()->uv_stride;
    // TODO(borongchen): fix visible_rect
    auto visible_rect = gfx::Rect(gfx::Point(0, 0), coded_size);
    auto natural_size = visible_rect.size();
    auto video_frame = VideoFrame::WrapExternalYuvData(
        PIXEL_FORMAT_I420, coded_size, visible_rect, natural_size,
        info.get()->y_stride, info.get()->uv_stride, info.get()->uv_stride,
        info.get()->pixel_buffer,
        info.get()->pixel_buffer + y_plane_size_in_bytes,
        info.get()->pixel_buffer + y_plane_size_in_bytes +
            uv_plane_size_in_bytes,
        kNoTimestamp);
    // Ensure image data stays alive along enough.
    video_frame->AddDestructionObserver(
        base::DoNothingWithBoundArgs(std::move(info)));
    if (!video_frame) {
      LOG(ERROR) << __func__ << " failed to create video frame";
      std::move(callback).Run(nullptr);
      return;
    }
    std::move(callback).Run(video_frame);
    return;
  }
#endif  // BUILDFLAG(IS_ANDROID)
  LOG(ERROR) << "Cobalt: " << __func__;
  std::move(callback).Run(nullptr);
}

#if BUILDFLAG(IS_ANDROID)
void StarboardRendererWrapper::OnGpuFactoryInitialized() {
  DCHECK(is_decode_to_texture_preferred_);
  LOG(ERROR) << "Cobalt: " << __func__ << " " << texture_native_id_;
  DCHECK(texture_native_id_ != -1);
  is_gpu_factory_initialized_ = true;
  LOG(ERROR) << "Cobalt: " << __func__;
  decode_target_graphics_context_provider_.egl_context = &texture_native_id_;
  decode_target_graphics_context_provider_.gles_context_runner_context = this;
  decode_target_graphics_context_provider_.gles_context_runner =
      &StarboardRendererWrapper::GraphicsContextRunner;
  renderer_->set_decode_target_graphics_context_provider(base::BindRepeating(
      &StarboardRendererWrapper::GetSbDecodeTargetGraphicsContextProvider,
      base::Unretained(this)));
  LOG(ERROR) << "Cobalt: " << __func__;
  if (texture_owner_) {
    LOG(ERROR) << "Cobalt: " << __func__;
    codec_buffer_wait_coordinator_ =
        base::MakeRefCounted<CodecBufferWaitCoordinator>(
            std::move(texture_owner_), ref_counted_lock_);
  }

  if (media_resource_ != nullptr && client_ != nullptr && !init_cb_.is_null()) {
    LOG(ERROR) << "Cobalt: " << __func__;
    renderer_->Initialize(media_resource_, client_, std::move(init_cb_));
  }
}

void StarboardRendererWrapper::OnTextureOwnerInitialized(
    scoped_refptr<gpu::TextureOwner> texture_owner,
    base::UnguessableToken channel_token,
    int32_t route_id) {
  LOG(ERROR) << "Cobalt: " << __func__;
  if (!texture_owner) {
    LOG(ERROR) << "Could not allocated TextureOwner";
    return;
  }
  texture_owner_ = std::move(texture_owner);
  LOG(ERROR) << "Cobalt: " << __func__ << " " << texture_owner_->GetTextureId();

  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  LOG(ERROR) << "Cobalt: " << __func__;
  gpu_factory_.AsyncCall(&StarboardGpuFactory::Initialize)
      .WithArgs(channel_token, route_id, texture_owner_->GetTextureId(),
                &texture_native_id_, &done_event)
      .Then(base::BindOnce(&StarboardRendererWrapper::OnGpuFactoryInitialized,
                           weak_factory_.GetWeakPtr()));
  LOG(ERROR) << "Cobalt: " << __func__;
  // Blocking is okay here because it ensures TextureOwner is created
  // with valid |texture_native_id_| before OnGpuFactoryInitialized().
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  done_event.Wait();
  LOG(ERROR) << "Cobalt: " << __func__ << " " << texture_native_id_;
}

void StarboardRendererWrapper::CreateVideoFrame_OnImageReady(
    base::WeakPtr<StarboardRendererWrapper> thiz,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    SharedImageVideoProvider::ImageRecord record) {
  if (!thiz) {
    return;
  }

  if (codec_buffer_wait_coordinator_) {
    LOG(ERROR) << "Cobalt: " << __func__;
    std::unique_ptr<CodecOutputBufferRenderer> output_buffer_renderer =
        std::make_unique<CodecOutputBufferRenderer>(
            codec_buffer_wait_coordinator_, ref_counted_lock_);
    record.codec_image_holder->codec_image_raw()->Initialize(
        std::move(output_buffer_renderer), true,
        /*promotion_hint_cb=*/base::NullCallback());
  }
  LOG(ERROR) << "Cobalt: " << __func__ << " " << record.mailbox.ToDebugString();
  gpu::MailboxHolder mailbox_holders[VideoFrame::kMaxPlanes];
  mailbox_holders[0] = gpu::MailboxHolder(record.mailbox, gpu::SyncToken(),
                                          GL_TEXTURE_EXTERNAL_OES);
  LOG(ERROR) << "Cobalt: " << __func__ << " "
             << mailbox_holders[0].mailbox.IsZero() << " "
             << mailbox_holders[0].mailbox.ToDebugString();
  auto frame = VideoFrame::WrapNativeTextures(
      PIXEL_FORMAT_ABGR, mailbox_holders, VideoFrame::ReleaseMailboxCB(),
      coded_size, visible_rect, natural_size, kNoTimestamp);
  LOG(ERROR) << "Cobalt: " << __func__ << " " << frame->AsHumanReadableString();
  // If, for some reason, we failed to create a frame, then fail.  Note that we
  // don't need to call |release_cb|; dropping it is okay since the api says so.
  if (!frame) {
    LOG(ERROR) << __func__ << " failed to create video frame";
    return;
  }
  LOG(ERROR) << "Cobalt: " << __func__ << " " << record.mailbox.ToDebugString();
  frame->SetReleaseMailboxCB(std::move(record.release_cb));
  LOG(ERROR) << "Cobalt: " << __func__ << " " << record.mailbox.ToDebugString();
  current_frame_ = std::move(frame);
}

// static
scoped_refptr<gpu::TextureOwner> StarboardRendererWrapper::CreateTextureOwner(
    scoped_refptr<gpu::SharedContextState> context_state) {
  auto texture = CreateTexture(context_state.get());
  LOG(ERROR) << "Cobalt: " << __func__;
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
    DCHECK(is_decode_to_texture_preferred_);
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

// static
void StarboardRendererWrapper::GraphicsContextRunner(
    SbDecodeTargetGraphicsContextProvider* graphics_context_provider,
    SbDecodeTargetGlesContextRunnerTarget target_function,
    void* target_function_context) {
  LOG(ERROR) << "Cobalt: " << __func__;

  StarboardRendererWrapper* provider =
      reinterpret_cast<StarboardRendererWrapper*>(
          graphics_context_provider->gles_context_runner_context);

#if BUILDFLAG(IS_ANDROID)
  if (!provider->is_gpu_factory_initialized_) {
    LOG(ERROR) << "Cobalt: " << __func__;
    return;
  }
  LOG(ERROR) << "Cobalt: " << __func__;
  if (provider->gpu_factory_) {
    LOG(ERROR) << "Cobalt: " << __func__;
    base::WaitableEvent done_event(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    LOG(ERROR) << "Cobalt: " << __func__;
    provider->gpu_factory_
        .AsyncCall(&StarboardGpuFactory::RunSbDecodeTargetFunctionOnGpu)
        .WithArgs(target_function, target_function_context, &done_event);
    LOG(ERROR) << "Cobalt: " << __func__;
    // TODO(borongchen): Blocking is okay here because XYZ.
    base::ScopedAllowBaseSyncPrimitives allow_wait;
    done_event.Wait();
    LOG(ERROR) << "Cobalt: " << __func__;
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

void StarboardRendererWrapper::GetCurrentDecodeTarget() {
  LOG(ERROR) << "Cobalt: " << __func__;
#if BUILDFLAG(IS_ANDROID)
  decode_target_ = renderer_->GetSbDecodeTarget();
  LOG(ERROR) << "Cobalt: " << __func__;
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace media
