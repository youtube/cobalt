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

#include "media/mojo/clients/starboard/direct_renderer.h"

#include "base/functional/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/time/default_tick_clock.h"
#include "media/base/starboard/direct_renderer_service_factory.h"
#include "media/mojo/clients/starboard/starboard_renderer_client.h"

namespace media {

DirectRenderer::DirectRenderer(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& gpu_task_runner,
    const StarboardRendererConfig& config,
    MediaLog* media_log,
    void* subscriber_remote)
    : task_runner_(task_runner),
      gpu_task_runner_(gpu_task_runner),
      config_(config),
      media_log_(media_log),
      subscriber_remote_(subscriber_remote),
      media_time_interpolator_(base::DefaultTickClock::GetInstance()) {
  DVLOG(1) << __func__;
}

DirectRenderer::~DirectRenderer() {
  DVLOG(1) << __func__;
  if (service_) {
    gpu_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce([](DirectRendererHost* service) { delete service; },
                       base::Unretained(service_)));
  }
}

void DirectRenderer::ClearDirectClient() {
  base::AutoLock lock(lock_);
  direct_client_ = nullptr;
}

void DirectRenderer::Initialize(MediaResource* media_resource,
                                media::RendererClient* client,
                                PipelineStatusCallback init_cb) {
  DVLOG(1) << __func__;
  client_ = client;
  direct_client_ = static_cast<StarboardRendererClient*>(client)->GetWeakPtr();

  auto create_service_cb = GetCreateDirectRendererServiceCallback();
  LOG(INFO) << "DirectRenderer::Initialize: create_service_cb is_null="
            << create_service_cb.is_null();
  if (create_service_cb.is_null()) {
    DLOG(ERROR) << "CreateServiceCallback is not set!";
    std::move(init_cb).Run(PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

  std::vector<DemuxerStream*> streams = media_resource->GetAllStreams();

  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          create_service_cb, weak_factory_.GetWeakPtr(), task_runner_, streams,
          config_, media_log_ ? media_log_->Clone() : nullptr,
          subscriber_remote_,
          base::BindPostTaskToCurrentDefault(base::BindOnce(
              &DirectRenderer::OnServiceCreated, weak_factory_.GetWeakPtr(),
              base::Unretained(media_resource), std::move(init_cb)))));
}

void DirectRenderer::OnServiceCreated(MediaResource* media_resource,
                                      PipelineStatusCallback init_cb,
                                      DirectRendererHost* service) {
  LOG(INFO) << "DirectRenderer::OnServiceCreated called";
  service_ = service;
  if (!service_) {
    std::move(init_cb).Run(PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DirectRendererHost::InitializeService,
                     base::Unretained(service_),
                     base::Unretained(media_resource),
                     base::BindPostTaskToCurrentDefault(std::move(init_cb))));
}

void DirectRenderer::SetCdm(CdmContext* cdm_context,
                            CdmAttachedCB cdm_attached_cb) {
  DVLOG(1) << __func__;
  std::move(cdm_attached_cb).Run(true);
}

void DirectRenderer::SetLatencyHint(
    std::optional<base::TimeDelta> latency_hint) {
  DVLOG(1) << __func__;
}

void DirectRenderer::Flush(base::OnceClosure flush_cb) {
  DVLOG(1) << __func__;
  if (service_) {
    gpu_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &DirectRendererHost::Flush, base::Unretained(service_),
            base::BindPostTaskToCurrentDefault(std::move(flush_cb))));
  } else {
    std::move(flush_cb).Run();
  }
}

void DirectRenderer::StartPlayingFrom(base::TimeDelta time) {
  DVLOG(1) << __func__;
  {
    base::AutoLock auto_lock(lock_);
    media_time_interpolator_.SetBounds(time, time, base::TimeTicks::Now());
  }
  if (service_) {
    gpu_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DirectRendererHost::StartPlayingFrom,
                                  base::Unretained(service_), time));
  }
}

void DirectRenderer::SetPlaybackRate(double playback_rate) {
  DVLOG(1) << __func__;
  {
    base::AutoLock auto_lock(lock_);
    media_time_interpolator_.SetPlaybackRate(playback_rate);
  }
  if (service_) {
    gpu_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DirectRendererHost::SetPlaybackRate,
                                  base::Unretained(service_), playback_rate));
  }
}

void DirectRenderer::SetVolume(float volume) {
  DVLOG(1) << __func__;
  if (service_) {
    gpu_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DirectRendererHost::SetVolume,
                                  base::Unretained(service_), volume));
  }
}

base::TimeDelta DirectRenderer::GetMediaTime() {
  base::AutoLock auto_lock(lock_);
  return media_time_interpolator_.GetInterpolatedTime();
}

RendererType DirectRenderer::GetRendererType() {
  return RendererType::kStarboard;
}

void DirectRenderer::OnTimeUpdate(base::TimeDelta time,
                                  base::TimeDelta max_time,
                                  base::TimeTicks capture_time) {
  base::AutoLock auto_lock(lock_);
  media_time_interpolator_.SetBounds(time, max_time, capture_time);
}

void DirectRenderer::OnStatisticsUpdate(const PipelineStatistics& stats) {
  if (client_) {
    client_->OnStatisticsUpdate(stats);
  }
}

void DirectRenderer::OnBufferingStateChange(BufferingState state,
                                            BufferingStateChangeReason reason) {
  if (client_) {
    client_->OnBufferingStateChange(state, reason);
  }
}

void DirectRenderer::OnError(PipelineStatus status) {
  if (client_) {
    client_->OnError(status);
  }
}

void DirectRenderer::OnFallback(PipelineStatus status) {
  if (client_) {
    client_->OnFallback(status);
  }
}

void DirectRenderer::OnEnded() {
  if (client_) {
    client_->OnEnded();
  }
}

void DirectRenderer::OnWaiting(WaitingReason reason) {
  if (client_) {
    client_->OnWaiting(reason);
  }
}

void DirectRenderer::OnAudioConfigChange(const AudioDecoderConfig& config) {
  if (client_) {
    client_->OnAudioConfigChange(config);
  }
}

void DirectRenderer::OnVideoConfigChange(const VideoDecoderConfig& config) {
  if (client_) {
    client_->OnVideoConfigChange(config);
  }
}

void DirectRenderer::OnVideoNaturalSizeChange(const gfx::Size& size) {
  if (client_) {
    client_->OnVideoNaturalSizeChange(size);
  }
}

void DirectRenderer::OnVideoOpacityChange(bool opaque) {
  if (client_) {
    client_->OnVideoOpacityChange(opaque);
  }
}

void DirectRenderer::OnVideoFrameRateChange(std::optional<int> fps) {
  if (client_) {
    client_->OnVideoFrameRateChange(fps);
  }
}

void DirectRenderer::PaintVideoHoleFrame(const gfx::Size& size) {
  base::AutoLock lock(lock_);
  if (direct_client_) {
    direct_client_->PaintVideoHoleFrame(size);
  }
}

void DirectRenderer::UpdateStarboardRenderingMode(StarboardRenderingMode mode) {
  base::AutoLock lock(lock_);
  if (direct_client_) {
    direct_client_->UpdateStarboardRenderingMode(mode);
  }
}

void DirectRenderer::GetSbWindowHandle() {
  base::AutoLock lock(lock_);
  if (direct_client_) {
    direct_client_->GetSbWindowHandle();
  }
}

#if BUILDFLAG(IS_ANDROID)
void DirectRenderer::RequestOverlayInfo(bool restart_for_transitions) {
  if (direct_client_) {
    direct_client_->RequestOverlayInfo(restart_for_transitions);
  }
}
#endif

}  // namespace media
