// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <algorithm>
#include <memory>
#include <vector>

#include "base/basictypes.h"  // For COMPILE_ASSERT
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/media/base/audio_decoder_config.h"
#include "cobalt/media/base/bind_to_current_loop.h"
#include "cobalt/media/base/channel_layout.h"
#include "cobalt/media/base/decoder_buffer.h"
#include "cobalt/media/base/demuxer.h"
#include "cobalt/media/base/demuxer_stream.h"
#include "cobalt/media/base/media_export.h"
#include "cobalt/media/base/media_log.h"
#include "cobalt/media/base/pipeline.h"
#include "cobalt/media/base/pipeline_status.h"
#include "cobalt/media/base/ranges.h"
#include "cobalt/media/base/sbplayer_set_bounds_helper.h"
#include "cobalt/media/base/starboard_player.h"
#include "cobalt/media/base/video_decoder_config.h"
#include "ui/gfx/size.h"

namespace cobalt {
namespace media {

using base::Time;
using base::TimeDelta;

namespace {

static const int kRetryDelayAtSuspendInMilliseconds = 100;

// Used to post parameters to SbPlayerPipeline::StartTask() as the number of
// parameters exceed what base::Bind() can support.
struct StartTaskParameters {
  Demuxer* demuxer;
  SetDrmSystemReadyCB set_drm_system_ready_cb;
  PipelineStatusCB ended_cb;
  ErrorCB error_cb;
  Pipeline::SeekCB seek_cb;
  Pipeline::BufferingStateCB buffering_state_cb;
  base::Closure duration_change_cb;
  base::Closure output_mode_change_cb;
  base::Closure content_size_change_cb;
  std::string max_video_capabilities;
#if SB_HAS(PLAYER_WITH_URL)
  std::string source_url;
  bool is_url_based;
#endif  // SB_HAS(PLAYER_WITH_URL)
};

// SbPlayerPipeline is a PipelineBase implementation that uses the SbPlayer
// interface internally.
class MEDIA_EXPORT SbPlayerPipeline : public Pipeline,
                                      public DemuxerHost,
                                      public StarboardPlayer::Host {
 public:
  // Constructs a media pipeline that will execute on |task_runner|.
  SbPlayerPipeline(
      PipelineWindow window,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      bool allow_resume_after_suspend, MediaLog* media_log,
      VideoFrameProvider* video_frame_provider);
  ~SbPlayerPipeline() override;

  void Suspend() override;
  void Resume() override;
  void Start(Demuxer* demuxer,
             const SetDrmSystemReadyCB& set_drm_system_ready_cb,
             const PipelineStatusCB& ended_cb, const ErrorCB& error_cb,
             const SeekCB& seek_cb, const BufferingStateCB& buffering_state_cb,
             const base::Closure& duration_change_cb,
             const base::Closure& output_mode_change_cb,
             const base::Closure& content_size_change_cb,
             const std::string& max_video_capabilities) override;
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
  Ranges<TimeDelta> GetBufferedTimeRanges() override;
  TimeDelta GetMediaDuration() const override;
#if SB_HAS(PLAYER_WITH_URL)
  TimeDelta GetMediaStartDate() const override;
#endif  // SB_HAS(PLAYER_WITH_URL)
  void GetNaturalVideoSize(gfx::Size* out_size) const override;

  bool DidLoadingProgress() const override;
  PipelineStatistics GetStatistics() const override;
  SetBoundsCB GetSetBoundsCB() override;
  void SetDecodeToTextureOutputMode(bool enabled) override;

 private:
  void StartTask(const StartTaskParameters& parameters);
  void SetVolumeTask(float volume);
  void SetPlaybackRateTask(float volume);
  void SetDurationTask(TimeDelta duration);

  // DemuxerHost implementaion.
  void OnBufferedTimeRangesChanged(
      const Ranges<base::TimeDelta>& ranges) override;
  void SetDuration(TimeDelta duration) override;
  void OnDemuxerError(PipelineStatus error) override;
  void AddTextStream(DemuxerStream* text_stream,
                     const TextTrackConfig& config) override;
  void RemoveTextStream(DemuxerStream* text_stream) override;

#if SB_HAS(PLAYER_WITH_URL)
  void CreateUrlPlayer(const std::string& source_url);
  void SetDrmSystem(SbDrmSystem drm_system);
#endif  // SB_HAS(PLAYER_WITH_URL)
  void CreatePlayer(SbDrmSystem drm_system);

  void OnDemuxerInitialized(PipelineStatus status);
  void OnDemuxerSeeked(PipelineStatus status);
  void OnDemuxerStopped();
  void OnDemuxerStreamRead(DemuxerStream::Type type,
                           DemuxerStream::Status status,
                           const scoped_refptr<DecoderBuffer>& buffer);
  // StarboardPlayer::Host implementation.
  void OnNeedData(DemuxerStream::Type type) override;
  void OnPlayerStatus(SbPlayerState state) override;
#if SB_HAS(PLAYER_ERROR_MESSAGE)
  void OnPlayerError(SbPlayerError error, const std::string& message) override;
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)

  // Used to make a delayed call to OnNeedData() if |audio_read_delayed_| is
  // true. If |audio_read_delayed_| is false, that means the delayed call has
  // been cancelled due to a seek.
  void DelayedNeedData();

  void UpdateDecoderConfig(DemuxerStream* stream);
  void CallSeekCB(PipelineStatus status);

  void SuspendTask(base::WaitableEvent* done_event);
  void ResumeTask(base::WaitableEvent* done_event);

  // Store the media time retrieved by GetMediaTime so we can cache it as an
  // estimate and avoid calling SbPlayerGetInfo too frequently.
  void StoreMediaTime(TimeDelta media_time);

  // Message loop used to execute pipeline tasks.  It is thread-safe.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Whether we should save DecoderBuffers for resume after suspend.
  const bool allow_resume_after_suspend_;

  // The window this player associates with.  It should only be assigned in the
  // dtor and accesed once by SbPlayerCreate().
  PipelineWindow window_;

  // Lock used to serialize access for the following member variables.
  mutable base::Lock lock_;

  // Amount of available buffered data.  Set by filters.
  Ranges<TimeDelta> buffered_time_ranges_;

  // True when AddBufferedByteRange() has been called more recently than
  // DidLoadingProgress().
  mutable bool did_loading_progress_;

  // Video's natural width and height.  Set by filters.
  gfx::Size natural_size_;

  // Current volume level (from 0.0f to 1.0f).  This value is set immediately
  // via SetVolume() and a task is dispatched on the message loop to notify the
  // filters.
  float volume_ = 1.f;

  // Current playback rate (>= 0.0f).  This value is set immediately via
  // SetPlaybackRate() and a task is dispatched on the message loop to notify
  // the filters.
  float playback_rate_ = 0.f;

  // The saved audio and video demuxer streams.  Note that it is safe to store
  // raw pointers of the demuxer streams, as the Demuxer guarantees that its
  // |DemuxerStream|s live as long as the Demuxer itself.
  DemuxerStream* audio_stream_ = nullptr;
  DemuxerStream* video_stream_ = nullptr;

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
  StarboardPlayer::OnEncryptedMediaInitDataEncounteredCB
      on_encrypted_media_init_data_encountered_cb_;
#endif  //  SB_HAS(PLAYER_WITH_URL)

  // Demuxer reference used for setting the preload value.
  Demuxer* demuxer_ = nullptr;
  bool audio_read_in_progress_ = false;
  bool audio_read_delayed_ = false;
  bool video_read_in_progress_ = false;
  TimeDelta duration_;

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
  base::TimeDelta seek_time_;
  std::unique_ptr<StarboardPlayer> player_;
  bool is_initial_preroll_ = true;
  bool suspended_ = false;
  bool stopped_ = false;
  bool ended_ = false;

  VideoFrameProvider* video_frame_provider_;

  // Don't read audio from the stream more than |kAudioLimit| ahead of the
  // current media time.
  static const SbTime kAudioLimit = kSbTimeSecond;
  // Only call GetMediaTime() from OnNeedData if it has been
  // |kMediaTimeCheckInterval| since the last call to GetMediaTime().
  static const SbTime kMediaTimeCheckInterval = 0.1 * kSbTimeSecond;
  // Timestamp for the last written audio.
  SbTime timestamp_of_last_written_audio_ = 0;
  // Last media time reported by GetMediaTime().
  SbTime last_media_time_;
  // Time when we last checked the media time.
  SbTime last_time_media_time_retrieved_ = 0;
  // The maximum video playback capabilities required for the playback.
  std::string max_video_capabilities_;

  DISALLOW_COPY_AND_ASSIGN(SbPlayerPipeline);
};

SbPlayerPipeline::SbPlayerPipeline(
    PipelineWindow window,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    bool allow_resume_after_suspend, MediaLog* media_log,
    VideoFrameProvider* video_frame_provider)
    : window_(window),
      task_runner_(task_runner),
      allow_resume_after_suspend_(allow_resume_after_suspend),
      natural_size_(0, 0),
      set_bounds_helper_(new SbPlayerSetBoundsHelper),
      video_frame_provider_(video_frame_provider) {
#if SB_API_VERSION >= SB_SET_AUDIO_WRITE_DURATION_VERSION
  SbMediaSetAudioWriteDuration(kAudioLimit);
#endif  // SB_API_VERSION >= SB_SET_AUDIO_WRITE_DURATION_VERSION
}

SbPlayerPipeline::~SbPlayerPipeline() { DCHECK(!player_); }

void SbPlayerPipeline::Suspend() {
  DCHECK(!task_runner_->BelongsToCurrentThread());

  base::WaitableEvent waitable_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&SbPlayerPipeline::SuspendTask,
                                    base::Unretained(this), &waitable_event));
  waitable_event.Wait();
}

void SbPlayerPipeline::Resume() {
  DCHECK(!task_runner_->BelongsToCurrentThread());

  base::WaitableEvent waitable_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&SbPlayerPipeline::ResumeTask,
                                    base::Unretained(this), &waitable_event));
  waitable_event.Wait();
}

#if SB_HAS(PLAYER_WITH_URL)
void OnEncryptedMediaInitDataEncountered(
    const Pipeline::OnEncryptedMediaInitDataEncounteredCB&
        on_encrypted_media_init_data_encountered,
    const char* init_data_type, const unsigned char* init_data,
    unsigned int init_data_length) {
  media::EmeInitDataType init_data_type_enum;
  if (!SbStringCompareAll(init_data_type, "cenc")) {
    init_data_type_enum = media::kEmeInitDataTypeCenc;
  } else if (!SbStringCompareAll(init_data_type, "fairplay")) {
    init_data_type_enum = media::kEmeInitDataTypeFairplay;
  } else if (!SbStringCompareAll(init_data_type, "keyids")) {
    init_data_type_enum = media::kEmeInitDataTypeKeyIds;
  } else if (!SbStringCompareAll(init_data_type, "webm")) {
    init_data_type_enum = media::kEmeInitDataTypeWebM;
  } else {
    LOG(WARNING) << "Unknown EME initialization data type.";
    return;
  }
  std::vector<uint8_t> init_data_vec(init_data, init_data + init_data_length);
  DCHECK(!on_encrypted_media_init_data_encountered.is_null());
  on_encrypted_media_init_data_encountered.Run(init_data_type_enum,
                                               init_data_vec);
}
#endif  // SB_HAS(PLAYER_WITH_URL)

void SbPlayerPipeline::Start(Demuxer* demuxer,
                             const SetDrmSystemReadyCB& set_drm_system_ready_cb,
                             const PipelineStatusCB& ended_cb,
                             const ErrorCB& error_cb, const SeekCB& seek_cb,
                             const BufferingStateCB& buffering_state_cb,
                             const base::Closure& duration_change_cb,
                             const base::Closure& output_mode_change_cb,
                             const base::Closure& content_size_change_cb,
                             const std::string& max_video_capabilities) {
  TRACE_EVENT0("cobalt::media", "SbPlayerPipeline::Start");

  DCHECK(!ended_cb.is_null());
  DCHECK(!error_cb.is_null());
  DCHECK(!seek_cb.is_null());
  DCHECK(!buffering_state_cb.is_null());
  DCHECK(!duration_change_cb.is_null());
  DCHECK(!output_mode_change_cb.is_null());
  DCHECK(!content_size_change_cb.is_null());
  DCHECK(demuxer);

  StartTaskParameters parameters;
  parameters.demuxer = demuxer;
  parameters.set_drm_system_ready_cb = set_drm_system_ready_cb;
  parameters.ended_cb = ended_cb;
  parameters.error_cb = error_cb;
  parameters.seek_cb = seek_cb;
  parameters.buffering_state_cb = buffering_state_cb;
  parameters.duration_change_cb = duration_change_cb;
  parameters.output_mode_change_cb = output_mode_change_cb;
  parameters.content_size_change_cb = content_size_change_cb;
  parameters.max_video_capabilities = max_video_capabilities;
#if SB_HAS(PLAYER_WITH_URL)
  parameters.is_url_based = false;
#endif  // SB_HAS(PLAYER_WITH_URL)

  task_runner_->PostTask(
      FROM_HERE, base::Bind(&SbPlayerPipeline::StartTask, this, parameters));
}

#if SB_HAS(PLAYER_WITH_URL)
void SbPlayerPipeline::Start(const SetDrmSystemReadyCB& set_drm_system_ready_cb,
                             const OnEncryptedMediaInitDataEncounteredCB&
                                 on_encrypted_media_init_data_encountered_cb,
                             const std::string& source_url,
                             const PipelineStatusCB& ended_cb,
                             const ErrorCB& error_cb,
                             const SeekCB& seek_cb,
                             const BufferingStateCB& buffering_state_cb,
                             const base::Closure& duration_change_cb,
                             const base::Closure& output_mode_change_cb,
                             const base::Closure& content_size_change_cb) {
  TRACE_EVENT0("cobalt::media", "SbPlayerPipeline::Start");

  DCHECK(!ended_cb.is_null());
  DCHECK(!error_cb.is_null());
  DCHECK(!seek_cb.is_null());
  DCHECK(!buffering_state_cb.is_null());
  DCHECK(!duration_change_cb.is_null());
  DCHECK(!output_mode_change_cb.is_null());
  DCHECK(!content_size_change_cb.is_null());
  DCHECK(!on_encrypted_media_init_data_encountered_cb.is_null());

  StartTaskParameters parameters;
  parameters.demuxer = NULL;
  parameters.set_drm_system_ready_cb = set_drm_system_ready_cb;
  parameters.ended_cb = ended_cb;
  parameters.error_cb = error_cb;
  parameters.seek_cb = seek_cb;
  parameters.buffering_state_cb = buffering_state_cb;
  parameters.duration_change_cb = duration_change_cb;
  parameters.output_mode_change_cb = output_mode_change_cb;
  parameters.content_size_change_cb = content_size_change_cb;
  parameters.source_url = source_url;
  parameters.is_url_based = true;
  on_encrypted_media_init_data_encountered_cb_ =
      base::Bind(&OnEncryptedMediaInitDataEncountered,
                 on_encrypted_media_init_data_encountered_cb);
  set_drm_system_ready_cb_ = parameters.set_drm_system_ready_cb;
  DCHECK(!set_drm_system_ready_cb_.is_null());
  set_drm_system_ready_cb_.Run(
      base::Bind(&SbPlayerPipeline::SetDrmSystem, this));

  task_runner_->PostTask(
      FROM_HERE, base::Bind(&SbPlayerPipeline::StartTask, this, parameters));
}
#endif  // SB_HAS(PLAYER_WITH_URL)

void SbPlayerPipeline::Stop(const base::Closure& stop_cb) {
  TRACE_EVENT0("cobalt::media", "SbPlayerPipeline::Stop");

  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&SbPlayerPipeline::Stop, this, stop_cb));
    return;
  }

  DCHECK(stop_cb_.is_null());
  DCHECK(!stop_cb.is_null());

  stopped_ = true;

  if (player_) {
    std::unique_ptr<StarboardPlayer> player;
    {
      base::AutoLock auto_lock(lock_);
      player = std::move(player_);
    }

    DLOG(INFO) << "Destroying SbPlayer.";
    player.reset();
    DLOG(INFO) << "SbPlayer destroyed.";
  }

  // When Stop() is in progress, we no longer need to call |error_cb_|.
  error_cb_.Reset();
  if (demuxer_) {
    stop_cb_ = stop_cb;
    demuxer_->Stop();
    OnDemuxerStopped();
  } else {
    stop_cb.Run();
  }
}

void SbPlayerPipeline::Seek(TimeDelta time, const SeekCB& seek_cb) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&SbPlayerPipeline::Seek, this, time, seek_cb));
    return;
  }

  if (!player_) {
    seek_cb.Run(PIPELINE_ERROR_INVALID_STATE, false);
    return;
  }

  player_->PrepareForSeek();
  ended_ = false;

  DCHECK(seek_cb_.is_null());
  DCHECK(!seek_cb.is_null());

  if (audio_read_in_progress_ || video_read_in_progress_) {
    const TimeDelta kDelay = TimeDelta::FromMilliseconds(50);
    task_runner_->PostDelayedTask(
        FROM_HERE, base::Bind(&SbPlayerPipeline::Seek, this, time, seek_cb),
        kDelay);
    return;
  }

  {
    base::AutoLock auto_lock(lock_);
    seek_cb_ = seek_cb;
    seek_time_ = time;
  }

  // Ignore pending delayed calls to OnNeedData, and update variables used to
  // decide when to delay.
  audio_read_delayed_ = false;
  StoreMediaTime(seek_time_);
  timestamp_of_last_written_audio_ = 0;

#if SB_HAS(PLAYER_WITH_URL)
  if (is_url_based_) {
    player_->Seek(seek_time_);
    return;
  }
#endif  // SB_HAS(PLAYER_WITH_URL)
  demuxer_->Seek(time, BindToCurrentLoop(base::Bind(
                           &SbPlayerPipeline::OnDemuxerSeeked, this)));
}

bool SbPlayerPipeline::HasAudio() const {
  base::AutoLock auto_lock(lock_);
  return audio_stream_ != NULL;
}

bool SbPlayerPipeline::HasVideo() const {
  base::AutoLock auto_lock(lock_);
  return video_stream_ != NULL;
}

float SbPlayerPipeline::GetPlaybackRate() const {
  base::AutoLock auto_lock(lock_);
  return playback_rate_;
}

void SbPlayerPipeline::SetPlaybackRate(float playback_rate) {
  base::AutoLock auto_lock(lock_);
  playback_rate_ = playback_rate;
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SbPlayerPipeline::SetPlaybackRateTask, this, playback_rate));
}

float SbPlayerPipeline::GetVolume() const {
  base::AutoLock auto_lock(lock_);
  return volume_;
}

void SbPlayerPipeline::SetVolume(float volume) {
  if (volume < 0.0f || volume > 1.0f) return;

  base::AutoLock auto_lock(lock_);
  volume_ = volume;
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&SbPlayerPipeline::SetVolumeTask, this, volume));
}

void SbPlayerPipeline::StoreMediaTime(TimeDelta media_time) {
  last_media_time_ = media_time.ToSbTime();
  last_time_media_time_retrieved_ = SbTimeGetNow();
}

TimeDelta SbPlayerPipeline::GetMediaTime() {
  base::AutoLock auto_lock(lock_);

  if (!seek_cb_.is_null()) {
    StoreMediaTime(seek_time_);
    return seek_time_;
  }
  if (!player_) {
    StoreMediaTime(TimeDelta());
    return TimeDelta();
  }
  if (ended_) {
    StoreMediaTime(duration_);
    return duration_;
  }
  base::TimeDelta media_time;
#if SB_HAS(PLAYER_WITH_URL)
  if (is_url_based_) {
    int frame_width;
    int frame_height;
    player_->GetVideoResolution(&frame_width, &frame_height);
    if (frame_width != natural_size_.width() ||
        frame_height != natural_size_.height()) {
      natural_size_ = gfx::Size(frame_width, frame_height);
      content_size_change_cb_.Run();
    }
  }
#endif  // SB_HAS(PLAYER_WITH_URL)
  player_->GetInfo(&statistics_.video_frames_decoded,
                   &statistics_.video_frames_dropped, &media_time);
  StoreMediaTime(media_time);
  return media_time;
}

Ranges<TimeDelta> SbPlayerPipeline::GetBufferedTimeRanges() {
  base::AutoLock auto_lock(lock_);

#if SB_HAS(PLAYER_WITH_URL)
  Ranges<TimeDelta> time_ranges;

  if (!player_) {
    return time_ranges;
  }

  if (is_url_based_) {
    base::TimeDelta media_time;
    base::TimeDelta buffer_start_time;
    base::TimeDelta buffer_length_time;
    player_->GetInfo(&statistics_.video_frames_decoded,
                     &statistics_.video_frames_dropped, &media_time);
    player_->GetUrlPlayerBufferedTimeRanges(&buffer_start_time,
                                            &buffer_length_time);

    if (buffer_length_time.InSeconds() == 0) {
      buffered_time_ranges_ = time_ranges;
      return time_ranges;
    }

    time_ranges.Add(buffer_start_time, buffer_start_time + buffer_length_time);

    if (buffered_time_ranges_.size() > 0) {
      base::TimeDelta old_buffer_start_time = buffered_time_ranges_.start(0);
      base::TimeDelta old_buffer_length_time = buffered_time_ranges_.end(0);
      int64 old_start_seconds = old_buffer_start_time.InSeconds();
      int64 new_start_seconds = buffer_start_time.InSeconds();
      int64 old_length_seconds = old_buffer_length_time.InSeconds();
      int64 new_length_seconds = buffer_length_time.InSeconds();
      if (old_start_seconds != new_start_seconds ||
          old_length_seconds != new_length_seconds) {
        did_loading_progress_ = true;
      }
    } else {
      did_loading_progress_ = true;
    }

    buffered_time_ranges_ = time_ranges;
    return time_ranges;
  }
#endif  // SB_HAS(PLAYER_WITH_URL)

  return buffered_time_ranges_;
}

TimeDelta SbPlayerPipeline::GetMediaDuration() const {
  base::AutoLock auto_lock(lock_);
  return duration_;
}

#if SB_HAS(PLAYER_WITH_URL)
TimeDelta SbPlayerPipeline::GetMediaStartDate() const {
  base::AutoLock auto_lock(lock_);
  return start_date_;
}
#endif  // SB_HAS(PLAYER_WITH_URL)

void SbPlayerPipeline::GetNaturalVideoSize(gfx::Size* out_size) const {
  CHECK(out_size);
  base::AutoLock auto_lock(lock_);
  *out_size = natural_size_;
}

bool SbPlayerPipeline::DidLoadingProgress() const {
  base::AutoLock auto_lock(lock_);
  bool ret = did_loading_progress_;
  did_loading_progress_ = false;
  return ret;
}

PipelineStatistics SbPlayerPipeline::GetStatistics() const {
  base::AutoLock auto_lock(lock_);
  return statistics_;
}

Pipeline::SetBoundsCB SbPlayerPipeline::GetSetBoundsCB() {
  return base::Bind(&SbPlayerSetBoundsHelper::SetBounds, set_bounds_helper_);
}

void SbPlayerPipeline::SetDecodeToTextureOutputMode(bool enabled) {
  TRACE_EVENT1("cobalt::media",
               "SbPlayerPipeline::SetDecodeToTextureOutputMode", "mode",
               enabled);

  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&SbPlayerPipeline::SetDecodeToTextureOutputMode,
                              this, enabled));
    return;
  }

  // The player can't be created yet, if it is, then we're updating the output
  // mode too late.
  DCHECK(!player_);

  decode_to_texture_output_mode_ = enabled;
}

void SbPlayerPipeline::StartTask(const StartTaskParameters& parameters) {
  TRACE_EVENT0("cobalt::media", "SbPlayerPipeline::StartTask");

  DCHECK(task_runner_->BelongsToCurrentThread());

  DCHECK(!demuxer_);

  demuxer_ = parameters.demuxer;
  set_drm_system_ready_cb_ = parameters.set_drm_system_ready_cb;
  ended_cb_ = parameters.ended_cb;
  error_cb_ = parameters.error_cb;
  {
    base::AutoLock auto_lock(lock_);
    seek_cb_ = parameters.seek_cb;
  }
  buffering_state_cb_ = parameters.buffering_state_cb;
  duration_change_cb_ = parameters.duration_change_cb;
  output_mode_change_cb_ = parameters.output_mode_change_cb;
  content_size_change_cb_ = parameters.content_size_change_cb;
  max_video_capabilities_ = parameters.max_video_capabilities;
#if SB_HAS(PLAYER_WITH_URL)
  is_url_based_ = parameters.is_url_based;
  if (is_url_based_) {
    CreateUrlPlayer(parameters.source_url);
    return;
  }
#endif  // SB_HAS(PLAYER_WITH_URL)
  const bool kEnableTextTracks = false;
  demuxer_->Initialize(this,
                       BindToCurrentLoop(base::Bind(
                           &SbPlayerPipeline::OnDemuxerInitialized, this)),
                       kEnableTextTracks);
}

void SbPlayerPipeline::SetVolumeTask(float volume) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (player_) {
    player_->SetVolume(volume_);
  }
}

void SbPlayerPipeline::SetPlaybackRateTask(float volume) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (player_) {
    player_->SetPlaybackRate(playback_rate_);
  }
}

void SbPlayerPipeline::SetDurationTask(TimeDelta duration) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!duration_change_cb_.is_null()) {
    duration_change_cb_.Run();
  }
}

void SbPlayerPipeline::OnBufferedTimeRangesChanged(
    const Ranges<base::TimeDelta>& ranges) {
  base::AutoLock auto_lock(lock_);
  did_loading_progress_ = true;
  buffered_time_ranges_ = ranges;
}

void SbPlayerPipeline::SetDuration(TimeDelta duration) {
  base::AutoLock auto_lock(lock_);
  duration_ = duration;
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SbPlayerPipeline::SetDurationTask, this, duration));
}

void SbPlayerPipeline::OnDemuxerError(PipelineStatus error) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&SbPlayerPipeline::OnDemuxerError, this, error));
    return;
  }

  if (error != PIPELINE_OK) {
    ResetAndRunIfNotNull(&error_cb_, error, "Demuxer error.");
  }
}

void SbPlayerPipeline::AddTextStream(DemuxerStream* text_stream,
                                     const TextTrackConfig& config) {
  NOTREACHED();
}

void SbPlayerPipeline::RemoveTextStream(DemuxerStream* text_stream) {
  NOTREACHED();
}

#if SB_HAS(PLAYER_WITH_URL)
void SbPlayerPipeline::CreateUrlPlayer(const std::string& source_url) {
  TRACE_EVENT0("cobalt::media", "SbPlayerPipeline::CreateUrlPlayer");
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (stopped_) {
    return;
  }

  if (suspended_) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&SbPlayerPipeline::CreateUrlPlayer, this, source_url),
        TimeDelta::FromMilliseconds(kRetryDelayAtSuspendInMilliseconds));
    return;
  }

  // TODO:  Check |suspended_| here as the pipeline can be suspended before the
  // player is created.  In this case we should delay creating the player as the
  // creation of player may fail.

  {
    base::AutoLock auto_lock(lock_);
    DLOG(INFO) << "StarboardPlayer created with url: " << source_url;
    player_.reset(new StarboardPlayer(
        task_runner_, source_url, window_, this, set_bounds_helper_.get(),
        allow_resume_after_suspend_, *decode_to_texture_output_mode_,
        on_encrypted_media_init_data_encountered_cb_, video_frame_provider_));
    SetPlaybackRateTask(playback_rate_);
    SetVolumeTask(volume_);
  }

  if (player_->IsValid()) {
    base::Closure output_mode_change_cb;
    {
      base::AutoLock auto_lock(lock_);
      DCHECK(!output_mode_change_cb_.is_null());
      output_mode_change_cb = std::move(output_mode_change_cb_);
    }
    output_mode_change_cb.Run();
    return;
  }

  player_.reset();
  CallSeekCB(DECODER_ERROR_NOT_SUPPORTED);
}

void SbPlayerPipeline::SetDrmSystem(SbDrmSystem drm_system) {
  TRACE_EVENT0("cobalt::media", "SbPlayerPipeline::SetDrmSystem");

  base::AutoLock auto_lock(lock_);
  if (!player_) {
    DLOG(INFO)
        << "Player not set before calling SbPlayerPipeline::SetDrmSystem";
    return;
  }

  if (player_->IsValid()) {
    player_->SetDrmSystem(drm_system);
  }
}
#endif  // SB_HAS(PLAYER_WITH_URL)

void SbPlayerPipeline::CreatePlayer(SbDrmSystem drm_system) {
#if SB_HAS(PLAYER_WITH_URL)
  DCHECK(!is_url_based_);
#endif  // SB_HAS(PLAYER_WITH_URL
  TRACE_EVENT0("cobalt::media", "SbPlayerPipeline::CreatePlayer");

  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(audio_stream_ || video_stream_);

  if (stopped_) {
    return;
  }

  if (suspended_) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&SbPlayerPipeline::CreatePlayer, this, drm_system),
        TimeDelta::FromMilliseconds(kRetryDelayAtSuspendInMilliseconds));
    return;
  }

  // TODO:  Check |suspended_| here as the pipeline can be suspended before the
  // player is created.  In this case we should delay creating the player as the
  // creation of player may fail.
  AudioDecoderConfig invalid_audio_config;
  const AudioDecoderConfig& audio_config =
      audio_stream_ ? audio_stream_->audio_decoder_config()
                    : invalid_audio_config;
  VideoDecoderConfig invalid_video_config;
  const VideoDecoderConfig& video_config =
      video_stream_ ? video_stream_->video_decoder_config()
                    : invalid_video_config;

  {
    base::AutoLock auto_lock(lock_);
    player_.reset(new StarboardPlayer(
        task_runner_, audio_config, video_config, window_, drm_system, this,
        set_bounds_helper_.get(), allow_resume_after_suspend_,
        *decode_to_texture_output_mode_, video_frame_provider_,
        max_video_capabilities_));
    SetPlaybackRateTask(playback_rate_);
    SetVolumeTask(volume_);
  }

  if (player_->IsValid()) {
    base::Closure output_mode_change_cb;
    {
      base::AutoLock auto_lock(lock_);
      DCHECK(!output_mode_change_cb_.is_null());
      output_mode_change_cb = std::move(output_mode_change_cb_);
    }
    output_mode_change_cb.Run();

    if (audio_stream_) {
      UpdateDecoderConfig(audio_stream_);
    }
    if (video_stream_) {
      UpdateDecoderConfig(video_stream_);
    }
    return;
  }

  player_.reset();
  CallSeekCB(DECODER_ERROR_NOT_SUPPORTED);
}

void SbPlayerPipeline::OnDemuxerInitialized(PipelineStatus status) {
#if SB_HAS(PLAYER_WITH_URL)
  DCHECK(!is_url_based_);
#endif  // SB_HAS(PLAYER_WITH_URL)
  TRACE_EVENT0("cobalt::media", "SbPlayerPipeline::OnDemuxerInitialized");

  DCHECK(task_runner_->BelongsToCurrentThread());

  if (stopped_) {
    return;
  }

  if (status != PIPELINE_OK) {
    ResetAndRunIfNotNull(&error_cb_, status, "Demuxer initialization error.");
    return;
  }

  if (suspended_) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&SbPlayerPipeline::OnDemuxerInitialized, this, status),
        TimeDelta::FromMilliseconds(kRetryDelayAtSuspendInMilliseconds));
    return;
  }

  DemuxerStream* audio_stream = demuxer_->GetStream(DemuxerStream::AUDIO);
  DemuxerStream* video_stream = demuxer_->GetStream(DemuxerStream::VIDEO);

#if !SB_HAS(AUDIOLESS_VIDEO)
  if (audio_stream == NULL) {
    LOG(INFO) << "The video has to contain an audio track.";
    ResetAndRunIfNotNull(&error_cb_, DEMUXER_ERROR_NO_SUPPORTED_STREAMS,
                         "The video has to contain an audio track.");
    return;
  }
#endif  // !SB_HAS(AUDIOLESS_VIDEO)

#if SB_API_VERSION < 10
  if (video_stream == NULL) {
    LOG(INFO) << "The video has to contain a video track.";
    ResetAndRunIfNotNull(&error_cb_, DEMUXER_ERROR_NO_SUPPORTED_STREAMS,
                         "The video has to contain a video track.");
    return;
  }
#endif  // SB_API_VERSION < 10

  if (audio_stream == NULL && video_stream == NULL) {
    LOG(INFO) << "The video has to contain an audio track or a video track.";
    ResetAndRunIfNotNull(
        &error_cb_, DEMUXER_ERROR_NO_SUPPORTED_STREAMS,
        "The video has to contain an audio track or a video track.");
    return;
  }

  {
    base::AutoLock auto_lock(lock_);
    audio_stream_ = audio_stream;
    video_stream_ = video_stream;

    bool is_encrypted =
        audio_stream_ && audio_stream_->audio_decoder_config().is_encrypted();
    is_encrypted |=
        video_stream_ && video_stream_->video_decoder_config().is_encrypted();
    bool natural_size_changed = false;
    if (video_stream_) {
      natural_size_changed =
          (video_stream_->video_decoder_config().natural_size().width() !=
               natural_size_.width() ||
           video_stream_->video_decoder_config().natural_size().height() !=
               natural_size_.height());
      natural_size_ = video_stream_->video_decoder_config().natural_size();
    }
    if (natural_size_changed) {
      content_size_change_cb_.Run();
    }
    if (is_encrypted) {
      set_drm_system_ready_cb_.Run(
          BindToCurrentLoop(base::Bind(&SbPlayerPipeline::CreatePlayer, this)));
      return;
    }
  }

  CreatePlayer(kSbDrmSystemInvalid);
}

void SbPlayerPipeline::OnDemuxerSeeked(PipelineStatus status) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (status == PIPELINE_OK && player_) {
    player_->Seek(seek_time_);
  }
}

void SbPlayerPipeline::OnDemuxerStopped() {
  TRACE_EVENT0("cobalt::media", "SbPlayerPipeline::OnDemuxerStopped");

  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&SbPlayerPipeline::OnDemuxerStopped, this));
    return;
  }

  std::move(stop_cb_).Run();
}

void SbPlayerPipeline::OnDemuxerStreamRead(
    DemuxerStream::Type type, DemuxerStream::Status status,
    const scoped_refptr<DecoderBuffer>& buffer) {
#if SB_HAS(PLAYER_WITH_URL)
  DCHECK(!is_url_based_);
#endif  // SB_HAS(PLAYER_WITH_URL)
  DCHECK(type == DemuxerStream::AUDIO || type == DemuxerStream::VIDEO)
      << "Unsupported DemuxerStream::Type " << type;

  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&SbPlayerPipeline::OnDemuxerStreamRead, this,
                              type, status, buffer));
    return;
  }

  DemuxerStream* stream =
      type == DemuxerStream::AUDIO ? audio_stream_ : video_stream_;
  DCHECK(stream);

  // In case if Stop() has been called.
  if (!player_) {
    return;
  }

  if (status == DemuxerStream::kAborted) {
    if (type == DemuxerStream::AUDIO) {
      DCHECK(audio_read_in_progress_);
      audio_read_in_progress_ = false;
    } else {
      DCHECK(video_read_in_progress_);
      video_read_in_progress_ = false;
    }
    if (!seek_cb_.is_null()) {
      CallSeekCB(PIPELINE_OK);
    }
    return;
  }

  if (status == DemuxerStream::kConfigChanged) {
    UpdateDecoderConfig(stream);
    stream->Read(
        base::Bind(&SbPlayerPipeline::OnDemuxerStreamRead, this, type));
    return;
  }

  if (type == DemuxerStream::AUDIO) {
    audio_read_in_progress_ = false;
    if (!buffer->end_of_stream()) {
      timestamp_of_last_written_audio_ = buffer->timestamp().ToSbTime();
    }
  } else {
    video_read_in_progress_ = false;
  }

  player_->WriteBuffer(type, buffer);
}

void SbPlayerPipeline::OnNeedData(DemuxerStream::Type type) {
#if SB_HAS(PLAYER_WITH_URL)
  DCHECK(!is_url_based_);
#endif  // SB_HAS(PLAYER_WITH_URL)
  DCHECK(task_runner_->BelongsToCurrentThread());

  // In case if Stop() has been called.
  if (!player_) {
    return;
  }

  if (type == DemuxerStream::AUDIO) {
    if (!audio_stream_) {
      LOG(WARNING)
          << "Calling OnNeedData() for audio data during audioless playback";
      return;
    }
    if (audio_read_in_progress_) {
      return;
    }
#if SB_API_VERSION >= SB_SET_AUDIO_WRITE_DURATION_VERSION
    // If we haven't checked the media time recently, update it now.
    if (SbTimeGetNow() - last_time_media_time_retrieved_ >
        kMediaTimeCheckInterval) {
      GetMediaTime();
    }
    // The estimated time ahead of playback may be negative if no audio has been
    // written.
    SbTime time_ahead_of_playback =
        timestamp_of_last_written_audio_ - last_media_time_;
    // Delay reading audio more than |kAudioLimit| ahead of playback, taking
    // into account that our estimate of playback time might be behind by
    // |kMediaTimeCheckInterval|.
    if (time_ahead_of_playback > (kAudioLimit + kMediaTimeCheckInterval)) {
      SbTime delay_time = (time_ahead_of_playback - kAudioLimit) /
                          std::max(playback_rate_, 1.0f);
      task_runner_->PostDelayedTask(
          FROM_HERE, base::Bind(&SbPlayerPipeline::DelayedNeedData, this),
          base::TimeDelta::FromMicroseconds(delay_time));
      audio_read_delayed_ = true;
      return;
    }
    audio_read_delayed_ = false;
#endif  // SB_API_VERSION >= SB_SET_AUDIO_WRITE_DURATION_VERSION
    audio_read_in_progress_ = true;
  } else {
    DCHECK_EQ(type, DemuxerStream::VIDEO);
    if (video_read_in_progress_) {
      return;
    }
    video_read_in_progress_ = true;
  }
  DemuxerStream* stream =
      type == DemuxerStream::AUDIO ? audio_stream_ : video_stream_;
  DCHECK(stream);
  stream->Read(base::Bind(&SbPlayerPipeline::OnDemuxerStreamRead, this, type));
}

void SbPlayerPipeline::OnPlayerStatus(SbPlayerState state) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // In case if Stop() has been called.
  if (!player_) {
    return;
  }
  switch (state) {
    case kSbPlayerStateInitialized:
      NOTREACHED();
      break;
    case kSbPlayerStatePrerolling:
#if SB_HAS(PLAYER_WITH_URL)
      if (is_url_based_) {
        break;
      }
#endif  // SB_HAS(PLAYER_WITH_URL)
      buffering_state_cb_.Run(kHaveMetadata);
      break;
    case kSbPlayerStatePresenting: {
#if SB_HAS(PLAYER_WITH_URL)
      if (is_url_based_) {
        duration_ = player_->GetDuration();
        start_date_ = player_->GetStartDate();
        buffering_state_cb_.Run(kHaveMetadata);
        int frame_width;
        int frame_height;
        player_->GetVideoResolution(&frame_width, &frame_height);
        bool natural_size_changed = (frame_width != natural_size_.width() ||
                                     frame_height != natural_size_.height());
        natural_size_ = gfx::Size(frame_width, frame_height);
        if (natural_size_changed) {
          content_size_change_cb_.Run();
        }
      }
#endif  // SB_HAS(PLAYER_WITH_URL)
      buffering_state_cb_.Run(kPrerollCompleted);
      if (!seek_cb_.is_null()) {
        CallSeekCB(PIPELINE_OK);
      }
      break;
    }
    case kSbPlayerStateEndOfStream:
      ended_cb_.Run(PIPELINE_OK);
      ended_ = true;
      break;
    case kSbPlayerStateDestroyed:
      break;
#if !SB_HAS(PLAYER_ERROR_MESSAGE)
    case kSbPlayerStateError:
      ResetAndRunIfNotNull(&error_cb_, PIPELINE_ERROR_DECODE,
                           "Pipeline player state error.");
      break;
#endif  // !SB_HAS(PLAYER_ERROR_MESSAGE)
  }
}

#if SB_HAS(PLAYER_ERROR_MESSAGE)
void SbPlayerPipeline::OnPlayerError(SbPlayerError error,
                                     const std::string& message) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // In case if Stop() has been called.
  if (!player_) {
    return;
  }

#if SB_HAS(PLAYER_WITH_URL)
  if (error >= kSbPlayerErrorMax) {
    DCHECK(is_url_based_);
    switch (static_cast<SbUrlPlayerError>(error)) {
      case kSbUrlPlayerErrorNetwork:
        ResetAndRunIfNotNull(&error_cb_, PIPELINE_ERROR_NETWORK, message);
        break;
      case kSbUrlPlayerErrorSrcNotSupported:
        ResetAndRunIfNotNull(&error_cb_, DEMUXER_ERROR_COULD_NOT_OPEN, message);
        break;
    }
    return;
  }
#endif  // SB_HAS(PLAYER_WITH_URL)

  switch (error) {
    case kSbPlayerErrorDecode:
      ResetAndRunIfNotNull(&error_cb_, PIPELINE_ERROR_DECODE, message);
      break;
#if SB_API_VERSION >= 10
    case kSbPlayerErrorCapabilityChanged:
      ResetAndRunIfNotNull(&error_cb_, PLAYBACK_CAPABILITY_CHANGED, message);
      break;
#endif  // SB_API_VERSION >= 10
#if SB_API_VERSION >= SB_HAS_PLAYER_ERROR_MAX_VERSION
    case kSbPlayerErrorMax:
      NOTREACHED();
      break;
#endif  // SB_API_VERSION >= SB_HAS_PLAYER_ERROR_MAX_VERSION
  }
}
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)

void SbPlayerPipeline::DelayedNeedData() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (audio_read_delayed_) {
    OnNeedData(DemuxerStream::AUDIO);
  }
}

void SbPlayerPipeline::UpdateDecoderConfig(DemuxerStream* stream) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (stream->type() == DemuxerStream::AUDIO) {
    const AudioDecoderConfig& decoder_config = stream->audio_decoder_config();
    player_->UpdateAudioConfig(decoder_config);
  } else {
    DCHECK_EQ(stream->type(), DemuxerStream::VIDEO);
    const VideoDecoderConfig& decoder_config = stream->video_decoder_config();
    base::AutoLock auto_lock(lock_);
    bool natural_size_changed =
        (decoder_config.natural_size().width() != natural_size_.width() ||
         decoder_config.natural_size().height() != natural_size_.height());
    natural_size_ = decoder_config.natural_size();
    player_->UpdateVideoConfig(decoder_config);
    if (natural_size_changed) {
      content_size_change_cb_.Run();
    }
  }
}

void SbPlayerPipeline::CallSeekCB(PipelineStatus status) {
  SeekCB seek_cb;
  bool is_initial_preroll;
  {
    base::AutoLock auto_lock(lock_);
    DCHECK(!seek_cb_.is_null());
    seek_cb = std::move(seek_cb_);
    is_initial_preroll = is_initial_preroll_;
    is_initial_preroll_ = false;
  }
  seek_cb.Run(status, is_initial_preroll);
}

void SbPlayerPipeline::SuspendTask(base::WaitableEvent* done_event) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(done_event);
  DCHECK(!suspended_);

  if (suspended_) {
    done_event->Signal();
    return;
  }

  if (player_) {
    player_->Suspend();
  }

  suspended_ = true;

  done_event->Signal();
}

void SbPlayerPipeline::ResumeTask(base::WaitableEvent* done_event) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(done_event);
  DCHECK(suspended_);

  if (!suspended_) {
    done_event->Signal();
    return;
  }

  if (player_) {
    player_->Resume();
  }

  suspended_ = false;

  done_event->Signal();
}

}  // namespace

// static
scoped_refptr<Pipeline> Pipeline::Create(
    PipelineWindow window,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    bool allow_resume_after_suspend, MediaLog* media_log,
    VideoFrameProvider* video_frame_provider) {
  return new SbPlayerPipeline(window, task_runner, allow_resume_after_suspend,
                              media_log, video_frame_provider);
}

}  // namespace media
}  // namespace cobalt
