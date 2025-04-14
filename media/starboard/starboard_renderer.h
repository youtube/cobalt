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
#include "media/starboard/sbplayer_bridge.h"
#include "media/starboard/sbplayer_set_bounds_helper.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {
using base::Time;
using base::TimeDelta;

// SbPlayer based Renderer implementation, the entry point for all video
// playbacks on Starboard platforms. Every Starboard renderer is usually
// owned by StarboardRendererWrapper and must live on a single
// thread/process/TaskRunner, usually Chrome_InProcGpuThread.
class MEDIA_EXPORT StarboardRenderer final : public Renderer,
                                             private SbPlayerBridge::Host {
 public:
  StarboardRenderer(scoped_refptr<base::SequencedTaskRunner> task_runner,
                    std::unique_ptr<MediaLog> media_log,
                    const base::UnguessableToken& overlay_plane_id,
                    TimeDelta audio_write_duration_local,
                    TimeDelta audio_write_duration_remote);

  // Disallow copy and assign.
  StarboardRenderer(const StarboardRenderer&) = delete;
  StarboardRenderer& operator=(const StarboardRenderer&) = delete;

  ~StarboardRenderer() final;

  // Renderer implementation.
  void Initialize(MediaResource* media_resource,
                  RendererClient* client,
                  PipelineStatusCallback init_cb) final;
  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) final;
  void SetLatencyHint(absl::optional<TimeDelta> latency_hint) final {
    // TODO(b/380935131): Consider to implement `LatencyHint` for SbPlayer.
    NOTIMPLEMENTED();
  }
  void SetPreservesPitch(bool preserves_pitch) final {
    LOG_IF(INFO, !preserves_pitch)
        << "SetPreservesPitch() with preserves_pitch=false is not supported.";
  }
  void SetWasPlayedWithUserActivation(
      bool was_played_with_user_activation) final {
    LOG_IF(INFO, was_played_with_user_activation)
        << "SetWasPlayedWithUserActivation() with "
           "was_played_with_user_activation=true is not supported.";
  }
  void Flush(base::OnceClosure flush_cb) final;
  void StartPlayingFrom(TimeDelta time) final;
  void SetPlaybackRate(double playback_rate) final;
  void SetVolume(float volume) final;
  TimeDelta GetMediaTime() final;
  void OnSelectedVideoTracksChanged(
      const std::vector<DemuxerStream*>& enabled_tracks,
      base::OnceClosure change_completed_cb) final {
    LOG(INFO) << "Track changes are not supported.";
    std::move(change_completed_cb).Run();
  }
  void OnEnabledAudioTracksChanged(
      const std::vector<DemuxerStream*>& enabled_tracks,
      base::OnceClosure change_completed_cb) final {
    LOG(INFO) << "Track changes are not supported.";
    std::move(change_completed_cb).Run();
  }
  RendererType GetRendererType() final { return RendererType::kStarboard; }

  using PaintVideoHoleFrameCallback =
      base::RepeatingCallback<void(const gfx::Size&)>;
  void set_paint_video_hole_frame_callback(
      PaintVideoHoleFrameCallback paint_video_hole_frame_cb);
  void OnVideoGeometryChange(const gfx::Rect& output_rect);

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
  void OnStatisticsUpdate(const PipelineStatistics& stats);

  void OnNeedData(DemuxerStream::Type type,
                  int max_number_of_buffers_to_write) final;
  void OnPlayerStatus(SbPlayerState state) final;
  void OnPlayerError(SbPlayerError error, const std::string& message) final;

  // Used to make a delayed call to OnNeedData() if |audio_read_delayed_| is
  // true. If |audio_read_delayed_| is false, that means the delayed call has
  // been cancelled due to a seek.
  void DelayedNeedData(int max_number_of_buffers_to_write);

  // Store the media time retrieved by GetMediaTime so we can cache it as an
  // estimate and avoid calling SbPlayerGetInfo too frequently.
  void StoreMediaTime(TimeDelta media_time);

  int GetDefaultMaxBuffers(AudioCodec codec,
                           TimeDelta duration_to_write,
                           bool is_preroll);

  int GetEstimatedMaxBuffers(TimeDelta write_duration,
                             TimeDelta time_ahead_of_playback,
                             bool is_preroll);

  State state_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<MediaLog> media_log_;

  raw_ptr<DemuxerStream> audio_stream_ = nullptr;
  raw_ptr<DemuxerStream> video_stream_ = nullptr;
  // TODO(b/375274109): Investigate whether we should call
  //                    `void OnVideoFrameRateChange(absl::optional<int> fps)`
  //                    on `client_`?
  raw_ptr<RendererClient> client_ = nullptr;
  PaintVideoHoleFrameCallback paint_video_hole_frame_cb_;

  // Temporary callback used for Initialize().
  PipelineStatusCallback init_cb_;

  scoped_refptr<SbPlayerSetBoundsHelper> set_bounds_helper_;

  raw_ptr<CdmContext> cdm_context_;

  DefaultSbPlayerInterface sbplayer_interface_;

  TimeDelta seek_time_;

  const TimeDelta audio_write_duration_local_;
  const TimeDelta audio_write_duration_remote_;
  // The two variables below should always contain the same value.  They are
  // kept as separate variables so we can keep the existing implementation as
  // is, which simplifies the implementation across multiple Starboard versions.
  TimeDelta audio_write_duration_;
  TimeDelta audio_write_duration_for_preroll_ = audio_write_duration_;
  // Only call GetMediaTime() from OnNeedData if it has been
  // |kMediaTimeCheckInterval| since the last call to GetMediaTime().
  static constexpr TimeDelta kMediaTimeCheckInterval = base::Microseconds(100);
  // Timestamp for the last written audio.
  TimeDelta timestamp_of_last_written_audio_;
  // Indicates if video end of stream has been written into the underlying
  // player.
  bool is_video_eos_written_ = false;
  TimeDelta last_audio_sample_interval_ = base::Microseconds(0);
  int last_estimated_max_buffers_for_preroll_ = 1;
  // Last media time reported by GetMediaTime().
  TimeDelta last_media_time_;
  // Timestamp microseconds when we last checked the media time.
  Time last_time_media_time_retrieved_;

  bool audio_read_delayed_ = false;
  // TODO(b/375674101): Support batched samples write.
  const int max_audio_samples_per_write_ = 1;

  SbDrmSystem drm_system_{kSbDrmSystemInvalid};

  mutable base::Lock lock_;
  // TODO: b/407063029 - Guard player_bridge_ and annotate with GUARDED_BY.
  std::unique_ptr<SbPlayerBridge> player_bridge_;

  bool player_bridge_initialized_ = false;
  std::optional<TimeDelta> playing_start_from_time_;

  base::OnceClosure pending_flush_cb_;

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

  // WeakPtrFactory should be defined last (after all member variables).
  base::WeakPtrFactory<StarboardRenderer> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_STARBOARD_STARBOARD_RENDERER_H_
