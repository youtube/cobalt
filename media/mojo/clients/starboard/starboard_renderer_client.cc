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

#include "media/mojo/clients/starboard/starboard_renderer_client.h"

#include "base/functional/bind.h"
#include "base/unguessable_token.h"
#include "media/base/media_log.h"
#include "media/base/media_resource.h"
#include "media/base/video_frame.h"
#include "media/mojo/clients/mojo_renderer.h"
#include "media/renderers/video_overlay_factory.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace media {

StarboardRendererClient::StarboardRendererClient(
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
    std::unique_ptr<MediaLog> media_log,
    std::unique_ptr<MojoRenderer> mojo_renderer,
    std::unique_ptr<VideoOverlayFactory> video_overlay_factory,
    VideoRendererSink* video_renderer_sink,
    mojo::PendingRemote<RendererExtension> pending_renderer_extension,
    mojo::PendingReceiver<ClientExtension> client_extension_receiver,
    BindHostReceiverCallback bind_host_receiver_callback,
    GpuVideoAcceleratorFactories* gpu_factories)
    : media_task_runner_(media_task_runner),
      media_log_(std::move(media_log)),
      MojoRendererWrapper(std::move(mojo_renderer)),
      video_overlay_factory_(std::move(video_overlay_factory)),
      video_renderer_sink_(video_renderer_sink),
      pending_renderer_extension_(std::move(pending_renderer_extension)),
      pending_client_extension_receiver_(std::move(client_extension_receiver)),
      client_extension_receiver_(this),
      bind_host_receiver_callback_(bind_host_receiver_callback),
      gpu_factories_(gpu_factories) {
  DCHECK(media_task_runner_);
  DCHECK(video_renderer_sink_);
  DCHECK(video_overlay_factory_);
  DCHECK(bind_host_receiver_callback_);
  LOG(INFO) << "StarboardRendererClient constructed.";
}

StarboardRendererClient::~StarboardRendererClient() {
  SignalMediaPlayingStateChange(false);
}

void StarboardRendererClient::Initialize(MediaResource* media_resource,
                                         RendererClient* client,
                                         PipelineStatusCallback init_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  client_ = client;

  // Bind the receiver of VideoGeometryChangeSubscriber on renderer Thread.
  // This uses BindPostTaskToCurrentDefault() to ensure the callback is ran
  // on renderer thread, not media thread.
  bind_host_receiver_callback_.Run(
      video_geometry_change_subcriber_remote_.BindNewPipeAndPassReceiver());
  DCHECK(video_geometry_change_subcriber_remote_);
  video_geometry_change_subcriber_remote_->SubscribeToVideoGeometryChange(
      video_overlay_factory_->overlay_plane_id(),
      video_geometry_change_client_receiver_.BindNewPipeAndPassRemote(),
      base::BindOnce(&StarboardRendererClient::OnSubscribeToVideoGeometryChange,
                     base::Unretained(this), media_resource, client));

  DCHECK(!AreMojoPipesConnected());
  InitAndBindMojoRenderer(base::BindOnce(
      &StarboardRendererClient::InitializeMojoRenderer,
      weak_factory_.GetWeakPtr(), media_resource, client, std::move(init_cb)));
}

void StarboardRendererClient::InitAndBindMojoRenderer(
    base::OnceClosure complete_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!AreMojoPipesConnected());

  // Consume and bind the delayed PendingRemote and PendingReceiver now that
  // we are on |media_task_runner_|.
  renderer_extension_.Bind(std::move(pending_renderer_extension_),
                           media_task_runner_);
  client_extension_receiver_.Bind(std::move(pending_client_extension_receiver_),
                                  media_task_runner_);

  renderer_extension_.set_disconnect_handler(base::BindOnce(
      &StarboardRendererClient::OnConnectionError, base::Unretained(this)));

  // Generate |command_buffer_id|.
  mojom::CommandBufferIdPtr command_buffer_id;
  if (gpu_factories_) {
    gpu_factories_->GetChannelToken(
        base::BindOnce(&StarboardRendererClient::OnGpuChannelTokenReady,
                       weak_factory_.GetWeakPtr(), std::move(command_buffer_id),
                       std::move(complete_cb)));
    return;
  }

  DCHECK(complete_cb);
  InitAndConstructMojoRenderer(std::move(command_buffer_id),
                               std::move(complete_cb));
}

void StarboardRendererClient::InitializeMojoRenderer(
    MediaResource* media_resource,
    RendererClient* client,
    PipelineStatusCallback init_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(AreMojoPipesConnected());
  MojoRendererWrapper::Initialize(media_resource, client, std::move(init_cb));
}

void StarboardRendererClient::OnGpuChannelTokenReady(
    mojom::CommandBufferIdPtr command_buffer_id,
    base::OnceClosure complete_cb,
    const base::UnguessableToken& channel_token) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (channel_token) {
    command_buffer_id = mojom::CommandBufferId::New();
    command_buffer_id->channel_token = std::move(channel_token);
    command_buffer_id->route_id = gpu_factories_->GetCommandBufferRouteId();
  }
  InitAndConstructMojoRenderer(std::move(command_buffer_id),
                               std::move(complete_cb));
}

void StarboardRendererClient::InitAndConstructMojoRenderer(
    media::mojom::CommandBufferIdPtr command_buffer_id,
    base::OnceClosure complete_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(AreMojoPipesConnected());
  // Notify GpuChannelToken to StarboardRendererWrapper before
  // MojoRendererWrapper::Initialize(). Hence, StarboardRendererWrapper
  // should have |command_buffer_id| if available before
  // StarboardRendererWrapper::Initialize().
  renderer_extension_->OnGpuChannelTokenReady(std::move(command_buffer_id));
  std::move(complete_cb).Run();
}

void StarboardRendererClient::StartPlayingFrom(base::TimeDelta time) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  SignalMediaPlayingStateChange(true);
  next_video_frame_.reset();
  MojoRendererWrapper::StartPlayingFrom(time);
}

RendererType StarboardRendererClient::GetRendererType() {
  return RendererType::kStarboard;
}

void StarboardRendererClient::PaintVideoHoleFrame(const gfx::Size& size) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (rendering_mode_ == StarboardRenderingMode::kPunchOut) {
    video_renderer_sink_->PaintSingleFrame(
        video_overlay_factory_->CreateFrame(size));
  }
}

void StarboardRendererClient::UpdateStarboardRenderingMode(
    const StarboardRenderingMode mode) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  rendering_mode_ = mode;
  switch (rendering_mode_) {
    case StarboardRenderingMode::kPunchOut:
      // StarboardRenderingMode::kPunchOut doesn't update video
      // frame via VideoRendererSink::RenderCallback::Render().
      // The video frame is handled by Sbplayer, and render to its
      // surface directly.
      LOG(ERROR) << "Cobalt: " << __func__ << " kPunchOut";
      StopVideoRendererSink();
      break;
    case StarboardRenderingMode::kDecodeToTexture:
      // StarboardRenderingMode::kDecodeToTexture needs to update
      // video frame via VideoRendererSink::RenderCallback::Render().
      LOG(ERROR) << "Cobalt: " << __func__ << " kDecodeToTexture";
      StartVideoRendererSink();
      break;
    case StarboardRenderingMode::kInvalid:
      NOTREACHED() << "Invalid SbPlayer output mode";
      break;
  }
}

void StarboardRendererClient::OnVideoGeometryChange(
    const gfx::RectF& rect_f,
    gfx::OverlayTransform /* transform */) {
  gfx::Rect new_bounds = gfx::ToEnclosingRect(rect_f);
  renderer_extension_->OnVideoGeometryChange(new_bounds);
}

void StarboardRendererClient::OnError(PipelineStatus status) {
  SignalMediaPlayingStateChange(false);
  client_->OnError(status);
}

void StarboardRendererClient::OnFallback(PipelineStatus fallback) {
  SignalMediaPlayingStateChange(false);
  client_->OnFallback(std::move(fallback).AddHere());
}

void StarboardRendererClient::OnEnded() {
  SignalMediaPlayingStateChange(false);
  client_->OnEnded();
}

void StarboardRendererClient::OnStatisticsUpdate(
    const PipelineStatistics& stats) {
  client_->OnStatisticsUpdate(stats);
}

void StarboardRendererClient::OnBufferingStateChange(
    BufferingState state,
    BufferingStateChangeReason reason) {
  client_->OnBufferingStateChange(state, reason);
}

void StarboardRendererClient::OnWaiting(WaitingReason reason) {
  client_->OnWaiting(reason);
}

void StarboardRendererClient::OnAudioConfigChange(
    const AudioDecoderConfig& config) {
  client_->OnAudioConfigChange(config);
}

void StarboardRendererClient::OnVideoConfigChange(
    const VideoDecoderConfig& config) {
  client_->OnVideoConfigChange(config);
}

void StarboardRendererClient::OnVideoNaturalSizeChange(const gfx::Size& size) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  // DCHECK(has_video_);
  //  TODO(borongchen): we should refresh video_renderer_sink_

  client_->OnVideoNaturalSizeChange(size);
}

void StarboardRendererClient::OnVideoOpacityChange(bool opaque) {
  // DCHECK(has_video_);
  client_->OnVideoOpacityChange(opaque);
}

void StarboardRendererClient::OnVideoFrameRateChange(absl::optional<int> fps) {
  /*DCHECK(has_video_);

  if (fps.has_value()) {
    // We use microseconds as that is the max resolution of TimeDelta
    render_interval_ = base::Microseconds(1000000 / *fps);
  }*/

  client_->OnVideoFrameRateChange(fps);
}

void StarboardRendererClient::SignalMediaPlayingStateChange(bool is_playing) {
  // Skip if we are already in the same playing state
  if (is_playing == is_playing_) {
    return;
  }

  // Only start the render loop if we are in decode-to-texture mode
  if (rendering_mode_ == StarboardRenderingMode::kDecodeToTexture) {
    if (is_playing) {
      StartVideoRendererSink();
    } else {
      StopVideoRendererSink();
    }
  }
  is_playing_ = is_playing;
}

void StarboardRendererClient::OnConnectionError() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  MEDIA_LOG(ERROR, media_log_) << "StarboardRendererClient disconnected";
  client_->OnError(PIPELINE_ERROR_DISCONNECTED);
}

void StarboardRendererClient::OnSubscribeToVideoGeometryChange(
    MediaResource* media_resource,
    RendererClient* client) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
}

scoped_refptr<VideoFrame> StarboardRendererClient::Render(
    base::TimeTicks deadline_min,
    base::TimeTicks deadline_max,
    VideoRendererSink::RenderCallback::RenderingMode rendering_mode) {
  DCHECK(rendering_mode_ == StarboardRenderingMode::kDecodeToTexture);

  if (!media_task_runner_->RunsTasksInCurrentSequence()) {
    media_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&StarboardRendererClient::UpdateCurrentFrame,
                                  weak_factory_.GetWeakPtr()));
  }

  return next_video_frame_;
}

void StarboardRendererClient::UpdateCurrentFrame() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(rendering_mode_ == StarboardRenderingMode::kDecodeToTexture);
  renderer_extension_->Render(base::BindOnce(
      &StarboardRendererClient::OnRenderDone, weak_factory_.GetWeakPtr()));
}

void StarboardRendererClient::OnRenderDone(
    const scoped_refptr<VideoFrame>& frame) {
  if (frame.get() != nullptr) {
    next_video_frame_ = std::move(frame);
  } else {
    // LOG(ERROR) << "Cobalt: " << __func__ << " nullptr frame";
  }
}

void StarboardRendererClient::OnFrameDropped() {
  // no-op: dropped frame is handled by SbPlayer.
}

base::TimeDelta StarboardRendererClient::GetPreferredRenderInterval() {
  // TODO(b/375274109): Refine render interval for decode-to-texture mode.
  // This is at 60 fps for our render interval.
  return base::Microseconds(16666);
}

void StarboardRendererClient::StartVideoRendererSink() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (!video_renderer_sink_started_) {
    video_renderer_sink_started_ = true;
    video_renderer_sink_->Start(this);
  }
}

void StarboardRendererClient::StopVideoRendererSink() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (video_renderer_sink_started_) {
    video_renderer_sink_started_ = false;
    video_renderer_sink_->Stop();
  }
}

}  // namespace media
