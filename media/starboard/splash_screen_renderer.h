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

#ifndef MEDIA_STARBOARD_SPLASH_SCREEN_RENDERER_H_
#define MEDIA_STARBOARD_SPLASH_SCREEN_RENDERER_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/renderer.h"
#include "media/starboard/sbplayer_bridge.h"
#include "media/starboard/sbplayer_interface.h"
#include "media/starboard/sbplayer_set_bounds_helper.h"
#include "starboard/window.h"

namespace media {

class SplashScreenRenderer : public Renderer, private SbPlayerBridge::Host {
 public:
  SplashScreenRenderer(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      SbWindow window);
  ~SplashScreenRenderer() override;

  // Renderer implementation.
  void Initialize(MediaResource* media_resource,
                  RendererClient* client,
                  PipelineStatusCallback init_cb) override;
  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) override;
  void SetLatencyHint(absl::optional<base::TimeDelta> latency_hint) override {}
  void SetPreservesPitch(bool preserves_pitch) override {}
  void SetWasPlayedWithUserActivation(
      bool was_played_with_user_activation) override {}
  void Flush(base::OnceClosure flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() override;
  RendererType GetRendererType() override;

 private:
  // SbPlayerBridge::Host implementation.
  void OnNeedData(DemuxerStream::Type type,
                  int max_number_of_samples_to_write) override;
  void OnPlayerStatus(SbPlayerState state) override;
  void OnPlayerError(SbPlayerError error, const std::string& message) override;

  void OnDemuxerStreamRead(DemuxerStream::Status status,
                           DemuxerStream::DecoderBufferVector buffers);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SbWindow window_;
  RendererClient* client_ = nullptr;
  DemuxerStream* video_stream_ = nullptr;

  DefaultSbPlayerInterface sbplayer_interface_;
  scoped_refptr<SbPlayerSetBoundsHelper> set_bounds_helper_;
  std::unique_ptr<SbPlayerBridge> player_bridge_;
};

}  // namespace media

#endif  // MEDIA_STARBOARD_SPLASH_SCREEN_RENDERER_H_
