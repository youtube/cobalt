// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "media/starboard/splash_screen_renderer.h"

#include "base/functional/bind.h"
#include "base/task/bind_post_task.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_resource.h"
#include "media/base/renderer_client.h"

namespace media {

SplashScreenRenderer::SplashScreenRenderer(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    SbWindow window)
    : task_runner_(task_runner),
      window_(window),
      set_bounds_helper_(new SbPlayerSetBoundsHelper) {}

SplashScreenRenderer::~SplashScreenRenderer() = default;

void SplashScreenRenderer::Initialize(MediaResource* media_resource,
                                      RendererClient* client,
                                      PipelineStatusCallback init_cb) {
  client_ = client;
  video_stream_ = media_resource->GetFirstStream(DemuxerStream::VIDEO);

  player_bridge_.reset(new SbPlayerBridge(
      &sbplayer_interface_, task_runner_,
      base::NullCallback(),  // GetDecodeTargetGraphicsContextProviderFunc
      AudioDecoderConfig(), "", video_stream_->video_decoder_config(),
      "video/webm", window_, kSbDrmSystemInvalid, this,
      set_bounds_helper_.get(),
      false,  // allow_resume_after_suspend
      kSbPlayerOutputModePunchOut, "", -1));

  std::move(init_cb).Run(PIPELINE_OK);
}

void SplashScreenRenderer::SetCdm(CdmContext* cdm_context,
                                  CdmAttachedCB cdm_attached_cb) {
  // Not supported.
  std::move(cdm_attached_cb).Run(false);
}

void SplashScreenRenderer::Flush(base::OnceClosure flush_cb) {
  std::move(flush_cb).Run();
}

void SplashScreenRenderer::StartPlayingFrom(base::TimeDelta time) {
  player_bridge_->SetPlaybackRate(1.0);
}

void SplashScreenRenderer::SetPlaybackRate(double playback_rate) {
  player_bridge_->SetPlaybackRate(playback_rate);
}

void SplashScreenRenderer::SetVolume(float volume) {
  player_bridge_->SetVolume(volume);
}

base::TimeDelta SplashScreenRenderer::GetMediaTime() {
  return player_bridge_ ? player_bridge_->GetMediaTime() : base::TimeDelta();
}

RendererType SplashScreenRenderer::GetRendererType() {
  return RendererType::kStarboard;
}

void SplashScreenRenderer::OnNeedData(DemuxerStream::Type type,
                                      int max_number_of_samples_to_write) {
  if (type == DemuxerStream::VIDEO) {
    video_stream_->Read(1, base::BindPostTaskToCurrentDefault(base::BindOnce(
                               &SplashScreenRenderer::OnDemuxerStreamRead,
                               base::Unretained(this))));
  }
}

void SplashScreenRenderer::OnPlayerStatus(SbPlayerState state) {
  if (state == kSbPlayerStateEndOfStream) {
    client_->OnEnded();
  }
}

void SplashScreenRenderer::OnPlayerError(SbPlayerError error,
                                         const std::string& message) {
  client_->OnError(PIPELINE_ERROR_DECODE);
}

void SplashScreenRenderer::OnDemuxerStreamRead(
    DemuxerStream::Status status,
    DemuxerStream::DecoderBufferVector buffers) {
  if (status != DemuxerStream::kOk) {
    return;
  }
  player_bridge_->WriteBuffers(DemuxerStream::VIDEO, buffers);
}

}  // namespace media
