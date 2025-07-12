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

namespace media {

StarboardRendererWrapper::StarboardRendererWrapper(
    StarboardRendererTraits traits)
    : renderer_extension_receiver_(
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
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &StarboardRendererWrapper::OnRequestOverlayInfoByStarboard,
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
  // TODO(b/375070492): get SbDecodeTarget from
  // SbPlayerBridge::GetCurrentSbDecodeTarget().
  std::move(callback).Run(nullptr);
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
  // TODO(b/375070492): add |decode_target_graphics_context_provider_|.

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

void StarboardRendererWrapper::OnRequestOverlayInfoByStarboard(
    bool restart_for_transitions) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_extension_remote_->RequestOverlayInfo(restart_for_transitions);
}

void StarboardRendererWrapper::OnOverlayInfoChanged(
    const OverlayInfo& overlay_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GetRenderer()->OnOverlayInfoChanged(overlay_info);
}

}  // namespace media
