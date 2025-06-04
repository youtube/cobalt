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
#include "media/base/starboard/starboard_rendering_mode.h"
#include "media/gpu/starboard/starboard_gpu_factory.h"
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
          traits.max_video_capabilities),
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
  if (IsGpuChannelTokenAvailable()) {
    // Create StarboardGpuFactory() using |command_buffer_id_|.
    base::WaitableEvent done_event(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    gpu_factory_.AsyncCall(&StarboardGpuFactory::Initialize)
        .WithArgs(command_buffer_id_->channel_token,
                  command_buffer_id_->route_id, &done_event)
        .Then(base::BindOnce(&StarboardRendererWrapper::OnGpuFactoryInitialized,
                             weak_factory_.GetWeakPtr()));
    // Blocking is okay here because it ensures StarboardRenderer is initialized
    // after getting gpu::CommandBufferStub().
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    done_event.Wait();
  }

  renderer_.SetStarboardRendererCallbacks(
      base::BindRepeating(
          &StarboardRendererWrapper::OnPaintVideoHoleFrameByStarboard,
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &StarboardRendererWrapper::OnUpdateStarboardRenderingModeByStarboard,
          weak_factory_.GetWeakPtr()));

  if (IsGpuChannelTokenAvailable() && !is_gpu_factory_initialized_) {
    // Delay |init_cb| after StarboardGpuFactory() is created.
    media_resource_ = media_resource;
    client_ = client;
    init_cb_ = std::move(init_cb);
    return;
  }

  renderer_.Initialize(media_resource, client, std::move(init_cb));
}

void StarboardRendererWrapper::Flush(base::OnceClosure flush_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  renderer_.Flush(std::move(flush_cb));
}

void StarboardRendererWrapper::StartPlayingFrom(base::TimeDelta time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  renderer_.StartPlayingFrom(time);
}

void StarboardRendererWrapper::SetPlaybackRate(double playback_rate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  renderer_.SetPlaybackRate(playback_rate);
}

void StarboardRendererWrapper::SetVolume(float volume) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  renderer_.SetVolume(volume);
}

void StarboardRendererWrapper::SetCdm(CdmContext* cdm_context,
                                      CdmAttachedCB cdm_attached_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  renderer_.SetCdm(cdm_context, std::move(cdm_attached_cb));
}

void StarboardRendererWrapper::SetLatencyHint(
    absl::optional<base::TimeDelta> latency_hint) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  renderer_.SetLatencyHint(latency_hint);
}

base::TimeDelta StarboardRendererWrapper::GetMediaTime() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return renderer_.GetMediaTime();
}

RendererType StarboardRendererWrapper::GetRendererType() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return RendererType::kStarboard;
}

void StarboardRendererWrapper::OnVideoGeometryChange(
    const gfx::Rect& output_rect) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  renderer_.OnVideoGeometryChange(output_rect);
}

void StarboardRendererWrapper::OnGpuChannelTokenReady(
    mojom::CommandBufferIdPtr command_buffer_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  command_buffer_id_ = std::move(command_buffer_id);
}

void StarboardRendererWrapper::OnGpuFactoryInitialized() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  is_gpu_factory_initialized_ = true;
  // TODO(b/375070492): add |decode_target_graphics_context_provider_|.

  if (media_resource_ != nullptr && client_ != nullptr && !init_cb_.is_null()) {
    renderer_.Initialize(media_resource_, client_, std::move(init_cb_));
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

}  // namespace media
