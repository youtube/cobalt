// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "media/mojo/services/starboard/direct_renderer_service.h"

#include "base/logging.h"
#include "cobalt/media/service/video_geometry_setter_service.h"
#include "media/base/video_frame.h"
#include "media/mojo/clients/starboard/direct_renderer.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace media {

namespace {
cobalt::media::VideoGeometrySetterService* g_video_geometry_setter_service =
    nullptr;
}

// static
void DirectRendererService::SetVideoGeometrySetterService(
    cobalt::media::VideoGeometrySetterService* service) {
  g_video_geometry_setter_service = service;
}

// static
cobalt::media::VideoGeometrySetterService*
DirectRendererService::GetVideoGeometrySetterService() {
  return g_video_geometry_setter_service;
}

// static
void DirectRendererService::Create(
    base::WeakPtr<DirectRendererClient> client,
    const scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
    const std::vector<DemuxerStream*>& streams,
    const StarboardRendererConfig& config,
    std::unique_ptr<MediaLog> media_log,
    void* subscriber_remote,
    base::OnceCallback<void(DirectRendererHost*)> callback) {
  auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();

  DCHECK(media_log);

  auto starboard_renderer = std::make_unique<StarboardRenderer>(
      task_runner, std::move(media_log), config.overlay_plane_id,
      config.audio_write_duration_local, config.audio_write_duration_remote,
      config.max_video_capabilities, config.experimental_features,
      config.viewport_size
#if BUILDFLAG(IS_ANDROID)
      ,
      base::NullCallback()
#endif
  );

  mojo::PendingRemote<::cobalt::media::mojom::VideoGeometryChangeSubscriber>
      subscriber;
  if (subscriber_remote) {
    auto* pending_remote_ptr = static_cast<mojo::PendingRemote<
        ::cobalt::media::mojom::VideoGeometryChangeSubscriber>*>(
        subscriber_remote);
    subscriber = std::move(*pending_remote_ptr);
    delete pending_remote_ptr;
  }

  auto service = std::make_unique<DirectRendererService>(
      std::move(starboard_renderer), client, client_task_runner, task_runner,
      config.overlay_plane_id, std::move(subscriber));

  std::move(callback).Run(static_cast<DirectRendererHost*>(service.release()));
}

DirectRendererService::DirectRendererService(
    std::unique_ptr<StarboardRenderer> renderer,
    base::WeakPtr<DirectRendererClient> client,
    const scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::UnguessableToken& overlay_plane_id,
    mojo::PendingRemote<::cobalt::media::mojom::VideoGeometryChangeSubscriber>
        subscriber_remote)
    : renderer_(std::move(renderer)),
      client_(std::move(client)),
      client_task_runner_(client_task_runner),
      task_runner_(task_runner) {
  DVLOG(1) << __func__;

  if (subscriber_remote.is_valid()) {
    video_geometry_change_subscriber_remote_.Bind(std::move(subscriber_remote));
    mojo::PendingRemote<::cobalt::media::mojom::VideoGeometryChangeClient>
        client_remote;
    video_geometry_change_receiver_.Bind(
        client_remote.InitWithNewPipeAndPassReceiver());

    video_geometry_change_subscriber_remote_->SubscribeToVideoGeometryChange(
        overlay_plane_id, std::move(client_remote), base::BindOnce([]() {
          LOG(INFO) << "Subscribed to video geometry change via Mojo";
        }));
  }
}

DirectRendererService::~DirectRendererService() {
  LOG(INFO) << "DirectRendererService destroyed";
}

void DirectRendererService::Initialize(MediaResource* media_resource,
                                       RendererClient* client,
                                       PipelineStatusCallback init_cb) {
  LOG(INFO) << "DirectRendererService::Initialize called";

  renderer_->SetStarboardRendererCallbacks(
      base::BindRepeating(
          &DirectRendererService::OnPaintVideoHoleFrameByStarboard,
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &DirectRendererService::OnUpdateStarboardRenderingModeByStarboard,
          weak_factory_.GetWeakPtr()),
#if BUILDFLAG(IS_STARBOARD)
      base::BindRepeating(&DirectRendererService::OnGetSbWindowHandle,
                          weak_factory_.GetWeakPtr())
#else
      base::NullCallback()
#endif
#if BUILDFLAG(IS_ANDROID)
          ,
      base::BindRepeating(
          &DirectRendererService::OnRequestOverlayInfoByStarboard,
          weak_factory_.GetWeakPtr())
#endif
  );

  renderer_->Initialize(media_resource,
                        static_cast<media::RendererClient*>(this),
                        std::move(init_cb));
}

void DirectRendererService::InitializeService(
    MediaResource* media_resource,
    base::OnceCallback<void(PipelineStatus)> callback) {
  LOG(INFO) << "DirectRendererService::InitializeService called";
  Initialize(media_resource, nullptr, std::move(callback));
}

void DirectRendererService::SetCdm(CdmContext* cdm_context,
                                   CdmAttachedCB cdm_attached_cb) {
  DVLOG(1) << __func__;
  renderer_->SetCdm(cdm_context, std::move(cdm_attached_cb));
}

void DirectRendererService::SetLatencyHint(
    std::optional<base::TimeDelta> latency_hint) {
  DVLOG(1) << __func__;
  renderer_->SetLatencyHint(latency_hint);
}

void DirectRendererService::Flush(base::OnceClosure flush_cb) {
  LOG(INFO) << "DirectRendererService::Flush called";
  CancelPeriodicMediaTimeUpdates();
  renderer_->Flush(std::move(flush_cb));
}

void DirectRendererService::StartPlayingFrom(base::TimeDelta time) {
  LOG(INFO) << "DirectRendererService::StartPlayingFrom called with " << time;
  renderer_->StartPlayingFrom(time);
  SchedulePeriodicMediaTimeUpdates();
}

void DirectRendererService::SetPlaybackRate(double playback_rate) {
  LOG(INFO) << "DirectRendererService::SetPlaybackRate called with "
            << playback_rate;
  playback_rate_ = playback_rate;
  renderer_->SetPlaybackRate(playback_rate);
}

void DirectRendererService::SetVolume(float volume) {
  LOG(INFO) << "DirectRendererService::SetVolume called with " << volume;
  renderer_->SetVolume(volume);
}

base::TimeDelta DirectRendererService::GetMediaTime() {
  return renderer_->GetMediaTime();
}

RendererType DirectRendererService::GetRendererType() {
  return renderer_->GetRendererType();
}

void DirectRendererService::OnGpuChannelTokenReady(
    const base::UnguessableToken& token) {
  DVLOG(1) << __func__;
}

void DirectRendererService::GetCurrentVideoFrame(
    base::OnceCallback<void(scoped_refptr<VideoFrame>)> cb) {
  DVLOG(1) << __func__;
  std::move(cb).Run(nullptr);
}

void DirectRendererService::OnSbWindowHandleReady(uint64_t sb_window_handle) {
  DVLOG(1) << __func__;
}

void DirectRendererService::OnPaintVideoHoleFrameByStarboard(
    const gfx::Size& size) {
  if (client_ && client_task_runner_) {
    client_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DirectRendererClient::PaintVideoHoleFrame,
                                  client_, size));
  }
}

void DirectRendererService::OnUpdateStarboardRenderingModeByStarboard(
    const StarboardRenderingMode mode) {
  if (client_ && client_task_runner_) {
    client_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&DirectRendererClient::UpdateStarboardRenderingMode,
                       client_, mode));
  }
}

void DirectRendererService::OnGetSbWindowHandle() {
  if (client_ && client_task_runner_) {
    client_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&DirectRendererClient::GetSbWindowHandle, client_));
  }
}

#if BUILDFLAG(IS_ANDROID)
void DirectRendererService::OnRequestOverlayInfoByStarboard(
    bool restart_for_transitions) {
  if (client_ && client_task_runner_) {
    client_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DirectRendererClient::RequestOverlayInfo,
                                  client_, restart_for_transitions));
  }
}
#endif

// RendererClient implementation.
void DirectRendererService::OnError(PipelineStatus status) {
  if (client_ && client_task_runner_) {
    client_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&media::RendererClient::OnError, client_, status));
  }
}

void DirectRendererService::OnFallback(PipelineStatus status) {
  if (client_ && client_task_runner_) {
    client_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&media::RendererClient::OnFallback, client_, status));
  }
}

void DirectRendererService::OnEnded() {
  if (client_ && client_task_runner_) {
    client_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&media::RendererClient::OnEnded, client_));
  }
}

void DirectRendererService::OnStatisticsUpdate(
    const PipelineStatistics& stats) {
  if (client_ && client_task_runner_) {
    client_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&media::RendererClient::OnStatisticsUpdate,
                                  client_, stats));
  }
}

void DirectRendererService::OnBufferingStateChange(
    BufferingState state,
    BufferingStateChangeReason reason) {
  if (client_ && client_task_runner_) {
    client_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&media::RendererClient::OnBufferingStateChange, client_,
                       state, reason));
  }
}

void DirectRendererService::OnWaiting(WaitingReason reason) {
  DVLOG(1) << __func__;
}

void DirectRendererService::OnAudioConfigChange(
    const AudioDecoderConfig& config) {
  DVLOG(1) << __func__;
}

void DirectRendererService::OnVideoConfigChange(
    const VideoDecoderConfig& config) {
  DVLOG(1) << __func__;
}

void DirectRendererService::OnVideoNaturalSizeChange(const gfx::Size& size) {
  DVLOG(1) << __func__;
}

void DirectRendererService::OnVideoOpacityChange(bool opaque) {
  DVLOG(1) << __func__;
}

void DirectRendererService::OnVideoFrameRateChange(std::optional<int> fps) {
  DVLOG(1) << __func__;
}

void DirectRendererService::SchedulePeriodicMediaTimeUpdates() {
  UpdateMediaTime(/*force=*/true);
  time_update_timer_.Start(
      FROM_HERE, base::Milliseconds(250),
      base::BindRepeating(&DirectRendererService::UpdateMediaTime,
                          weak_factory_.GetWeakPtr(), /*force=*/false));
}

void DirectRendererService::CancelPeriodicMediaTimeUpdates() {
  time_update_timer_.Stop();
  UpdateMediaTime(/*force=*/false);
}

void DirectRendererService::UpdateMediaTime(bool force) {
  const base::TimeDelta media_time = renderer_->GetMediaTime();
  if (!force && media_time == last_media_time_) {
    return;
  }

  base::TimeDelta max_time = media_time;
  if (time_update_timer_.IsRunning() && (playback_rate_ > 0)) {
    max_time += base::Milliseconds(500);
  }

  if (client_ && client_task_runner_) {
    client_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&DirectRendererClient::OnTimeUpdate, client_, media_time,
                       max_time, base::TimeTicks::Now()));
  }
  last_media_time_ = media_time;
}

void DirectRendererService::OnVideoGeometryChange(
    const gfx::RectF& rect_f,
    gfx::OverlayTransform transform) {
  gfx::Rect new_bounds = gfx::ToEnclosingRect(rect_f);
  renderer_->OnVideoGeometryChange(new_bounds);
}

}  // namespace media
