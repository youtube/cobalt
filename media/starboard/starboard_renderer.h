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

#ifndef MEDIA_STARBOARD_STARBOARD_RENDERER_H_
#define MEDIA_STARBOARD_STARBOARD_RENDERER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_log.h"
#include "media/base/media_resource.h"
#include "media/base/pipeline_status.h"
#include "media/base/renderer.h"
#include "media/base/renderer_client.h"
#include "media/base/video_renderer_sink.h"
#include "media/renderers/video_overlay_factory.h"
#include "media/starboard/sbplayer_bridge.h"
#include "media/starboard/sbplayer_set_bounds_helper.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

// SbPlayer based Renderer implementation, the entry point for all video
// playbacks on Starboard platforms.
class MEDIA_EXPORT StarboardRenderer final : public Renderer,
                                             private SbPlayerBridge::Host {
 public:
  StarboardRenderer(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                    VideoRendererSink* video_renderer_sink,
                    MediaLog* media_log);

  ~StarboardRenderer() final;

  // Renderer implementation.
  void Initialize(MediaResource* media_resource,
                  RendererClient* client,
                  PipelineStatusCallback init_cb) final;
  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) final;
  void SetLatencyHint(absl::optional<base::TimeDelta> latency_hint) final {
    // TODO(b/375271848): Address NOTIMPLEMENTED().
    NOTIMPLEMENTED();
  }
  void SetPreservesPitch(bool preserves_pitch) final {
    // TODO(b/375271848): Address NOTIMPLEMENTED().
    NOTIMPLEMENTED();
  }
  void SetWasPlayedWithUserActivation(
      bool was_played_with_user_activation) final {
    // TODO(b/375271848): Address NOTIMPLEMENTED().
    NOTIMPLEMENTED();
  }
  void Flush(base::OnceClosure flush_cb) final;
  void StartPlayingFrom(base::TimeDelta time) final;
  void SetPlaybackRate(double playback_rate) final;
  void SetVolume(float volume) final;
  base::TimeDelta GetMediaTime() final;
  void OnSelectedVideoTracksChanged(
      const std::vector<DemuxerStream*>& enabled_tracks,
      base::OnceClosure change_completed_cb) final {
    // TODO(b/375271848): Address NOTIMPLEMENTED().
    NOTIMPLEMENTED();
  }
  void OnEnabledAudioTracksChanged(
      const std::vector<DemuxerStream*>& enabled_tracks,
      base::OnceClosure change_completed_cb) final {
    // TODO(b/375271848): Address NOTIMPLEMENTED().
    NOTIMPLEMENTED();
  }
  RendererType GetRendererType() final { return RendererType::kStarboard; }
  SetBoundsCB GetSetBoundsCB() override;

 private:
  enum State {
    STATE_UNINITIALIZED,
    STATE_INIT_PENDING_CDM,  // Initialization is waiting for the CDM to be set.
    STATE_INITIALIZING,      // Initializing to create SbPlayerBridge.
    STATE_FLUSHED,           // After initialization or after flush completed.
    STATE_PLAYING,           // After StartPlayingFrom has been called.
    STATE_ERROR
  };

  void CreatePlayerBridge();
  void UpdateDecoderConfig(DemuxerStream* stream);
  void OnDemuxerStreamRead(DemuxerStream* stream,
                           DemuxerStream::Status status,
                           DemuxerStream::DecoderBufferVector buffers);

  void OnNeedData(DemuxerStream::Type type,
                  int max_number_of_buffers_to_write) final;
  void OnPlayerStatus(SbPlayerState state) final;
  void OnPlayerError(SbPlayerError error, const std::string& message) final;

  State state_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const raw_ptr<VideoRendererSink> video_renderer_sink_;
  const raw_ptr<MediaLog> media_log_;

  raw_ptr<DemuxerStream> audio_stream_ = nullptr;
  raw_ptr<DemuxerStream> video_stream_ = nullptr;
  // TODO(b/375274109): Investigate whether we should call
  //                    `void OnVideoFrameRateChange(absl::optional<int> fps)`
  //                    on `client_`?
  raw_ptr<RendererClient> client_ = nullptr;

  // Temporary callback used for Initialize().
  PipelineStatusCallback init_cb_;

  // Overlay factory used to create overlays for video frames rendered
  // by the remote renderer.
  std::unique_ptr<VideoOverlayFactory> video_overlay_factory_;

  scoped_refptr<SbPlayerSetBoundsHelper> set_bounds_helper_;

  raw_ptr<CdmContext> cdm_context_;

  DefaultSbPlayerInterface sbplayer_interface_;
  // TODO(b/326652276): Support audio write duration.
  const base::TimeDelta audio_write_duration_local_ = base::Milliseconds(500);
  const base::TimeDelta audio_write_duration_remote_ = base::Seconds(10);
  // TODO(b/375674101): Support batched samples write.
  const int max_audio_samples_per_write_ = 1;

  SbDrmSystem drm_system_{kSbDrmSystemInvalid};

  base::Lock lock_;
  std::unique_ptr<SbPlayerBridge> player_bridge_;

  bool player_bridge_initialized_ = false;
  std::optional<base::TimeDelta> playing_start_from_time_;

  base::OnceClosure pending_flush_cb_;

  base::TimeDelta audio_write_duration_ = audio_write_duration_local_;

  bool audio_read_in_progress_ = false;
  bool video_read_in_progress_ = false;

  double playback_rate_ = 0.;
  float volume_ = 1.0f;

  uint32_t last_video_frames_decoded_ = 0;
  uint32_t last_video_frames_dropped_ = 0;

  // Message to signal a capability changed error.
  // "MEDIA_ERR_CAPABILITY_CHANGED" must be in the error message to be
  // understood as a capability changed error. Do not change this message.
  static inline constexpr const char* kSbPlayerCapabilityChangedErrorMessage =
      "MEDIA_ERR_CAPABILITY_CHANGED";

  base::WeakPtrFactory<StarboardRenderer> weak_factory_{this};
  base::WeakPtr<StarboardRenderer> weak_this_{weak_factory_.GetWeakPtr()};

  StarboardRenderer(const StarboardRenderer&) = delete;
  StarboardRenderer& operator=(const StarboardRenderer&) = delete;
};

}  // namespace media

#endif  // MEDIA_STARBOARD_STARBOARD_RENDERER_H_
