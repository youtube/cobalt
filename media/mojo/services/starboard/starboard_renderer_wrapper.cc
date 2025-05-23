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

#include "base/functional/callback_helpers.h"
#include "base/task/bind_post_task.h"
#include "base/time/time.h"
#include "media/base/starboard/starboard_rendering_mode.h"
#include "media/mojo/services/mojo_media_log.h"
#include "ui/gl/gl_bindings.h"

namespace media {

StarboardRendererWrapper::StarboardRendererWrapper(
#if BUILDFLAG(IS_ANDROID)
    StarboardRendererTraits traits,
    scoped_refptr<gpu::RefCountedLock> drdc_lock)
    : gpu::RefCountedLockHelperDrDc(std::move(drdc_lock)),
#else   // BUILDFLAG(IS_ANDROID)
    StarboardRendererTraits traits)
    :
#endif  // BUILDFLAG(IS_ANDROID)
      renderer_extension_receiver_(
          this,
          std::move(traits.renderer_extension_receiver)),
      client_extension_remote_(std::move(traits.client_extension_remote),
                               traits.task_runner),
      renderer_(
          traits.task_runner,
          std::make_unique<MojoMediaLog>(std::move(traits.media_log_remote),
                                         traits.task_runner),
          traits.overlay_plane_id,
          traits.audio_write_duration_local,
          traits.audio_write_duration_remote,
          traits.max_video_capabilities) {
  DETACH_FROM_THREAD(thread_checker_);
  base::SequenceBound<StarboardGpuFactoryImpl> gpu_factory_impl(
      traits.gpu_task_runner,
      std::move(traits.get_starboard_command_buffer_stub_cb));
  gpu_factory_ = std::move(gpu_factory_impl);
  gpu_task_runner_ = std::move(traits.gpu_task_runner);
}

StarboardRendererWrapper::~StarboardRendererWrapper() = default;

void StarboardRendererWrapper::Initialize(MediaResource* media_resource,
                                          RendererClient* client,
                                          PipelineStatusCallback init_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(init_cb);

  GetRenderer()->SetStarboardRendererCallbacks(
      base::BindRepeating(
          &StarboardRendererWrapper::OnPaintVideoHoleFrameByStarboard,
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &StarboardRendererWrapper::OnUpdateStarboardRenderingModeByStarboard,
          weak_factory_.GetWeakPtr()));

  base::ScopedClosureRunner scoped_init_cb(
      base::BindOnce(&StarboardRendererWrapper::ContinueInitialization,
                     weak_factory_.GetWeakPtr(), std::move(media_resource),
                     std::move(client), std::move(init_cb)));

  // OnGpuChannelTokenReady() is called before Initialize()
  // in StarboardRendererClient if GpuVideoAcceleratorFactories is available.
  // In this case, it is safe to access |command_buffer_id_| for posting gpu
  // tasks.
  if (IsGpuChannelTokenAvailable()) {
    // Create StarboardGpuFactory using |command_buffer_id_|.
    base::OnceClosure init_callback = scoped_init_cb.Release();
    GetGpuFactory()
        ->AsyncCall(&StarboardGpuFactory::Initialize)
        .WithArgs(command_buffer_id_->channel_token,
                  command_buffer_id_->route_id,
                  base::BindPostTaskToCurrentDefault(std::move(init_callback)));
    return;
  }
}

void StarboardRendererWrapper::Flush(base::OnceClosure flush_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GetRenderer()->Flush(std::move(flush_cb));
}

void StarboardRendererWrapper::StartPlayingFrom(base::TimeDelta time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GetRenderer()->StartPlayingFrom(time);
}

void StarboardRendererWrapper::SetPlaybackRate(double playback_rate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GetRenderer()->SetPlaybackRate(playback_rate);
}

void StarboardRendererWrapper::SetVolume(float volume) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GetRenderer()->SetVolume(volume);
}

void StarboardRendererWrapper::SetCdm(CdmContext* cdm_context,
                                      CdmAttachedCB cdm_attached_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GetRenderer()->SetCdm(cdm_context, std::move(cdm_attached_cb));
}

void StarboardRendererWrapper::SetLatencyHint(
    absl::optional<base::TimeDelta> latency_hint) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GetRenderer()->SetLatencyHint(latency_hint);
}

base::TimeDelta StarboardRendererWrapper::GetMediaTime() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return GetRenderer()->GetMediaTime();
}

RendererType StarboardRendererWrapper::GetRendererType() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return RendererType::kStarboard;
}

void StarboardRendererWrapper::OnVideoGeometryChange(
    const gfx::Rect& output_rect) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GetRenderer()->OnVideoGeometryChange(output_rect);
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
    // Post GetRenderer()->GetSbDecodeTarget() on the gpu thread.
    base::RepeatingCallback<void()> get_current_decode_target_cb =
        base::BindRepeating(&StarboardRendererWrapper::GetCurrentDecodeTarget,
                            base::Unretained(this));
    base::WaitableEvent done_event(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    GetGpuFactory()
        ->AsyncCall(&StarboardGpuFactory::RunCallbackOnGpu)
        .WithArgs(std::move(get_current_decode_target_cb), &done_event);
    // TODO(borongchen): Blocking is okay here because XYZ.
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    done_event.Wait();
  }
  if (SbDecodeTargetIsValid(decode_target_)) {
    auto info = std::make_unique<SbDecodeTargetInfo>();
    memset(info.get(), 0, sizeof(SbDecodeTargetInfo));
    CHECK(SbDecodeTargetGetInfo(decode_target_, info.get()));

    VideoPixelFormat format;
    std::vector<uint32_t> texture_service_ids;
    std::vector<gpu::Mailbox> mailboxes;
    int plane_count = SbDecodeTargetNumberOfPlanesForFormat(info.get()->format);
    DCHECK_GE(plane_count, 1);
    const SbDecodeTargetInfoPlane& plane = info.get()->planes[0];
    auto coded_size = gfx::Size(info.get()->width, info.get()->height);
    auto visible_rect = gfx::Rect(
        gfx::Point(0, 0),
        gfx::Size(
            std::abs(plane.content_region.right - plane.content_region.left),
            std::abs(plane.content_region.top - plane.content_region.bottom)));
    auto natural_size = visible_rect.size();

    if (info.get()->format == kSbDecodeTargetFormat1PlaneRGBA) {
      DCHECK_EQ(plane_count, 1u);
      format = PIXEL_FORMAT_ABGR;
    } else if (info.get()->format == kSbDecodeTargetFormat3PlaneYUVI420) {
      DCHECK_EQ(plane_count, 3u);
      format = PIXEL_FORMAT_YV12;
    } else {
      NOTREACHED() << "Unsupported SbDecodeTargetFormat: "
                   << static_cast<int>(info.get()->format);
      std::move(callback).Run(nullptr);
      return;
    }
    for (int plane_index = 0; plane_index < plane_count; plane_index++) {
      SbDecodeTargetInfoPlane plane = info.get()->planes[plane_index];
      texture_service_ids.push_back(plane.texture);
    }
    DCHECK_EQ(texture_service_ids.size(), plane_count);
    {
      base::WaitableEvent done_event(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::NOT_SIGNALED);
      GetGpuFactory()
          ->AsyncCall(&StarboardGpuFactory::CreateImageOnGpu)
          .WithArgs(coded_size, GetRenderer()->color_space(), plane_count,
                    std::ref(mailboxes), std::ref(texture_service_ids),
#if BUILDFLAG(IS_ANDROID)
                    GetDrDcLock(),
#endif  // BUILDFLAG(IS_ANDROID)
                    &done_event);
      // TODO(borongchen): Blocking is okay here because XYZ.
      base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
      done_event.Wait();
      DCHECK_EQ(mailboxes.size(), plane_count);
      if (mailboxes.size()) {
        CreateVideoFrame_OnImageReady(format, coded_size, visible_rect,
                                      natural_size, mailboxes);
      }
    }
  }
  if (current_frame_) {
    // TODO(borongchen): return VideoFrame
    std::move(callback).Run(std::move(current_frame_));
    // std::move(callback).Run(nullptr);
  } else {
    std::move(callback).Run(nullptr);
  }
}

StarboardRenderer* StarboardRendererWrapper::GetRenderer() {
  if (test_renderer_) {
    return test_renderer_;
  }
  return &renderer_;
}

base::SequenceBound<StarboardGpuFactory>*
StarboardRendererWrapper::GetGpuFactory() {
  if (test_gpu_factory_) {
    return test_gpu_factory_;
  }
  return &gpu_factory_;
}

void StarboardRendererWrapper::ContinueInitialization(
    MediaResource* media_resource,
    RendererClient* client,
    PipelineStatusCallback init_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(init_cb);
  is_gpu_factory_initialized_ = true;
  decode_target_graphics_context_provider_.gles_context_runner_context = this;
  decode_target_graphics_context_provider_.gles_context_runner =
      &StarboardRendererWrapper::GraphicsContextRunner;
  GetRenderer()->set_decode_target_graphics_context_provider(
      base::BindRepeating(
          &StarboardRendererWrapper::GetSbDecodeTargetGraphicsContextProvider,
          base::Unretained(this)));

  GetRenderer()->Initialize(media_resource, client, std::move(init_cb));
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
  StarboardRendererWrapper* provider =
      reinterpret_cast<StarboardRendererWrapper*>(
          graphics_context_provider->gles_context_runner_context);

  if (!provider->is_gpu_factory_initialized_) {
    return;
  }
  if (provider->gpu_task_runner_->RunsTasksInCurrentSequence()) {
    // If it is on the gpu thread, post target_function() directly on it.
    target_function(target_function_context);
  } else if (provider->gpu_factory_) {
    // If it is not on the gpu thread, post target_function() with
    // |gpu_factory_|.
    base::WaitableEvent done_event(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    provider->gpu_factory_
        .AsyncCall(&StarboardGpuFactory::RunSbDecodeTargetFunctionOnGpu)
        .WithArgs(target_function, target_function_context, &done_event);
    // TODO(borongchen): Blocking is okay here because XYZ.
    base::ScopedAllowBaseSyncPrimitives allow_wait;
    done_event.Wait();
  }
}

void StarboardRendererWrapper::GetCurrentDecodeTarget() {
  decode_target_ = GetRenderer()->GetSbDecodeTarget();
}

void StarboardRendererWrapper::CreateVideoFrame_OnImageReady(
    VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    std::vector<gpu::Mailbox>& mailboxes) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  gpu::MailboxHolder mailbox_holders[VideoFrame::kMaxPlanes];
  for (size_t plane = 0; plane < mailboxes.size(); plane++) {
    mailbox_holders[plane] = gpu::MailboxHolder(
        mailboxes[plane], gpu::SyncToken(), GL_TEXTURE_EXTERNAL_OES);
  }
  auto frame = VideoFrame::WrapNativeTextures(
      format, mailbox_holders, VideoFrame::ReleaseMailboxCB(), coded_size,
      visible_rect, natural_size, base::TimeDelta());
  if (!frame) {
    LOG(ERROR) << __func__ << " failed to create video frame";
    return;
  }
  frame->metadata().texture_owner = true;
  current_frame_ = std::move(frame);
}

}  // namespace media
