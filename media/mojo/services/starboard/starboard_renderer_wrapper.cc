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

#include "base/time/time.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/starboard/starboard_rendering_mode.h"
#include "media/gpu/starboard/starboard_gpu_factory.h"
#include "media/mojo/services/mojo_media_log.h"
#include "ui/gl/gl_bindings.h"

namespace media {

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
          traits.audio_write_duration_remote,
          traits.max_video_capabilities)),
      get_starboard_command_buffer_stub_cb_(
          traits.get_starboard_command_buffer_stub_cb),
      gpu_factory_(traits.gpu_task_runner,
                   traits.get_starboard_command_buffer_stub_cb),
      gpu_task_runner_(std::move(traits.gpu_task_runner)) {
  DETACH_FROM_THREAD(thread_checker_);
}

StarboardRendererWrapper::~StarboardRendererWrapper() = default;

void StarboardRendererWrapper::Initialize(MediaResource* media_resource,
                                          RendererClient* client,
                                          PipelineStatusCallback init_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // OnGpuChannelTokenReady() is called before Initialize()
  // in StarboardRendererClient, so it is safe to access
  // |command_buffer_id_| for posting gpu tasks.
  if (command_buffer_id_) {
    base::WaitableEvent done_event(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    LOG(ERROR) << "Cobalt: " << __func__;
    gpu_factory_.AsyncCall(&StarboardGpuFactory::Initialize)
        .WithArgs(command_buffer_id_->channel_token,
                  command_buffer_id_->route_id, &done_event)
        .Then(base::BindOnce(&StarboardRendererWrapper::OnGpuFactoryInitialized,
                             weak_factory_.GetWeakPtr()));
    LOG(ERROR) << "Cobalt: " << __func__;
    // Blocking is okay here because it ensures TextureOwner is created
    // with valid |texture_native_id_| before OnGpuFactoryInitialized().
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    done_event.Wait();
  }

  renderer_->SetStarboardRendererCallbacks(
      base::BindRepeating(
          &StarboardRendererWrapper::OnPaintVideoHoleFrameByStarboard,
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &StarboardRendererWrapper::OnUpdateStarboardRenderingModeByStarboard,
          weak_factory_.GetWeakPtr()));

  if (!is_gpu_factory_initialized_) {
    LOG(ERROR) << "Cobalt: " << __func__;
    media_resource_ = media_resource;
    client_ = client;
    init_cb_ = std::move(init_cb);
    return;
  }

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

void StarboardRendererWrapper::GetCurrentVideoFrame(
    GetCurrentVideoFrameCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  {
    // Post renderer_->GetSbDecodeTarget() on the gpu thread.
    base::RepeatingCallback<void()> get_current_decode_target_cb =
        base::BindRepeating(&StarboardRendererWrapper::GetCurrentDecodeTarget,
                            base::Unretained(this));
    base::WaitableEvent done_event(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    LOG(ERROR) << "Cobalt: " << __func__;
    gpu_factory_.AsyncCall(&StarboardGpuFactory::RunCallbackOnGpu)
        .WithArgs(std::move(get_current_decode_target_cb), &done_event);
    LOG(ERROR) << "Cobalt: " << __func__;
    // TODO(borongchen): Blocking is okay here because XYZ.
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    done_event.Wait();
    LOG(ERROR) << "Cobalt: " << __func__;
  }
  if (SbDecodeTargetIsValid(decode_target_)) {
    LOG(ERROR) << "Cobalt: " << __func__;
    auto info = std::make_unique<SbDecodeTargetInfo>();
    memset(info.get(), 0, sizeof(SbDecodeTargetInfo));
    CHECK(SbDecodeTargetGetInfo(decode_target_, info.get()));
    LOG(ERROR) << "Cobalt: " << __func__ << ", width: " << info.get()->width
               << ", height: " << info.get()->height
               << ", format: " << info.get()->format
               << ", is_opaque: " << info.get()->is_opaque;
    int plane_count = SbDecodeTargetNumberOfPlanesForFormat(info.get()->format);
    auto coded_size = gfx::Size(info.get()->width, info.get()->height);
    auto visible_rect = gfx::Rect(gfx::Point(0, 0), coded_size);
    auto natural_size = visible_rect.size();
    VideoPixelFormat format;
    if (info.get()->format == kSbDecodeTargetFormat1PlaneRGBA) {
      format = PIXEL_FORMAT_ABGR;
    } else if (info.get()->format == kSbDecodeTargetFormat3PlaneYUVI420) {
      format = PIXEL_FORMAT_I420;
    }
    for (int plane_index = 0; plane_index < plane_count; plane_index++) {
      SbDecodeTargetInfoPlane plane = info.get()->planes[plane_index];
      LOG(ERROR) << "Cobalt: " << __func__ << ", idx: " << plane_index
                 << ", texture: " << plane.texture << ", width: " << plane.width
                 << ", height: " << plane.height << ", "
                 << plane.content_region.left << "/" << plane.content_region.top
                 << "/" << plane.content_region.right << "/"
                 << plane.content_region.bottom;
    }
    {
      // TODO(borongchen): wrap texture to a VideoFrame
      base::RepeatingCallback<void()> create_image_cb = base::BindRepeating(
          &StarboardRendererWrapper::CreateImage, base::Unretained(this),
          plane_count, format, coded_size, visible_rect, natural_size);
      base::WaitableEvent done_event(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::NOT_SIGNALED);
      LOG(ERROR) << "Cobalt: " << __func__;
      gpu_factory_.AsyncCall(&StarboardGpuFactory::RunCallbackOnGpu)
          .WithArgs(std::move(create_image_cb), &done_event);
      LOG(ERROR) << "Cobalt: " << __func__;
      // TODO(borongchen): Blocking is okay here because XYZ.
      base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
      done_event.Wait();
      LOG(ERROR) << "Cobalt: " << __func__;
    }
  }
  if (current_frame_) {
    LOG(ERROR) << "Cobalt: " << __func__;
    // TODO(borongchen): return VideoFrame
    // std::move(callback).Run(std::move(current_frame_));
    std::move(callback).Run(nullptr);
  } else {
    LOG(ERROR) << "Cobalt: " << __func__;
    std::move(callback).Run(nullptr);
  }
}

void StarboardRendererWrapper::OnGpuFactoryInitialized() {
  is_gpu_factory_initialized_ = true;
  LOG(ERROR) << "Cobalt: " << __func__;
  decode_target_graphics_context_provider_.gles_context_runner_context = this;
  decode_target_graphics_context_provider_.gles_context_runner =
      &StarboardRendererWrapper::GraphicsContextRunner;
  renderer_->set_decode_target_graphics_context_provider(base::BindRepeating(
      &StarboardRendererWrapper::GetSbDecodeTargetGraphicsContextProvider,
      base::Unretained(this)));
  LOG(ERROR) << "Cobalt: " << __func__;

  if (media_resource_ != nullptr && client_ != nullptr && !init_cb_.is_null()) {
    LOG(ERROR) << "Cobalt: " << __func__;
    renderer_->Initialize(media_resource_, client_, std::move(init_cb_));
  }
}

void StarboardRendererWrapper::OnPaintVideoHoleFrameByStarboard(
    const gfx::Size& size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_extension_remote_->PaintVideoHoleFrame(size);
}

void StarboardRendererWrapper::OnUpdateStarboardRenderingModeByStarboard(
    const StarboardRenderingMode mode) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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

  if (!provider->is_gpu_factory_initialized_) {
    LOG(ERROR) << "Cobalt: " << __func__;
    return;
  }
  LOG(ERROR) << "Cobalt: " << __func__;
  if (provider->gpu_task_runner_->RunsTasksInCurrentSequence()) {
    // If it is on the gpu thread, post target_function() directly on it.
    target_function(target_function_context);
  } else if (provider->gpu_factory_) {
    // If it is not on the gpu thread, post target_function() with
    // |gpu_factory_|.
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
}

void StarboardRendererWrapper::GetCurrentDecodeTarget() {
  LOG(ERROR) << "Cobalt: " << __func__;
  decode_target_ = renderer_->GetSbDecodeTarget();
  LOG(ERROR) << "Cobalt: " << __func__;
}

void StarboardRendererWrapper::CreateImage(int plane_count,
                                           VideoPixelFormat format,
                                           const gfx::Size& coded_size,
                                           const gfx::Rect& visible_rect,
                                           const gfx::Size& natural_size) {
  LOG(ERROR) << "Cobalt: " << __func__ << " " << plane_count;
  gpu::MailboxHolder mailbox_holders[VideoFrame::kMaxPlanes];
  for (int plane = 0; plane < plane_count; plane++) {
    auto mailbox = gpu::Mailbox::GenerateForSharedImage();
    mailbox_holders[plane] =
        gpu::MailboxHolder(mailbox, gpu::SyncToken(), GL_TEXTURE_EXTERNAL_OES);
    LOG(ERROR) << "Cobalt: " << __func__ << " "
               << mailbox_holders[plane].mailbox.ToDebugString();
  }
  auto frame = VideoFrame::WrapNativeTextures(
      format, mailbox_holders, VideoFrame::ReleaseMailboxCB(), coded_size,
      visible_rect, natural_size, kNoTimestamp);
  if (!frame) {
    LOG(ERROR) << __func__ << " failed to create video frame";
    return;
  }
  LOG(ERROR) << "Cobalt: " << __func__;
  current_frame_ = std::move(frame);
  LOG(ERROR) << "Cobalt: " << __func__;
}

}  // namespace media
