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

#ifndef MEDIA_MOJO_SERVICES_STARBOARD_DIRECT_RENDERER_SERVICE_H_
#define MEDIA_MOJO_SERVICES_STARBOARD_DIRECT_RENDERER_SERVICE_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "cobalt/media/service/mojom/video_geometry_setter.mojom.h"
#include "media/base/renderer.h"
#include "media/base/renderer_client.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace cobalt {
namespace media {
class VideoGeometrySetterService;
}
}  // namespace cobalt
#include "media/base/starboard/starboard_renderer_config.h"
#include "media/mojo/clients/starboard/direct_renderer.h"
#include "media/starboard/starboard_renderer.h"

namespace media {

class DirectRendererClient;
class VideoFrame;

class DirectRendererService
    : public ::media::Renderer,
      public ::media::DirectRendererHost,
      public ::media::RendererClient,
      public ::cobalt::media::mojom::VideoGeometryChangeClient {
 public:
  static void Create(
      base::WeakPtr<DirectRendererClient> client,
      const scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
      const std::vector<DemuxerStream*>& streams,
      const StarboardRendererConfig& config,
      std::unique_ptr<MediaLog> media_log,
      void* subscriber_remote,
      base::OnceCallback<void(DirectRendererHost*)> callback);

  static void SetVideoGeometrySetterService(
      ::cobalt::media::VideoGeometrySetterService* service);
  static ::cobalt::media::VideoGeometrySetterService*
  GetVideoGeometrySetterService();

  DirectRendererService(
      std::unique_ptr<StarboardRenderer> renderer,
      base::WeakPtr<DirectRendererClient> client,
      const scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const base::UnguessableToken& overlay_plane_id,
      mojo::PendingRemote<::cobalt::media::mojom::VideoGeometryChangeSubscriber>
          subscriber_remote);

  DirectRendererService(const DirectRendererService&) = delete;
  DirectRendererService& operator=(const DirectRendererService&) = delete;

  void SetCallbacks();

  ~DirectRendererService() override;

  // Renderer implementation.
  void Initialize(MediaResource* media_resource,
                  RendererClient* client,
                  PipelineStatusCallback init_cb) override;
  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) override;
  void SetLatencyHint(std::optional<base::TimeDelta> latency_hint) override;
  void Flush(base::OnceClosure flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() override;
  RendererType GetRendererType() override;

  // DirectRendererHost implementation.
  void InitializeService(
      MediaResource* media_resource,
      base::OnceCallback<void(PipelineStatus)> callback) override;
  void OnGpuChannelTokenReady(const base::UnguessableToken& token) override;
  void GetCurrentVideoFrame(
      base::OnceCallback<void(scoped_refptr<VideoFrame>)> cb) override;
  void OnSbWindowHandleReady(uint64_t sb_window_handle) override;

  // cobalt::media::mojom::VideoGeometryChangeClient implementation.
  void OnVideoGeometryChange(const gfx::RectF& rect_f,
                             gfx::OverlayTransform transform) override;

  // RendererClient implementation (to receive from StarboardRenderer).
  void OnError(PipelineStatus status) override;
  void OnFallback(PipelineStatus status) override;
  void OnEnded() override;
  void OnStatisticsUpdate(const PipelineStatistics& stats) override;
  void OnBufferingStateChange(BufferingState state,
                              BufferingStateChangeReason reason) override;
  void OnWaiting(WaitingReason reason) override;
  void OnAudioConfigChange(const AudioDecoderConfig& config) override;
  void OnVideoConfigChange(const VideoDecoderConfig& config) override;
  void OnVideoNaturalSizeChange(const gfx::Size& size) override;
  void OnVideoOpacityChange(bool opaque) override;
  void OnVideoFrameRateChange(std::optional<int> fps) override;

 private:
  void OnPaintVideoHoleFrameByStarboard(const gfx::Size& size);
  void OnUpdateStarboardRenderingModeByStarboard(
      const StarboardRenderingMode mode);
  void OnGetSbWindowHandle();
#if BUILDFLAG(IS_ANDROID)
  void OnRequestOverlayInfoByStarboard(bool restart_for_transitions);
#endif

  void SchedulePeriodicMediaTimeUpdates();
  void CancelPeriodicMediaTimeUpdates();
  void UpdateMediaTime(bool force);

  std::unique_ptr<StarboardRenderer> renderer_;
  base::WeakPtr<DirectRendererClient> client_;
  scoped_refptr<base::SequencedTaskRunner> client_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::RepeatingTimer time_update_timer_;
  base::TimeDelta last_media_time_;
  double playback_rate_ = 0;

  mojo::Receiver<::cobalt::media::mojom::VideoGeometryChangeClient>
      video_geometry_change_receiver_{this};
  mojo::Remote<::cobalt::media::mojom::VideoGeometryChangeSubscriber>
      video_geometry_change_subscriber_remote_;

  base::WeakPtrFactory<DirectRendererService> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_STARBOARD_DIRECT_RENDERER_SERVICE_H_
