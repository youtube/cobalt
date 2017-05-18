// Copyright 2016 Google Inc. All Rights Reserved.
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

#include <vector>

#include "base/basictypes.h"  // For COMPILE_ASSERT
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/bind_to_loop.h"
#include "media/base/channel_layout.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/filter_collection.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/base/pipeline.h"
#include "media/base/pipeline_status.h"
#include "media/base/ranges.h"
#include "media/base/sbplayer_set_bounds_helper.h"
#include "media/base/starboard_player.h"
#include "media/base/starboard_utils.h"
#include "media/base/video_decoder_config.h"
#include "media/crypto/starboard_decryptor.h"
#include "ui/gfx/size.h"

namespace media {

#if SB_HAS(PLAYER)

using base::Time;
using base::TimeDelta;

namespace {

// Used to post parameters to SbPlayerPipeline::StartTask() as the number of
// parameters exceed what base::Bind() can support.
struct StartTaskParameters {
  scoped_refptr<Demuxer> demuxer;
  SetDecryptorReadyCB decryptor_ready_cb;
  PipelineStatusCB ended_cb;
  PipelineStatusCB error_cb;
  PipelineStatusCB seek_cb;
  Pipeline::BufferingStateCB buffering_state_cb;
  base::Closure duration_change_cb;
};

// SbPlayerPipeline is a PipelineBase implementation that uses the SbPlayer
// interface internally.
class MEDIA_EXPORT SbPlayerPipeline : public Pipeline,
                                      public DemuxerHost,
                                      public StarboardPlayer::Host {
 public:
  // Constructs a media pipeline that will execute on |message_loop|.
  SbPlayerPipeline(PipelineWindow window,
                   const scoped_refptr<base::MessageLoopProxy>& message_loop,
                   MediaLog* media_log);
  ~SbPlayerPipeline() OVERRIDE;

  void Suspend() OVERRIDE;
  void Resume() OVERRIDE;
  void Start(scoped_ptr<FilterCollection> filter_collection,
             const SetDecryptorReadyCB& decryptor_ready_cb,
             const PipelineStatusCB& ended_cb,
             const PipelineStatusCB& error_cb,
             const PipelineStatusCB& seek_cb,
             const BufferingStateCB& buffering_state_cb,
             const base::Closure& duration_change_cb) OVERRIDE;

  void Stop(const base::Closure& stop_cb) OVERRIDE;
  void Seek(TimeDelta time, const PipelineStatusCB& seek_cb);
  bool HasAudio() const OVERRIDE;
  bool HasVideo() const OVERRIDE;

  float GetPlaybackRate() const OVERRIDE;
  void SetPlaybackRate(float playback_rate) OVERRIDE;
  float GetVolume() const OVERRIDE;
  void SetVolume(float volume) OVERRIDE;

  TimeDelta GetMediaTime() const OVERRIDE;
  Ranges<TimeDelta> GetBufferedTimeRanges() OVERRIDE;
  TimeDelta GetMediaDuration() const OVERRIDE;
  int64 GetTotalBytes() const OVERRIDE;
  void GetNaturalVideoSize(gfx::Size* out_size) const OVERRIDE;

  bool DidLoadingProgress() const OVERRIDE;
  PipelineStatistics GetStatistics() const OVERRIDE;
  SetBoundsCB GetSetBoundsCB() OVERRIDE;
  void SetDecodeToTextureOutputMode(bool preference) OVERRIDE;

 private:
  void StartTask(const StartTaskParameters& parameters);
  void SetVolumeTask(float volume);
  void SetPlaybackRateTask(float volume);
  void SetDurationTask(TimeDelta duration);

  // DataSourceHost (by way of DemuxerHost) implementation.
  void SetTotalBytes(int64 total_bytes) OVERRIDE;
  void AddBufferedByteRange(int64 start, int64 end) OVERRIDE;
  void AddBufferedTimeRange(TimeDelta start, TimeDelta end) OVERRIDE;

  // DemuxerHost implementaion.
  void SetDuration(TimeDelta duration) OVERRIDE;
  void OnDemuxerError(PipelineStatus error) OVERRIDE;

  void CreatePlayer(SbDrmSystem drm_system);
  void SetDecryptor(Decryptor* decryptor);
  void OnDemuxerInitialized(PipelineStatus status);
  void OnDemuxerSeeked(PipelineStatus status);
  void OnDemuxerStopped();
  void OnDemuxerStreamRead(DemuxerStream::Type type,
                           DemuxerStream::Status status,
                           const scoped_refptr<DecoderBuffer>& buffer);

  // StarboardPlayer::Host implementation.
  void OnNeedData(DemuxerStream::Type type) OVERRIDE;
  void OnPlayerStatus(SbPlayerState state) OVERRIDE;

  void UpdateDecoderConfig(const scoped_refptr<DemuxerStream>& stream);

  void SuspendTask(base::WaitableEvent* done_event);
  void ResumeTask(base::WaitableEvent* done_event);

  // Message loop used to execute pipeline tasks.  It is thread-safe.
  scoped_refptr<base::MessageLoopProxy> message_loop_;

  // The window this player associates with.  It should only be assigned in the
  // dtor and accesed once by SbPlayerCreate().
  PipelineWindow window_;

  // Lock used to serialize access for the following member variables.
  mutable base::Lock lock_;

  // Amount of available buffered data.  Set by filters.
  Ranges<int64> buffered_byte_ranges_;
  Ranges<TimeDelta> buffered_time_ranges_;

  // True when AddBufferedByteRange() has been called more recently than
  // DidLoadingProgress().
  mutable bool did_loading_progress_;

  // Total size of the media.  Set by filters.
  int64 total_bytes_;

  // Video's natural width and height.  Set by filters.
  gfx::Size natural_size_;

  // Current volume level (from 0.0f to 1.0f).  This value is set immediately
  // via SetVolume() and a task is dispatched on the message loop to notify the
  // filters.
  float volume_;

  // Current playback rate (>= 0.0f).  This value is set immediately via
  // SetPlaybackRate() and a task is dispatched on the message loop to notify
  // the filters.
  float playback_rate_;

  // Whether the media contains rendered audio and video streams.
  // TODO(fischman,scherkus): replace these with checks for
  // {audio,video}_decoder_ once extraction of {Audio,Video}Decoder from the
  // Filter heirarchy is done.
  bool has_audio_;
  bool has_video_;

  mutable PipelineStatistics statistics_;

  // The following member variables are only accessed by tasks posted to
  // |message_loop_|.

  // Temporary callback used for Stop().
  base::Closure stop_cb_;

  // Permanent callbacks passed in via Start().
  SetDecryptorReadyCB decryptor_ready_cb_;
  PipelineStatusCB ended_cb_;
  PipelineStatusCB error_cb_;
  BufferingStateCB buffering_state_cb_;
  base::Closure duration_change_cb_;
  base::optional<bool> decode_to_texture_output_mode_;

  // Demuxer reference used for setting the preload value.
  scoped_refptr<Demuxer> demuxer_;
  bool audio_read_in_progress_;
  bool video_read_in_progress_;
  TimeDelta duration_;

  scoped_refptr<SbPlayerSetBoundsHelper> set_bounds_helper_;

  // The following member variables can be accessed from WMPI thread but all
  // modifications to them happens on the pipeline thread.  So any access of
  // them from the WMPI thread and any modification to them on the pipeline
  // thread has to guarded by lock.  Access to them from the pipeline thread
  // needn't to be guarded.

  // Temporary callback used for Start() and Seek().
  PipelineStatusCB seek_cb_;
  base::TimeDelta seek_time_;
  scoped_ptr<StarboardPlayer> player_;
  bool suspended_;

  DISALLOW_COPY_AND_ASSIGN(SbPlayerPipeline);
};

SbPlayerPipeline::SbPlayerPipeline(
    PipelineWindow window,
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    MediaLog* media_log)
    : window_(window),
      message_loop_(message_loop),
      total_bytes_(0),
      natural_size_(0, 0),
      volume_(1.f),
      playback_rate_(0.f),
      has_audio_(false),
      has_video_(false),
      audio_read_in_progress_(false),
      video_read_in_progress_(false),
      set_bounds_helper_(new SbPlayerSetBoundsHelper),
      suspended_(false) {}

SbPlayerPipeline::~SbPlayerPipeline() {
  DCHECK(!player_);
}

void SbPlayerPipeline::Suspend() {
  DCHECK(!message_loop_->BelongsToCurrentThread());

  base::WaitableEvent waitable_event(true, /* manual_reset */
                                     false /* initially_signaled */);
  message_loop_->PostTask(FROM_HERE,
                          base::Bind(&SbPlayerPipeline::SuspendTask,
                                     base::Unretained(this), &waitable_event));
  waitable_event.Wait();
}

void SbPlayerPipeline::Resume() {
  DCHECK(!message_loop_->BelongsToCurrentThread());

  base::WaitableEvent waitable_event(true, /* manual_reset */
                                     false /* initially_signaled */);
  message_loop_->PostTask(FROM_HERE,
                          base::Bind(&SbPlayerPipeline::ResumeTask,
                                     base::Unretained(this), &waitable_event));
  waitable_event.Wait();
}

void SbPlayerPipeline::Start(scoped_ptr<FilterCollection> filter_collection,
                             const SetDecryptorReadyCB& decryptor_ready_cb,
                             const PipelineStatusCB& ended_cb,
                             const PipelineStatusCB& error_cb,
                             const PipelineStatusCB& seek_cb,
                             const BufferingStateCB& buffering_state_cb,
                             const base::Closure& duration_change_cb) {
  DCHECK(filter_collection);

  StartTaskParameters parameters;
  parameters.demuxer = filter_collection->GetDemuxer();
  parameters.decryptor_ready_cb = decryptor_ready_cb;
  parameters.ended_cb = ended_cb;
  parameters.error_cb = error_cb;
  parameters.seek_cb = seek_cb;
  parameters.buffering_state_cb = buffering_state_cb;
  parameters.duration_change_cb = duration_change_cb;

  message_loop_->PostTask(
      FROM_HERE, base::Bind(&SbPlayerPipeline::StartTask, this, parameters));
}

void SbPlayerPipeline::Stop(const base::Closure& stop_cb) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(FROM_HERE,
                            base::Bind(&SbPlayerPipeline::Stop, this, stop_cb));
    return;
  }

  DCHECK(stop_cb_.is_null());
  DCHECK(!stop_cb.is_null());

  if (player_) {
    scoped_ptr<StarboardPlayer> player;
    {
      base::AutoLock auto_lock(lock_);
      player = player_.Pass();
    }

    DLOG(INFO) << "Destroying SbPlayer.";
    player.reset();
    DLOG(INFO) << "SbPlayer destroyed.";
  }

  // When Stop() is in progress, we no longer need to call |error_cb_|.
  error_cb_.Reset();
  if (demuxer_) {
    stop_cb_ = stop_cb;
    demuxer_->Stop(base::Bind(&SbPlayerPipeline::OnDemuxerStopped, this));
  } else {
    stop_cb.Run();
  }
}

void SbPlayerPipeline::Seek(TimeDelta time, const PipelineStatusCB& seek_cb) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&SbPlayerPipeline::Seek, this, time, seek_cb));
    return;
  }

  if (!player_) {
    seek_cb.Run(PIPELINE_ERROR_INVALID_STATE);
    return;
  }

  player_->PrepareForSeek();

  DCHECK(seek_cb_.is_null());
  DCHECK(!seek_cb.is_null());

  if (audio_read_in_progress_ || video_read_in_progress_) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&SbPlayerPipeline::Seek, this, time, seek_cb));
    return;
  }

  {
    base::AutoLock auto_lock(lock_);
    seek_cb_ = seek_cb;
    seek_time_ = time;
  }
  demuxer_->Seek(time, BindToCurrentLoop(base::Bind(
                           &SbPlayerPipeline::OnDemuxerSeeked, this)));
}

bool SbPlayerPipeline::HasAudio() const {
  base::AutoLock auto_lock(lock_);
  return has_audio_;
}

bool SbPlayerPipeline::HasVideo() const {
  base::AutoLock auto_lock(lock_);
  return has_video_;
}

float SbPlayerPipeline::GetPlaybackRate() const {
  base::AutoLock auto_lock(lock_);
  return playback_rate_;
}

void SbPlayerPipeline::SetPlaybackRate(float playback_rate) {
  base::AutoLock auto_lock(lock_);
  playback_rate_ = playback_rate;
  message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&SbPlayerPipeline::SetPlaybackRateTask, this, playback_rate));
}

float SbPlayerPipeline::GetVolume() const {
  base::AutoLock auto_lock(lock_);
  return volume_;
}

void SbPlayerPipeline::SetVolume(float volume) {
  if (volume < 0.0f || volume > 1.0f)
    return;

  base::AutoLock auto_lock(lock_);
  volume_ = volume;
  message_loop_->PostTask(
      FROM_HERE, base::Bind(&SbPlayerPipeline::SetVolumeTask, this, volume));
}

TimeDelta SbPlayerPipeline::GetMediaTime() const {
  base::AutoLock auto_lock(lock_);

  if (!seek_cb_.is_null()) {
    return seek_time_;
  }
  if (!player_) {
    return TimeDelta();
  }
  base::TimeDelta media_time;
  player_->GetInfo(&statistics_.video_frames_decoded,
                   &statistics_.video_frames_dropped, &media_time);
  return media_time;
}

Ranges<TimeDelta> SbPlayerPipeline::GetBufferedTimeRanges() {
  base::AutoLock auto_lock(lock_);
  Ranges<TimeDelta> time_ranges;
  for (size_t i = 0; i < buffered_time_ranges_.size(); ++i) {
    time_ranges.Add(buffered_time_ranges_.start(i),
                    buffered_time_ranges_.end(i));
  }
  NOTIMPLEMENTED();
  /*if (clock_->Duration() == TimeDelta() || total_bytes_ == 0)
    return time_ranges;
  for (size_t i = 0; i < buffered_byte_ranges_.size(); ++i) {
    TimeDelta start = TimeForByteOffset_Locked(buffered_byte_ranges_.start(i));
    TimeDelta end = TimeForByteOffset_Locked(buffered_byte_ranges_.end(i));
    // Cap approximated buffered time at the length of the video.
    end = std::min(end, clock_->Duration());
    time_ranges.Add(start, end);
  }*/

  return time_ranges;
}

TimeDelta SbPlayerPipeline::GetMediaDuration() const {
  base::AutoLock auto_lock(lock_);
  return duration_;
}

int64 SbPlayerPipeline::GetTotalBytes() const {
  base::AutoLock auto_lock(lock_);
  return total_bytes_;
}

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
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&SbPlayerPipeline::SetDecodeToTextureOutputMode,
                              this, enabled));
    return;
  }

  decode_to_texture_output_mode_ = enabled;
}

void SbPlayerPipeline::StartTask(const StartTaskParameters& parameters) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  DCHECK(!demuxer_);

  demuxer_ = parameters.demuxer;
  decryptor_ready_cb_ = parameters.decryptor_ready_cb;
  ended_cb_ = parameters.ended_cb;
  error_cb_ = parameters.error_cb;
  {
    base::AutoLock auto_lock(lock_);
    seek_cb_ = parameters.seek_cb;
  }
  buffering_state_cb_ = parameters.buffering_state_cb;
  duration_change_cb_ = parameters.duration_change_cb;

  demuxer_->Initialize(
      this, BindToCurrentLoop(
                base::Bind(&SbPlayerPipeline::OnDemuxerInitialized, this)));
}

void SbPlayerPipeline::SetVolumeTask(float volume) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (player_) {
    player_->SetVolume(volume_);
  }
}

void SbPlayerPipeline::SetPlaybackRateTask(float volume) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (player_) {
    player_->SetPlaybackRate(playback_rate_);
  }
}

void SbPlayerPipeline::SetDurationTask(TimeDelta duration) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (!duration_change_cb_.is_null()) {
    duration_change_cb_.Run();
  }
}

void SbPlayerPipeline::SetTotalBytes(int64 total_bytes) {
  base::AutoLock auto_lock(lock_);
  total_bytes_ = total_bytes;
}

void SbPlayerPipeline::SetDuration(TimeDelta duration) {
  base::AutoLock auto_lock(lock_);
  duration_ = duration;
  message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&SbPlayerPipeline::SetDurationTask, this, duration));
}

void SbPlayerPipeline::OnDemuxerError(PipelineStatus error) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&SbPlayerPipeline::OnDemuxerError, this, error));
    return;
  }

  if (error != PIPELINE_OK) {
    ResetAndRunIfNotNull(&error_cb_, error);
  }
}

void SbPlayerPipeline::AddBufferedByteRange(int64 start, int64 end) {
  base::AutoLock auto_lock(lock_);
  buffered_byte_ranges_.Add(start, end);
  did_loading_progress_ = true;
}

void SbPlayerPipeline::AddBufferedTimeRange(TimeDelta start, TimeDelta end) {
  base::AutoLock auto_lock(lock_);
  buffered_time_ranges_.Add(start, end);
  did_loading_progress_ = true;
}

void SbPlayerPipeline::CreatePlayer(SbDrmSystem drm_system) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (suspended_) {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&SbPlayerPipeline::CreatePlayer, this, drm_system));
    return;
  }

  // TODO:  Check |suspended_| here as the pipeline can be suspended before the
  // player is created.  In this case we should delay creating the player as the
  // creation of player may fail.
  const AudioDecoderConfig& audio_config =
      demuxer_->GetStream(DemuxerStream::AUDIO)->audio_decoder_config();
  const VideoDecoderConfig& video_config =
      demuxer_->GetStream(DemuxerStream::VIDEO)->video_decoder_config();

  {
    base::AutoLock auto_lock(lock_);
    player_.reset(new StarboardPlayer(
        message_loop_, audio_config, video_config, window_, drm_system, this,
        set_bounds_helper_.get(), *decode_to_texture_output_mode_));
    SetPlaybackRateTask(playback_rate_);
    SetVolumeTask(volume_);
  }

  if (player_->IsValid()) {
    return;
  }

  player_.reset();

  PipelineStatusCB seek_cb;
  {
    base::AutoLock auto_lock(lock_);
    DCHECK(!seek_cb_.is_null());
    seek_cb = base::ResetAndReturn(&seek_cb_);
  }
  seek_cb.Run(DECODER_ERROR_NOT_SUPPORTED);
}

void SbPlayerPipeline::SetDecryptor(Decryptor* decryptor) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (!decryptor) {
    return;
  }
  StarboardDecryptor* sb_decryptor =
      reinterpret_cast<StarboardDecryptor*>(decryptor);
  CreatePlayer(sb_decryptor->drm_system());
}

void SbPlayerPipeline::OnDemuxerInitialized(PipelineStatus status) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (status != PIPELINE_OK) {
    ResetAndRunIfNotNull(&error_cb_, status);
    return;
  }

  if (demuxer_->GetStream(DemuxerStream::AUDIO) == NULL ||
      demuxer_->GetStream(DemuxerStream::VIDEO) == NULL) {
    ResetAndRunIfNotNull(&error_cb_, DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
    return;
  }

  {
    base::AutoLock auto_lock(lock_);
    has_audio_ = demuxer_->GetStream(DemuxerStream::AUDIO) != NULL;
    DCHECK(has_audio_);
    has_video_ = demuxer_->GetStream(DemuxerStream::VIDEO) != NULL;

    buffering_state_cb_.Run(kHaveMetadata);

    const AudioDecoderConfig& audio_config =
        demuxer_->GetStream(DemuxerStream::AUDIO)->audio_decoder_config();
    bool is_encrypted = audio_config.is_encrypted();
    if (has_video_) {
      const VideoDecoderConfig& video_config =
          demuxer_->GetStream(DemuxerStream::VIDEO)->video_decoder_config();
      natural_size_ = video_config.natural_size();
      is_encrypted |= video_config.is_encrypted();
    }
    if (is_encrypted) {
      decryptor_ready_cb_.Run(
          BindToCurrentLoop(base::Bind(&SbPlayerPipeline::SetDecryptor, this)));
      return;
    }
  }

  CreatePlayer(kSbDrmSystemInvalid);
}

void SbPlayerPipeline::OnDemuxerSeeked(PipelineStatus status) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (status == PIPELINE_OK) {
    player_->Seek(seek_time_);
  }
}

void SbPlayerPipeline::OnDemuxerStopped() {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&SbPlayerPipeline::OnDemuxerStopped, this));
    return;
  }

  base::ResetAndReturn(&stop_cb_).Run();
}

void SbPlayerPipeline::OnDemuxerStreamRead(
    DemuxerStream::Type type,
    DemuxerStream::Status status,
    const scoped_refptr<DecoderBuffer>& buffer) {
  DCHECK(type == DemuxerStream::AUDIO || type == DemuxerStream::VIDEO)
      << "Unsupported DemuxerStream::Type " << type;

  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&SbPlayerPipeline::OnDemuxerStreamRead, this,
                              type, status, buffer));
    return;
  }

  scoped_refptr<DemuxerStream> stream = demuxer_->GetStream(type);

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
      PipelineStatusCB seek_cb;
      {
        base::AutoLock auto_lock(lock_);
        seek_cb = base::ResetAndReturn(&seek_cb_);
      }
      seek_cb.Run(PIPELINE_OK);
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
  } else {
    video_read_in_progress_ = false;
  }

  player_->WriteBuffer(type, buffer);
}

void SbPlayerPipeline::OnNeedData(DemuxerStream::Type type) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // In case if Stop() has been called.
  if (!player_) {
    return;
  }

  if (type == DemuxerStream::AUDIO) {
    if (audio_read_in_progress_) {
      return;
    }
    audio_read_in_progress_ = true;
  } else {
    DCHECK_EQ(type, DemuxerStream::VIDEO);
    if (video_read_in_progress_) {
      return;
    }
    video_read_in_progress_ = true;
  }
  scoped_refptr<DemuxerStream> stream = demuxer_->GetStream(type);
  stream->Read(base::Bind(&SbPlayerPipeline::OnDemuxerStreamRead, this, type));
}

void SbPlayerPipeline::OnPlayerStatus(SbPlayerState state) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // In case if Stop() has been called.
  if (!player_) {
    return;
  }
  switch (state) {
    case kSbPlayerStateInitialized:
      NOTREACHED();
      break;
    case kSbPlayerStatePrerolling:
      break;
    case kSbPlayerStatePresenting:
      buffering_state_cb_.Run(kPrerollCompleted);
      if (!seek_cb_.is_null()) {
        PipelineStatusCB seek_cb;
        {
          base::AutoLock auto_lock(lock_);
          seek_cb = base::ResetAndReturn(&seek_cb_);
        }
        seek_cb.Run(PIPELINE_OK);
      }
      break;
    case kSbPlayerStateEndOfStream:
      ended_cb_.Run(PIPELINE_OK);
      break;
    case kSbPlayerStateDestroyed:
      break;
    case kSbPlayerStateError:
      ResetAndRunIfNotNull(&error_cb_, PIPELINE_ERROR_DECODE);
      break;
  }
}

void SbPlayerPipeline::UpdateDecoderConfig(
    const scoped_refptr<DemuxerStream>& stream) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (stream->type() == DemuxerStream::AUDIO) {
    stream->audio_decoder_config();
  } else {
    DCHECK_EQ(stream->type(), DemuxerStream::VIDEO);
    const VideoDecoderConfig& decoder_config = stream->video_decoder_config();
    base::AutoLock auto_lock(lock_);
    natural_size_ = decoder_config.natural_size();
    player_->UpdateVideoResolution(static_cast<int>(natural_size_.width()),
                                   static_cast<int>(natural_size_.height()));
  }
}

void SbPlayerPipeline::SuspendTask(base::WaitableEvent* done_event) {
  DCHECK(message_loop_->BelongsToCurrentThread());
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
  DCHECK(message_loop_->BelongsToCurrentThread());
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

#endif  // SB_HAS(PLAYER)

scoped_refptr<Pipeline> Pipeline::Create(
    PipelineWindow window,
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    MediaLog* media_log) {
#if SB_HAS(PLAYER)
  return new SbPlayerPipeline(window, message_loop, media_log);
#else
  return NULL;
#endif
}

}  // namespace media
