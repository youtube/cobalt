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

#ifndef MEDIA_MOJO_CLIENTS_STARBOARD_DIRECT_RENDERER_H_
#define MEDIA_MOJO_CLIENTS_STARBOARD_DIRECT_RENDERER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_log.h"
#include "media/base/pipeline_status.h"
#include "media/base/renderer.h"
#include "media/base/renderer_client.h"
#include "media/base/starboard/starboard_renderer_config.h"
#include "media/base/starboard/starboard_rendering_mode.h"
#include "media/base/time_delta_interpolator.h"
#include "media/renderers/video_overlay_factory.h"

namespace media {

class DirectRendererService;
class VideoFrame;
class MediaResource;
class StarboardRendererClient;

class DirectRendererHost {
 public:
  virtual ~DirectRendererHost() = default;

  virtual void InitializeService(
      MediaResource* media_resource,
      base::OnceCallback<void(PipelineStatus)> callback) = 0;

  virtual void OnGpuChannelTokenReady(const base::UnguessableToken& token) = 0;
  virtual void GetCurrentVideoFrame(
      base::OnceCallback<void(scoped_refptr<VideoFrame>)> cb) = 0;
  virtual void OnSbWindowHandleReady(uint64_t sb_window_handle) = 0;

  // Forwarded from Renderer
  virtual void StartPlayingFrom(base::TimeDelta time) = 0;
  virtual void SetPlaybackRate(double playback_rate) = 0;
  virtual void SetVolume(float volume) = 0;
  virtual void Flush(base::OnceClosure flush_cb) = 0;
};

// Pure C++ interface for receiving callbacks from DirectRendererService.
class DirectRendererClient : public media::RendererClient {
 public:
  virtual ~DirectRendererClient() = default;

  virtual void OnTimeUpdate(base::TimeDelta time,
                            base::TimeDelta max_time,
                            base::TimeTicks capture_time) = 0;

  virtual void PaintVideoHoleFrame(const gfx::Size& size) = 0;
  virtual void UpdateStarboardRenderingMode(StarboardRenderingMode mode) = 0;
  virtual void GetSbWindowHandle() = 0;
#if BUILDFLAG(IS_ANDROID)
  virtual void RequestOverlayInfo(bool restart_for_transitions) = 0;
#endif
};

class DirectRenderer : public Renderer, public DirectRendererClient {
 public:
  DirectRenderer(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& gpu_task_runner,
      const StarboardRendererConfig& config,
      MediaLog* media_log,
      void* subscriber_remote);

  DirectRenderer(const DirectRenderer&) = delete;
  DirectRenderer& operator=(const DirectRenderer&) = delete;

  ~DirectRenderer() override;

  void ClearDirectClient();

  // Renderer implementation.
  void Initialize(MediaResource* media_resource,
                  media::RendererClient* client,
                  PipelineStatusCallback init_cb) override;
  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) override;
  void SetLatencyHint(std::optional<base::TimeDelta> latency_hint) override;
  void Flush(base::OnceClosure flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() override;
  RendererType GetRendererType() override;

  // DirectRendererClient implementation.
  void OnTimeUpdate(base::TimeDelta time,
                    base::TimeDelta max_time,
                    base::TimeTicks capture_time) override;
  void OnStatisticsUpdate(const PipelineStatistics& stats) override;
  void OnBufferingStateChange(BufferingState state,
                              BufferingStateChangeReason reason) override;
  void OnError(PipelineStatus status) override;
  void OnFallback(PipelineStatus status) override;
  void OnEnded() override;
  void OnWaiting(WaitingReason reason) override;
  void OnAudioConfigChange(const AudioDecoderConfig& config) override;
  void OnVideoConfigChange(const VideoDecoderConfig& config) override;
  void OnVideoNaturalSizeChange(const gfx::Size& size) override;
  void OnVideoOpacityChange(bool opaque) override;
  void OnVideoFrameRateChange(std::optional<int> fps) override;

  void PaintVideoHoleFrame(const gfx::Size& size) override;
  void UpdateStarboardRenderingMode(StarboardRenderingMode mode) override;
  void GetSbWindowHandle() override;
#if BUILDFLAG(IS_ANDROID)
  void RequestOverlayInfo(bool restart_for_transitions) override;
#endif

 private:
  void OnServiceCreated(MediaResource* media_resource,
                        PipelineStatusCallback init_cb,
                        DirectRendererHost* service);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<base::SequencedTaskRunner> gpu_task_runner_;
  StarboardRendererConfig config_;
  raw_ptr<MediaLog> media_log_ = nullptr;
  void* subscriber_remote_ = nullptr;

  raw_ptr<media::RendererClient> client_ = nullptr;
  base::WeakPtr<StarboardRendererClient> direct_client_;
  raw_ptr<DirectRendererHost> service_ = nullptr;

  base::Lock lock_;
  std::unique_ptr<VideoOverlayFactory> video_overlay_factory_;
  media::TimeDeltaInterpolator media_time_interpolator_;
  base::WeakPtrFactory<DirectRenderer> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_STARBOARD_DIRECT_RENDERER_H_
