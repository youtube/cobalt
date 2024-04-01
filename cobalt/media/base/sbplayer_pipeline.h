// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_BASE_SBPLAYER_PIPELINE_H_
#define COBALT_MEDIA_BASE_SBPLAYER_PIPELINE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "cobalt/base/c_val.h"
#include "cobalt/math/size.h"
#include "cobalt/media/base/media_export.h"
#include "cobalt/media/base/metrics_provider.h"
#include "cobalt/media/base/pipeline.h"
#include "cobalt/media/base/playback_statistics.h"
#include "cobalt/media/base/sbplayer_bridge.h"
#include "cobalt/media/base/sbplayer_set_bounds_helper.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_log.h"
#include "media/base/pipeline_status.h"
#include "media/base/ranges.h"
#include "media/base/video_decoder_config.h"
#include "starboard/configuration_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cobalt {
namespace media {

using base::Time;
using base::TimeDelta;

// SbPlayerPipeline is a PipelineBase implementation that uses the SbPlayer
// interface internally.
class MEDIA_EXPORT SbPlayerPipeline : public Pipeline,
                                      public ::media::DemuxerHost,
                                      public SbPlayerBridge::Host {
 public:
  // Constructs a media pipeline that will execute on |task_runner|.
  SbPlayerPipeline(SbPlayerInterface* interface, PipelineWindow window,
                   const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                   const GetDecodeTargetGraphicsContextProviderFunc&
                       get_decode_target_graphics_context_provider_func,
                   bool allow_resume_after_suspend,
                   bool allow_batched_sample_write,
                   bool force_punch_out_by_default,
#if SB_API_VERSION >= 15
                   TimeDelta audio_write_duration_local,
                   TimeDelta audio_write_duration_remote,
#endif  // SB_API_VERSION >= 15
                   MediaLog* media_log,
                   MediaMetricsProvider* media_metrics_provider,
                   DecodeTargetProvider* decode_target_provider);
  ~SbPlayerPipeline() override;

  void Suspend() override;
  // TODO: This is temporary for supporting background media playback.
  //       Need to be removed with media refactor.
  void Resume(PipelineWindow window) override;

  void Start(Demuxer* demuxer,
             const SetDrmSystemReadyCB& set_drm_system_ready_cb,
             const PipelineStatusCB& ended_cb, const ErrorCB& error_cb,
             const SeekCB& seek_cb, const BufferingStateCB& buffering_state_cb,
             const base::Closure& duration_change_cb,
             const base::Closure& output_mode_change_cb,
             const base::Closure& content_size_change_cb,
             const std::string& max_video_capabilities,
             const int max_video_input_size) override;
#if SB_HAS(PLAYER_WITH_URL)
  void Start(const SetDrmSystemReadyCB& set_drm_system_ready_cb,
             const OnEncryptedMediaInitDataEncounteredCB&
                 encrypted_media_init_data_encountered_cb,
             const std::string& source_url, const PipelineStatusCB& ended_cb,
             const ErrorCB& error_cb, const SeekCB& seek_cb,
             const BufferingStateCB& buffering_state_cb,
             const base::Closure& duration_change_cb,
             const base::Closure& output_mode_change_cb,
             const base::Closure& content_size_change_cb) override;
#endif  // SB_HAS(PLAYER_WITH_URL)

  void Stop(const base::Closure& stop_cb) override;
  void Seek(TimeDelta time, const SeekCB& seek_cb);
  bool HasAudio() const override;
  bool HasVideo() const override;

  float GetPlaybackRate() const override;
  void SetPlaybackRate(float playback_rate) override;
  float GetVolume() const override;
  void SetVolume(float volume) override;

  TimeDelta GetMediaTime() override;
  ::media::Ranges<TimeDelta> GetBufferedTimeRanges() override;
  TimeDelta GetMediaDuration() const override;
#if SB_HAS(PLAYER_WITH_URL)
  TimeDelta GetMediaStartDate() const override;
#endif  // SB_HAS(PLAYER_WITH_URL)
  void GetNaturalVideoSize(gfx::Size* out_size) const override;
  std::vector<std::string> GetAudioConnectors() const override;

  bool DidLoadingProgress() const override;
  PipelineStatistics GetStatistics() const override;
  SetBoundsCB GetSetBoundsCB() override;
  void SetPreferredOutputModeToDecodeToTexture() override;

 private:
  // Used to post parameters to SbPlayerPipeline::StartTask() as the number of
  // parameters exceed what base::Bind() can support.
  struct StartTaskParameters {
    ::media::Demuxer* demuxer;
    SetDrmSystemReadyCB set_drm_system_ready_cb;
    ::media::PipelineStatusCB ended_cb;
    ErrorCB error_cb;
    Pipeline::SeekCB seek_cb;
    Pipeline::BufferingStateCB buffering_state_cb;
    base::Closure duration_change_cb;
    base::Closure output_mode_change_cb;
    base::Closure content_size_change_cb;
    std::string max_video_capabilities;
    int max_video_input_size;
#if SB_HAS(PLAYER_WITH_URL)
    std::string source_url;
    bool is_url_based;
#endif  // SB_HAS(PLAYER_WITH_URL)
  };

  void StartTask(StartTaskParameters parameters);
  void SetVolumeTask(float volume);
  void SetPlaybackRateTask(float volume);
  void SetDurationTask(TimeDelta duration);

  // DemuxerHost implementation.
  void OnBufferedTimeRangesChanged(
      const ::media::Ranges<TimeDelta>& ranges) override;
  void SetDuration(TimeDelta duration) override;
  void OnDemuxerError(PipelineStatus error) override;

#if SB_HAS(PLAYER_WITH_URL)
  void CreateUrlPlayer(const std::string& source_url);
  void SetDrmSystem(SbDrmSystem drm_system);
#endif  // SB_HAS(PLAYER_WITH_URL)
  void CreatePlayer(SbDrmSystem drm_system);

  void OnDemuxerInitialized(PipelineStatus status);
  void OnDemuxerSeeked(PipelineStatus status);
  void OnDemuxerStopped();
  void OnDemuxerStreamRead(
      ::media::DemuxerStream::Type type, int max_number_buffers_to_read,
      ::media::DemuxerStream::Status status,
      const std::vector<scoped_refptr<::media::DecoderBuffer>>& buffers);
  // SbPlayerBridge::Host implementation.
  void OnNeedData(::media::DemuxerStream::Type type,
                  int max_number_of_buffers_to_write) override;
  void OnPlayerStatus(SbPlayerState state) override;
  void OnPlayerError(SbPlayerError error, const std::string& message) override;

  // Used to make a delayed call to OnNeedData() if |audio_read_delayed_| is
  // true. If |audio_read_delayed_| is false, that means the delayed call has
  // been cancelled due to a seek.
  void DelayedNeedData(int max_number_of_buffers_to_write);

  void UpdateDecoderConfig(::media::DemuxerStream* stream);
  void CallSeekCB(PipelineStatus status, const std::string& error_message);
  void CallErrorCB(PipelineStatus status, const std::string& error_message);

  void SuspendTask(base::WaitableEvent* done_event);
  void ResumeTask(PipelineWindow window, base::WaitableEvent* done_event);

  // Store the media time retrieved by GetMediaTime so we can cache it as an
  // estimate and avoid calling SbPlayerGetInfo too frequently.
  void StoreMediaTime(TimeDelta media_time);

  // Retrieve the statistics as a string and append to message.
  std::string AppendStatisticsString(const std::string& message) const;

  // Get information on the time since app start and the time since the last
  // playback resume.
  std::string GetTimeInformation() const;

  void RunSetDrmSystemReadyCB(DrmSystemReadyCB drm_system_ready_cb);

  void SetReadInProgress(::media::DemuxerStream::Type type, bool in_progress);
  bool GetReadInProgress(::media::DemuxerStream::Type type) const;

  // An identifier string for the pipeline, used in CVal to identify multiple
  // pipelines.
  const std::string pipeline_identifier_;

  // A wrapped interface of starboard player functions, which will be used in
  // underlying SbPlayerBridge.
  SbPlayerInterface* sbplayer_interface_;

  // Message loop used to execute pipeline tasks.  It is thread-safe.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Whether we should save DecoderBuffers for resume after suspend.
  const bool allow_resume_after_suspend_;

  // Whether we enable batched sample write functionality.
  const bool allow_batched_sample_write_;

  // The default output mode passed to `SbPlayerGetPreferredOutputMode()`.
  SbPlayerOutputMode default_output_mode_ = kSbPlayerOutputModeInvalid;

  // The window this player associates with.  It should only be assigned in the
  // dtor and accessed once by SbPlayerCreate().
  PipelineWindow window_;

  // Call to get the SbDecodeTargetGraphicsContextProvider for SbPlayerCreate().
  const GetDecodeTargetGraphicsContextProviderFunc
      get_decode_target_graphics_context_provider_func_;

  // Lock used to serialize access for the following member variables.
  mutable base::Lock lock_;

  // Amount of available buffered data.  Set by filters.
  ::media::Ranges<TimeDelta> buffered_time_ranges_;

  // True when AddBufferedByteRange() has been called more recently than
  // DidLoadingProgress().
  mutable bool did_loading_progress_;

  // Video's natural width and height.  Set by filters.
  gfx::Size natural_size_;

  // Current volume level (from 0.0f to 1.0f).  This value is set immediately
  // via SetVolume() and a task is dispatched on the task runner to notify the
  // filters.
  base::CVal<float> volume_;

  // Current playback rate (>= 0.0f).  This value is set immediately via
  // SetPlaybackRate() and a task is dispatched on the task runner to notify
  // the filters.
  base::CVal<float> playback_rate_;

  // The saved audio and video demuxer streams.  Note that it is safe to store
  // raw pointers of the demuxer streams, as the Demuxer guarantees that its
  // |DemuxerStream|s live as long as the Demuxer itself.
  ::media::DemuxerStream* audio_stream_ = nullptr;
  ::media::DemuxerStream* video_stream_ = nullptr;

  mutable PipelineStatistics statistics_;

  // The following member variables are only accessed by tasks posted to
  // |task_runner_|.

  // Temporary callback used for Stop().
  base::Closure stop_cb_;

  // Permanent callbacks passed in via Start().
  SetDrmSystemReadyCB set_drm_system_ready_cb_;
  PipelineStatusCB ended_cb_;
  ErrorCB error_cb_;
  BufferingStateCB buffering_state_cb_;
  base::Closure duration_change_cb_;
  base::Closure output_mode_change_cb_;
  base::Closure content_size_change_cb_;
  base::Optional<bool> decode_to_texture_output_mode_;
#if SB_HAS(PLAYER_WITH_URL)
  SbPlayerBridge::OnEncryptedMediaInitDataEncounteredCB
      on_encrypted_media_init_data_encountered_cb_;
#endif  //  SB_HAS(PLAYER_WITH_URL)

  // Demuxer reference used for setting the preload value.
  Demuxer* demuxer_ = nullptr;
  bool audio_read_in_progress_ = false;
  bool audio_read_delayed_ = false;
  bool video_read_in_progress_ = false;
  base::CVal<TimeDelta> duration_;

#if SB_HAS(PLAYER_WITH_URL)
  TimeDelta start_date_;
  bool is_url_based_;
#endif  // SB_HAS(PLAYER_WITH_URL)

  scoped_refptr<SbPlayerSetBoundsHelper> set_bounds_helper_;

  // The following member variables can be accessed from WMPI thread but all
  // modifications to them happens on the pipeline thread.  So any access of
  // them from the WMPI thread and any modification to them on the pipeline
  // thread has to guarded by lock.  Access to them from the pipeline thread
  // needn't to be guarded.

  // Temporary callback used for Start() and Seek().
  SeekCB seek_cb_;
  TimeDelta seek_time_;
  std::unique_ptr<SbPlayerBridge> player_bridge_;
  bool is_initial_preroll_ = true;
  base::CVal<bool> started_;
  base::CVal<bool> suspended_;
  base::CVal<bool> stopped_;
  base::CVal<bool> ended_;
  base::CVal<SbPlayerState> player_state_;

  MediaMetricsProvider* media_metrics_provider_;

  DecodeTargetProvider* decode_target_provider_;

#if SB_API_VERSION >= 15
  const TimeDelta audio_write_duration_local_;
  const TimeDelta audio_write_duration_remote_;

  // The two variables below should always contain the same value.  They are
  // kept as separate variables so we can keep the existing implementation as
  // is, which simplifies the implementation across multiple Starboard versions.
  TimeDelta audio_write_duration_;
  TimeDelta audio_write_duration_for_preroll_ = audio_write_duration_;
#else   // SB_API_VERSION >= 15
  // Read audio from the stream if |timestamp_of_last_written_audio_| is less
  // than |seek_time_| + |audio_write_duration_for_preroll_|, this effectively
  // allows 10 seconds of audio to be written to the SbPlayer after playback
  // startup or seek.
  TimeDelta audio_write_duration_for_preroll_ = TimeDelta::FromSeconds(10);
  // Don't read audio from the stream more than |audio_write_duration_| ahead of
  // the current media time during playing.
  TimeDelta audio_write_duration_ = TimeDelta::FromSeconds(1);
#endif  // SB_API_VERSION >= 15
  // Only call GetMediaTime() from OnNeedData if it has been
  // |kMediaTimeCheckInterval| since the last call to GetMediaTime().
  static constexpr TimeDelta kMediaTimeCheckInterval =
      TimeDelta::FromMicroseconds(100);
  // Timestamp for the last written audio.
  TimeDelta timestamp_of_last_written_audio_;
  // Indicates if video end of stream has been written into the underlying
  // player.
  bool is_video_eos_written_ = false;

  // Last media time reported by GetMediaTime().
  base::CVal<TimeDelta> last_media_time_;
  // Timestamp microseconds when we last checked the media time.
  Time last_time_media_time_retrieved_;
  // Counter for retrograde media time.
  size_t retrograde_media_time_counter_ = 0;
  // The maximum video playback capabilities required for the playback.
  base::CVal<std::string> max_video_capabilities_;
  // Set the maximum size in bytes of an input buffer for video.
  int max_video_input_size_;

  PlaybackStatistics playback_statistics_;

  Time last_resume_time_;

  Time set_drm_system_ready_cb_time_;

  // Message to signal a capability changed error.
  // "MEDIA_ERR_CAPABILITY_CHANGED" must be in the error message to be
  // understood as a capability changed error. Do not change this message.
  static inline constexpr const char* kSbPlayerCapabilityChangedErrorMessage =
      "MEDIA_ERR_CAPABILITY_CHANGED";

  DISALLOW_COPY_AND_ASSIGN(SbPlayerPipeline);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_SBPLAYER_PIPELINE_H_
