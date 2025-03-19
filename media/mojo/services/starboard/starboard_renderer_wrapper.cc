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
#include "media/mojo/services/mojo_media_log.h"

namespace media {

StarboardRendererWrapper::StarboardRendererWrapper(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    mojo::PendingRemote<mojom::MediaLog> media_log_remote,
    const base::UnguessableToken& overlay_plane_id,
    TimeDelta audio_write_duration_local,
    TimeDelta audio_write_duration_remote,
    mojo::PendingReceiver<RendererExtension> renderer_extension_receiver,
    mojo::PendingRemote<ClientExtension> client_extension_remote)
    : renderer_extension_receiver_(this,
                                   std::move(renderer_extension_receiver)),
      client_extension_remote_(std::move(client_extension_remote),
                               task_runner) {
  renderer_ = std::make_unique<StarboardRenderer>(
      std::move(task_runner),
      std::make_unique<MojoMediaLog>(std::move(media_log_remote), task_runner),
      overlay_plane_id, audio_write_duration_local,
      audio_write_duration_remote);
}

StarboardRendererWrapper::~StarboardRendererWrapper() = default;

void StarboardRendererWrapper::Initialize(MediaResource* media_resource,
                                          RendererClient* client,
                                          PipelineStatusCallback init_cb) {
  renderer_->SetPaintVideoHoleFrameCallbacks(base::BindRepeating(
      &StarboardRendererWrapper::OnPaintVideoHoleFrameByStarboard,
      weak_factory_.GetWeakPtr()));
  renderer_->Initialize(media_resource, client, std::move(init_cb));
}

void StarboardRendererWrapper::Flush(base::OnceClosure flush_cb) {
  renderer_->Flush(std::move(flush_cb));
}

void StarboardRendererWrapper::StartPlayingFrom(base::TimeDelta time) {
  renderer_->StartPlayingFrom(time);
}

void StarboardRendererWrapper::SetPlaybackRate(double playback_rate) {
  renderer_->SetPlaybackRate(playback_rate);
}

void StarboardRendererWrapper::SetVolume(float volume) {
  renderer_->SetVolume(volume);
}

void StarboardRendererWrapper::SetCdm(CdmContext* cdm_context,
                                      CdmAttachedCB cdm_attached_cb) {
  renderer_->SetCdm(cdm_context, std::move(cdm_attached_cb));
}

void StarboardRendererWrapper::SetLatencyHint(
    absl::optional<base::TimeDelta> latency_hint) {
  renderer_->SetLatencyHint(latency_hint);
}

base::TimeDelta StarboardRendererWrapper::GetMediaTime() {
  return renderer_->GetMediaTime();
}

RendererType StarboardRendererWrapper::GetRendererType() {
  return RendererType::kStarboard;
}

void StarboardRendererWrapper::OnVideoGeometryChange(
    const gfx::Rect& output_rect) {
  renderer_->OnVideoGeometryChange(output_rect);
}

void StarboardRendererWrapper::OnPaintVideoHoleFrameByStarboard(
    const gfx::Size& size) {
  client_extension_remote_->PaintVideoHoleFrame(size);
}

}  // namespace media
