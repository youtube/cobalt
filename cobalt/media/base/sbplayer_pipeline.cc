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

#include "cobalt/media/base/sbplayer_pipeline.h"

#include <algorithm>
#include <utility>

#include "base/basictypes.h"  // For COMPILE_ASSERT
#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/c_val.h"
#include "cobalt/base/startup_timer.h"
#include "starboard/common/media.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"

namespace cobalt {
namespace media {
namespace {

using ::starboard::GetMediaAudioConnectorName;

static const int kRetryDelayAtSuspendInMilliseconds = 100;

// In the OnNeedData(), it attempts to write one more audio access
// unit than the audio write duration. Specifically, the check
// |time_ahead_of_playback_for_preroll| > |adjusted_write_duration_for_preroll|
// is used to skip audio writing, using '>' instead of '>='.
// Since the calculated write duration during preroll may align exactly
// with the audio write duration, the current check can fail, leading to an
// additional call to SbPlayerWriteSamples(). By writing an extra guard audio
// buffer, this extra write during preroll can be eliminated.
const int kPrerollGuardAudioBuffer = 1;

unsigned int g_pipeline_identifier_counter = 0;

bool HasRemoteAudioOutputs(
    const std::vector<SbMediaAudioConfiguration>& configurations) {
  for (auto&& configuration : configurations) {
    const auto connector = configuration.connector;
    switch (connector) {
      case kSbMediaAudioConnectorUnknown:
      case kSbMediaAudioConnectorAnalog:
      case kSbMediaAudioConnectorBuiltIn:
      case kSbMediaAudioConnectorHdmi:
      case kSbMediaAudioConnectorSpdif:
      case kSbMediaAudioConnectorUsb:
        LOG(INFO) << "Encountered local audio connector: "
                  << GetMediaAudioConnectorName(connector);
        break;
      case kSbMediaAudioConnectorBluetooth:
      case kSbMediaAudioConnectorRemoteWired:
      case kSbMediaAudioConnectorRemoteWireless:
      case kSbMediaAudioConnectorRemoteOther:
        LOG(INFO) << "Encountered remote audio connector: "
                  << GetMediaAudioConnectorName(connector);
        return true;
    }
  }

  LOG(INFO) << "No remote audio outputs found.";

  return false;
}

// The function adjusts audio write duration proportionally to the playback
// rate, when the playback rate is greater than 1.0.
//
// Having the right write duration is important:
// 1. Too small of it causes audio underflow.
// 2. Too large of it causes excessive audio switch latency.
// When playback rate is 2x, an 0.5 seconds of write duration effectively only
// lasts for 0.25 seconds and causes audio underflow, and the function will
// adjust it to 1 second in this case.
TimeDelta AdjustWriteDurationForPlaybackRate(TimeDelta write_duration,
                                             float playback_rate) {
  if (playback_rate <= 1.0) {
    return write_duration;
  }

  return write_duration * playback_rate;
}

// The function returns the default frames per DecoderBuffer.
//
// The number of frames is used to estimate the number of samples per write for
// audio preroll according to |audio_write_duration_|.
template <typename ChromeMedia>
inline int GetDefaultAudioFramesPerBuffer(
    typename ChromeMedia::AudioCodec codec) {
  switch (codec) {
    case ChromeMedia::AudioCodec::kOpus:
      return 960;
    case ChromeMedia::AudioCodec::kAAC:
      return 1024;
    case ChromeMedia::AudioCodec::kAC3:
    case ChromeMedia::AudioCodec::kEAC3:
      return 1536;
    default:
      NOTREACHED();
      return 1;
  }
}

}  // namespace

template <typename ChromeMedia>
SbPlayerPipeline<ChromeMedia>::SbPlayerPipeline(
    SbPlayerInterface* interface, PipelineWindow window,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const GetDecodeTargetGraphicsContextProviderFunc&
        get_decode_target_graphics_context_provider_func,
    bool allow_resume_after_suspend, int max_audio_samples_per_write,
    bool force_punch_out_by_default, TimeDelta audio_write_duration_local,
    TimeDelta audio_write_duration_remote, MediaLog* media_log,
    MediaMetricsProvider* media_metrics_provider,
    DecodeTargetProvider* decode_target_provider)
    : pipeline_identifier_(
          base::StringPrintf("%X", g_pipeline_identifier_counter++)),
      sbplayer_interface_(interface),
      task_runner_(task_runner),
      allow_resume_after_suspend_(allow_resume_after_suspend),
      max_audio_samples_per_write_(max_audio_samples_per_write),
      window_(window),
      get_decode_target_graphics_context_provider_func_(
          get_decode_target_graphics_context_provider_func),
      natural_size_(0, 0),
      volume_(base::StringPrintf("Media.Pipeline.%s.Volume",
                                 pipeline_identifier_.c_str()),
              1.0f, "Volume of the media pipeline."),
      playback_rate_(base::StringPrintf("Media.Pipeline.%s.PlaybackRate",
                                        pipeline_identifier_.c_str()),
                     0.0f, "Playback rate of the media pipeline."),
      duration_(base::StringPrintf("Media.Pipeline.%s.Duration",
                                   pipeline_identifier_.c_str()),
                TimeDelta(), "Playback duration of the media pipeline."),
      set_bounds_helper_(new SbPlayerSetBoundsHelper),
      started_(base::StringPrintf("Media.Pipeline.%s.Started",
                                  pipeline_identifier_.c_str()),
               false, "Whether the media pipeline has started."),
      suspended_(base::StringPrintf("Media.Pipeline.%s.Suspended",
                                    pipeline_identifier_.c_str()),
                 false,
                 "Whether the media pipeline has been suspended. The "
                 "pipeline is usually suspended after the app enters "
                 "background mode."),
      stopped_(base::StringPrintf("Media.Pipeline.%s.Stopped",
                                  pipeline_identifier_.c_str()),
               false, "Whether the media pipeline has stopped."),
      ended_(base::StringPrintf("Media.Pipeline.%s.Ended",
                                pipeline_identifier_.c_str()),
             false, "Whether the media pipeline has ended."),
      player_state_(base::StringPrintf("Media.Pipeline.%s.PlayerState",
                                       pipeline_identifier_.c_str()),
                    kSbPlayerStateInitialized,
                    "The underlying SbPlayer state of the media pipeline."),
      decode_target_provider_(decode_target_provider),
      audio_write_duration_local_(audio_write_duration_local),
      audio_write_duration_remote_(audio_write_duration_remote),
      media_metrics_provider_(media_metrics_provider),
      last_media_time_(base::StringPrintf("Media.Pipeline.%s.LastMediaTime",
                                          pipeline_identifier_.c_str()),
                       TimeDelta(),
                       "Last media time reported by the underlying player."),
      max_video_capabilities_(
          base::StringPrintf("Media.Pipeline.%s.MaxVideoCapabilities",
                             pipeline_identifier_.c_str()),
          "", "The max video capabilities required for the media pipeline."),
      playback_statistics_(pipeline_identifier_) {
  if (force_punch_out_by_default) {
    default_output_mode_ = kSbPlayerOutputModePunchOut;
  }
}

template <typename ChromeMedia>
SbPlayerPipeline<ChromeMedia>::~SbPlayerPipeline() {
  DCHECK(!player_bridge_);
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::Suspend() {
  DCHECK(!task_runner_->RunsTasksInCurrentSequence());

  base::WaitableEvent waitable_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&SbPlayerPipeline<ChromeMedia>::SuspendTask,
                                    base::Unretained(this), &waitable_event));
  waitable_event.Wait();
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::Resume(PipelineWindow window) {
  DCHECK(!task_runner_->RunsTasksInCurrentSequence());

  base::WaitableEvent waitable_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&SbPlayerPipeline<ChromeMedia>::ResumeTask,
                            base::Unretained(this), window, &waitable_event));
  waitable_event.Wait();
}

#if SB_HAS(PLAYER_WITH_URL)
void OnEncryptedMediaInitDataEncountered(
    const Pipeline::OnEncryptedMediaInitDataEncounteredCB&
        on_encrypted_media_init_data_encountered,
    const char* init_data_type, const unsigned char* init_data,
    unsigned int init_data_length) {
  LOG_IF(WARNING, strcmp(init_data_type, "cenc") != 0 &&
                      strcmp(init_data_type, "fairplay") != 0 &&
                      strcmp(init_data_type, "keyids") != 0 &&
                      strcmp(init_data_type, "webm") != 0)
      << "Unknown EME initialization data type: " << init_data_type;

  std::vector<uint8_t> init_data_vec(init_data, init_data + init_data_length);
  DCHECK(!on_encrypted_media_init_data_encountered.is_null());
  on_encrypted_media_init_data_encountered.Run(init_data_type, init_data_vec);
}
#endif  // SB_HAS(PLAYER_WITH_URL)

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::Start(
    Demuxer* demuxer, const SetDrmSystemReadyCB& set_drm_system_ready_cb,
    const PipelineStatusCB& ended_cb, const ErrorCB& error_cb,
    const SeekCB& seek_cb, const BufferingStateCB& buffering_state_cb,
    const base::Closure& duration_change_cb,
    const base::Closure& output_mode_change_cb,
    const base::Closure& content_size_change_cb,
    const std::string& max_video_capabilities, const int max_video_input_size) {
  TRACE_EVENT0("cobalt::media", "SbPlayerPipeline<ChromeMedia>::Start");

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
  parameters.max_video_input_size = max_video_input_size;
#if SB_HAS(PLAYER_WITH_URL)
  parameters.is_url_based = false;
#endif  // SB_HAS(PLAYER_WITH_URL)

  task_runner_->PostTask(
      FROM_HERE, base::Bind(&SbPlayerPipeline<ChromeMedia>::StartTask, this,
                            base::Passed(&parameters)));
}

#if SB_HAS(PLAYER_WITH_URL)
template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::Start(
    const SetDrmSystemReadyCB& set_drm_system_ready_cb,
    const OnEncryptedMediaInitDataEncounteredCB&
        on_encrypted_media_init_data_encountered_cb,
    const std::string& source_url, const PipelineStatusCB& ended_cb,
    const ErrorCB& error_cb, const SeekCB& seek_cb,
    const BufferingStateCB& buffering_state_cb,
    const base::Closure& duration_change_cb,
    const base::Closure& output_mode_change_cb,
    const base::Closure& content_size_change_cb) {
  TRACE_EVENT0("cobalt::media", "SbPlayerPipeline<ChromeMedia>::Start");

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
  RunSetDrmSystemReadyCB(
      base::Bind(&SbPlayerPipeline<ChromeMedia>::SetDrmSystem, this));

  task_runner_->PostTask(
      FROM_HERE, base::Bind(&SbPlayerPipeline<ChromeMedia>::StartTask, this,
                            base::Passed(&parameters)));
}
#endif  // SB_HAS(PLAYER_WITH_URL)

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::Stop(const base::Closure& stop_cb) {
  TRACE_EVENT0("cobalt::media", "SbPlayerPipeline<ChromeMedia>::Stop");

  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SbPlayerPipeline<ChromeMedia>::Stop, this, stop_cb));
    return;
  }

  DCHECK(stop_cb_.is_null());
  DCHECK(!stop_cb.is_null());

  stopped_ = true;

  if (player_bridge_) {
    decltype(player_bridge_) player_bridge;
    {
      base::AutoLock auto_lock(lock_);
      player_bridge = std::move(player_bridge_);
    }

    LOG(INFO) << "Destroying SbPlayer.";
    player_bridge.reset();
    LOG(INFO) << "SbPlayer destroyed.";
  }

  // When Stop() is in progress, we no longer need to call |error_cb_|.
  error_cb_.Reset();
  if (demuxer_) {
    stop_cb_ = stop_cb;
    demuxer_->Stop();
    video_stream_ = nullptr;
    audio_stream_ = nullptr;
    OnDemuxerStopped();
  } else {
    stop_cb.Run();
  }
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::Seek(TimeDelta time,
                                         const SeekCB& seek_cb) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SbPlayerPipeline<ChromeMedia>::Seek, this, time, seek_cb));
    return;
  }

  playback_statistics_.OnSeek(time);

  if (!player_bridge_) {
    seek_cb.Run(
        pipeline_status_cast<PipelineStatus>(
            ::media::PIPELINE_ERROR_INVALID_STATE),
        false,
        AppendStatisticsString("SbPlayerPipeline<ChromeMedia>::Seek failed: "
                               "player_bridge_ is nullptr."));
    return;
  }

  player_bridge_->PrepareForSeek();
  ended_ = false;

  DCHECK(seek_cb_.is_null());
  DCHECK(!seek_cb.is_null());

  if (audio_read_in_progress_ || video_read_in_progress_) {
    const TimeDelta kDelay = base::Milliseconds(50);
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&SbPlayerPipeline<ChromeMedia>::Seek, this, time, seek_cb),
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
  retrograde_media_time_counter_ = 0;
  timestamp_of_last_written_audio_ = TimeDelta();
  is_video_eos_written_ = false;

#if SB_HAS(PLAYER_WITH_URL)
  if (is_url_based_) {
    player_bridge_->Seek(seek_time_);
    return;
  }
#endif  // SB_HAS(PLAYER_WITH_URL)
  demuxer_->Seek(time,
                 BindPostTaskToCurrentDefault(base::Bind(
                     &SbPlayerPipeline<ChromeMedia>::OnDemuxerSeeked, this)));
}

template <typename ChromeMedia>
bool SbPlayerPipeline<ChromeMedia>::HasAudio() const {
  base::AutoLock auto_lock(lock_);
  return audio_stream_ != NULL;
}

template <typename ChromeMedia>
bool SbPlayerPipeline<ChromeMedia>::HasVideo() const {
  base::AutoLock auto_lock(lock_);
  return video_stream_ != NULL;
}

template <typename ChromeMedia>
float SbPlayerPipeline<ChromeMedia>::GetPlaybackRate() const {
  base::AutoLock auto_lock(lock_);
  return playback_rate_;
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::SetPlaybackRate(float playback_rate) {
  base::AutoLock auto_lock(lock_);
  playback_rate_ = playback_rate;
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&SbPlayerPipeline<ChromeMedia>::SetPlaybackRateTask,
                            this, playback_rate));
}

template <typename ChromeMedia>
float SbPlayerPipeline<ChromeMedia>::GetVolume() const {
  base::AutoLock auto_lock(lock_);
  return volume_;
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::SetVolume(float volume) {
  if (volume < 0.0f || volume > 1.0f) return;

  base::AutoLock auto_lock(lock_);
  volume_ = volume;
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SbPlayerPipeline<ChromeMedia>::SetVolumeTask, this, volume));
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::StoreMediaTime(TimeDelta media_time) {
  last_media_time_ = media_time;
  last_time_media_time_retrieved_ = Time::Now();
}

template <typename ChromeMedia>
TimeDelta SbPlayerPipeline<ChromeMedia>::GetMediaTime() {
  base::AutoLock auto_lock(lock_);

  if (!seek_cb_.is_null()) {
    StoreMediaTime(seek_time_);
    return seek_time_;
  }
  if (!player_bridge_) {
    StoreMediaTime(TimeDelta());
    return TimeDelta();
  }
  if (ended_) {
    StoreMediaTime(duration_);
    return duration_;
  }
  TimeDelta media_time;
#if SB_HAS(PLAYER_WITH_URL)
  if (is_url_based_) {
    int frame_width;
    int frame_height;
    player_bridge_->GetVideoResolution(&frame_width, &frame_height);
    if (frame_width != natural_size_.width() ||
        frame_height != natural_size_.height()) {
      natural_size_ = gfx::Size(frame_width, frame_height);
      content_size_change_cb_.Run();
    }
  }
#endif  // SB_HAS(PLAYER_WITH_URL)
  player_bridge_->GetInfo(&video_statistics_.video_frames_decoded,
                          &video_statistics_.video_frames_dropped, &media_time);

  // Guarantee that we report monotonically increasing media time
  if (media_time < last_media_time_) {
    if (retrograde_media_time_counter_ == 0) {
      DLOG(WARNING) << "Received retrograde media time, new:"
                    << media_time.InMicroseconds()
                    << ", last: " << last_media_time_ << ".";
    }
    media_time = last_media_time_;
    retrograde_media_time_counter_++;
  } else if (retrograde_media_time_counter_ != 0) {
    DLOG(WARNING) << "Received " << retrograde_media_time_counter_
                  << " retrograde media time before recovered.";
    retrograde_media_time_counter_ = 0;
  }
  StoreMediaTime(media_time);
  return media_time;
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::UpdateBufferedTimeRanges(
    const AddRangeCB& add_range_cb) {
  DCHECK(add_range_cb);

  // Make a copy under the lock, then process the ranges without the lock.
  decltype(buffered_time_ranges_) buffered_time_ranges;

  {
    base::AutoLock auto_lock(lock_);

#if SB_HAS(PLAYER_WITH_URL)
    Ranges time_ranges;

    if (!player_bridge_) {
      return;
    }

    if (is_url_based_) {
      TimeDelta media_time;
      TimeDelta buffer_start_time;
      TimeDelta buffer_length_time;
      player_bridge_->GetInfo(&video_statistics_.video_frames_decoded,
                              &video_statistics_.video_frames_dropped,
                              &media_time);
      player_bridge_->GetUrlPlayerBufferedTimeRanges(&buffer_start_time,
                                                     &buffer_length_time);

      if (buffer_length_time.InSeconds() == 0) {
        // The range is empty, set |buffered_time_ranges_| to empty range and
        // return as is.
        buffered_time_ranges_ = time_ranges;
        return;
      }

      time_ranges.Add(buffer_start_time,
                      buffer_start_time + buffer_length_time);

      if (buffered_time_ranges_.size() > 0) {
        TimeDelta old_buffer_start_time = buffered_time_ranges_.start(0);
        TimeDelta old_buffer_length_time = buffered_time_ranges_.end(0);
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
      buffered_time_ranges = buffered_time_ranges_;
    }
#else   // SB_HAS(PLAYER_WITH_URL)
    buffered_time_ranges = buffered_time_ranges_;
#endif  // SB_HAS(PLAYER_WITH_URL)
  }

  for (int i = 0; i < static_cast<int>(buffered_time_ranges.size()); ++i) {
    add_range_cb(buffered_time_ranges.start(i).InSecondsF(),
                 buffered_time_ranges.end(i).InSecondsF());
  }
}

template <typename ChromeMedia>
TimeDelta SbPlayerPipeline<ChromeMedia>::GetMediaDuration() const {
  base::AutoLock auto_lock(lock_);
  return duration_;
}

#if SB_HAS(PLAYER_WITH_URL)
template <typename ChromeMedia>
TimeDelta SbPlayerPipeline<ChromeMedia>::GetMediaStartDate() const {
  base::AutoLock auto_lock(lock_);
  return start_date_;
}
#endif  // SB_HAS(PLAYER_WITH_URL)

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::GetNaturalVideoSize(
    gfx::Size* out_size) const {
  CHECK(out_size);
  base::AutoLock auto_lock(lock_);
  *out_size = natural_size_;
}

template <typename ChromeMedia>
std::vector<std::string> SbPlayerPipeline<ChromeMedia>::GetAudioConnectors()
    const {
  base::AutoLock auto_lock(lock_);
  if (!player_bridge_) {
    return std::vector<std::string>();
  }

  std::vector<std::string> connectors;

#if SB_HAS(PLAYER_WITH_URL)
  // Url based player does not support audio connectors.
  if (is_url_based_) {
    return connectors;
  }
#endif  // SB_HAS(PLAYER_WITH_URL)

  auto configurations = player_bridge_->GetAudioConfigurations();
  for (auto&& configuration : configurations) {
    connectors.push_back(GetMediaAudioConnectorName(configuration.connector));
  }

  return connectors;
}

template <typename ChromeMedia>
bool SbPlayerPipeline<ChromeMedia>::DidLoadingProgress() const {
  base::AutoLock auto_lock(lock_);
  bool ret = did_loading_progress_;
  did_loading_progress_ = false;
  return ret;
}

template <typename ChromeMedia>
VideoStatistics SbPlayerPipeline<ChromeMedia>::GetVideoStatistics() const {
  base::AutoLock auto_lock(lock_);
  return video_statistics_;
}

template <typename ChromeMedia>
SetBoundsCB SbPlayerPipeline<ChromeMedia>::GetSetBoundsCB() {
  return base::Bind(&SbPlayerSetBoundsHelper::SetBounds, set_bounds_helper_);
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::SetPreferredOutputModeToDecodeToTexture() {
  TRACE_EVENT0(
      "cobalt::media",
      "SbPlayerPipeline<ChromeMedia>::SetPreferredOutputModeToDecodeToTexture");

  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SbPlayerPipeline<
                       ChromeMedia>::SetPreferredOutputModeToDecodeToTexture,
                   this));
    return;
  }

  // The player can't be created yet, if it is, then we're updating the output
  // mode too late.
  DCHECK(!player_bridge_);

  default_output_mode_ = kSbPlayerOutputModeDecodeToTexture;
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::StartTask(StartTaskParameters parameters) {
  TRACE_EVENT0("cobalt::media", "SbPlayerPipeline<ChromeMedia>::StartTask");

  DCHECK(task_runner_->RunsTasksInCurrentSequence());

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
  max_video_input_size_ = parameters.max_video_input_size;
#if SB_HAS(PLAYER_WITH_URL)
  is_url_based_ = parameters.is_url_based;
  if (is_url_based_) {
    CreateUrlPlayer(parameters.source_url);
    return;
  }
#endif  // SB_HAS(PLAYER_WITH_URL)
  demuxer_->Initialize(
      this, BindPostTaskToCurrentDefault(base::Bind(
                &SbPlayerPipeline<ChromeMedia>::OnDemuxerInitialized, this)));

  started_ = true;
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::SetVolumeTask(float volume) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (player_bridge_) {
    player_bridge_->SetVolume(volume_);
  }
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::SetPlaybackRateTask(float volume) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (player_bridge_) {
    player_bridge_->SetPlaybackRate(playback_rate_);
  }
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::SetDurationTask(TimeDelta duration) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!duration_change_cb_.is_null()) {
    duration_change_cb_.Run();
  }
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::OnBufferedTimeRangesChanged(
    const Ranges& ranges) {
  base::AutoLock auto_lock(lock_);
  did_loading_progress_ = true;
  buffered_time_ranges_ = ranges;
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::SetDuration(TimeDelta duration) {
  base::AutoLock auto_lock(lock_);
  duration_ = duration;
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&SbPlayerPipeline<ChromeMedia>::SetDurationTask,
                            this, duration));
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::OnDemuxerError(PipelineStatus error) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&SbPlayerPipeline<ChromeMedia>::OnDemuxerError,
                              this, error));
    return;
  }

  LOG(INFO)
      << "SbPlayerPipeline<ChromeMedia>::OnDemuxerError() called with error "
      << error;

  if (error != pipeline_status_cast<PipelineStatus>(::media::PIPELINE_OK)) {
    CallErrorCB(error, "Demuxer error.");
  }
}

#if SB_HAS(PLAYER_WITH_URL)
template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::CreateUrlPlayer(
    const std::string& source_url) {
  TRACE_EVENT0("cobalt::media",
               "SbPlayerPipeline<ChromeMedia>::CreateUrlPlayer");
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (stopped_) {
    return;
  }

  if (suspended_) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&SbPlayerPipeline<ChromeMedia>::CreateUrlPlayer, this,
                   source_url),
        TimeDelta::FromMilliseconds(kRetryDelayAtSuspendInMilliseconds));
    return;
  }

  // TODO:  Check |suspended_| here as the pipeline can be suspended before the
  // player is created.  In this case we should delay creating the player as the
  // creation of player may fail.
  std::string error_message;
  {
    base::AutoLock auto_lock(lock_);
    LOG(INFO) << "Creating SbPlayerBridge with url: " << source_url;
    player_bridge_.reset(new SbPlayerBridge(
        sbplayer_interface_, task_runner_, source_url, window_, this,
        set_bounds_helper_.get(), allow_resume_after_suspend_,
        default_output_mode_, on_encrypted_media_init_data_encountered_cb_,
        decode_target_provider_, pipeline_identifier_));
    if (player_bridge_->IsValid()) {
      SetPlaybackRateTask(playback_rate_);
      SetVolumeTask(volume_);
    } else {
      error_message = player_bridge_->GetPlayerCreationErrorMessage();
      player_bridge_.reset();
      LOG(INFO) << "Failed to create a valid SbPlayerBridge.";
    }
  }

  if (player_bridge_ && player_bridge_->IsValid()) {
    base::Closure output_mode_change_cb;
    {
      base::AutoLock auto_lock(lock_);
      DCHECK(!output_mode_change_cb_.is_null());
      output_mode_change_cb = std::move(output_mode_change_cb_);
    }
    output_mode_change_cb.Run();
    return;
  }

  std::string time_information = GetTimeInformation();
  LOG(INFO) << "SbPlayerPipeline<ChromeMedia>::CreateUrlPlayer failed to "
               "create a valid "
               "SbPlayerBridge - "
            << time_information << " \'" << error_message << "\'";

  CallSeekCB(
      pipeline_status_cast<PipelineStatus>(
          ::media::DECODER_ERROR_NOT_SUPPORTED),
      "SbPlayerPipeline<ChromeMedia>::CreateUrlPlayer failed to create a valid "
      "SbPlayerBridge - " +
          time_information + " \'" + error_message + "\'");
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::SetDrmSystem(SbDrmSystem drm_system) {
  TRACE_EVENT0("cobalt::media", "SbPlayerPipeline<ChromeMedia>::SetDrmSystem");

  base::AutoLock auto_lock(lock_);
  if (!player_bridge_) {
    LOG(INFO) << "Player not set before calling "
                 "SbPlayerPipeline<ChromeMedia>::SetDrmSystem";
    return;
  }

  if (player_bridge_->IsValid()) {
    player_bridge_->RecordSetDrmSystemReadyTime(set_drm_system_ready_cb_time_);
    player_bridge_->SetDrmSystem(drm_system);
  }
}
#endif  // SB_HAS(PLAYER_WITH_URL)

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::CreatePlayer(SbDrmSystem drm_system) {
#if SB_HAS(PLAYER_WITH_URL)
  DCHECK(!is_url_based_);
#endif  // SB_HAS(PLAYER_WITH_URL
  TRACE_EVENT0("cobalt::media", "SbPlayerPipeline<ChromeMedia>::CreatePlayer");

  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(audio_stream_ || video_stream_);

  if (stopped_) {
    return;
  }

  if (suspended_) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&SbPlayerPipeline<ChromeMedia>::CreatePlayer, this,
                   drm_system),
        base::Milliseconds(kRetryDelayAtSuspendInMilliseconds));
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

  std::string audio_mime_type = audio_stream_ ? audio_stream_->mime_type() : "";
  std::string video_mime_type;
  if (video_stream_) {
    playback_statistics_.UpdateVideoConfig(
        video_stream_->video_decoder_config());
    video_mime_type = video_stream_->mime_type();
  }

  std::string error_message;
  {
    base::AutoLock auto_lock(lock_);
    SB_DCHECK(!player_bridge_);
    // In the extreme case that CreatePlayer() is called when a |player_bridge_|
    // is available, reset the existing player first to reduce the number of
    // active players.
    player_bridge_.reset();
    LOG(INFO) << "Creating SbPlayerBridge.";
    player_bridge_.reset(new SbPlayerBridge<ChromeMedia>(
        sbplayer_interface_, task_runner_,
        get_decode_target_graphics_context_provider_func_, audio_config,
        audio_mime_type, video_config, video_mime_type, window_, drm_system,
        this, set_bounds_helper_.get(), allow_resume_after_suspend_,
        default_output_mode_, decode_target_provider_, max_video_capabilities_,
        max_video_input_size_, pipeline_identifier_));
    if (player_bridge_->IsValid()) {
      // TODO(b/267678497): When `player_bridge_->GetAudioConfigurations()`
      // returns no audio configurations, update the write durations again
      // before the SbPlayer reaches `kSbPlayerStatePresenting`.
      audio_write_duration_for_preroll_ = audio_write_duration_ =
          HasRemoteAudioOutputs(player_bridge_->GetAudioConfigurations())
              ? audio_write_duration_remote_
              : audio_write_duration_local_;
      LOG(INFO) << "SbPlayerBridge created, with audio write duration at "
                << audio_write_duration_for_preroll_;

      SetPlaybackRateTask(playback_rate_);
      SetVolumeTask(volume_);
    } else {
      error_message = player_bridge_->GetPlayerCreationErrorMessage();
      player_bridge_.reset();
      LOG(INFO) << "Failed to create a valid SbPlayerBridge.";
    }
  }

  if (player_bridge_ && player_bridge_->IsValid()) {
    player_bridge_->RecordSetDrmSystemReadyTime(set_drm_system_ready_cb_time_);
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

  std::string time_information = GetTimeInformation();
  LOG(INFO)
      << "SbPlayerPipeline<ChromeMedia>::CreatePlayer failed to create a valid "
         "SbPlayerBridge - "
      << time_information << " \'" << error_message << "\'";

  CallSeekCB(
      pipeline_status_cast<PipelineStatus>(
          ::media::DECODER_ERROR_NOT_SUPPORTED),
      "SbPlayerPipeline<ChromeMedia>::CreatePlayer failed to create a valid "
      "SbPlayerBridge - " +
          time_information + " \'" + error_message + "\'");
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::OnDemuxerInitialized(
    PipelineStatus status) {
#if SB_HAS(PLAYER_WITH_URL)
  DCHECK(!is_url_based_);
#endif  // SB_HAS(PLAYER_WITH_URL)
  TRACE_EVENT0("cobalt::media",
               "SbPlayerPipeline<ChromeMedia>::OnDemuxerInitialized");

  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (stopped_) {
    return;
  }

  if (status != pipeline_status_cast<PipelineStatus>(::media::PIPELINE_OK)) {
    CallErrorCB(status, "Demuxer initialization error.");
    return;
  }

  if (suspended_) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&SbPlayerPipeline<ChromeMedia>::OnDemuxerInitialized, this,
                   status),
        base::Milliseconds(kRetryDelayAtSuspendInMilliseconds));
    return;
  }

  auto streams = demuxer_->GetAllStreams();
  DemuxerStream* audio_stream = nullptr;
  DemuxerStream* video_stream = nullptr;
  for (auto&& stream : streams) {
    if (stream->type() == DemuxerStream::AUDIO) {
      if (audio_stream == nullptr) {
        audio_stream = stream;
      } else {
        LOG(WARNING) << "Encountered more than one audio streams.";
      }
    } else if (stream->type() == DemuxerStream::VIDEO) {
      if (video_stream == nullptr) {
        video_stream = stream;
      } else {
        LOG(WARNING) << "Encountered more than one video streams.";
      }
    }
  }

  if (audio_stream == NULL && video_stream == NULL) {
    LOG(INFO) << "The video has to contain an audio track or a video track.";
    CallErrorCB(pipeline_status_cast<PipelineStatus>(
                    ::media::DEMUXER_ERROR_NO_SUPPORTED_STREAMS),
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
      RunSetDrmSystemReadyCB(BindPostTaskToCurrentDefault(
          base::Bind(&SbPlayerPipeline<ChromeMedia>::CreatePlayer, this)));
      return;
    }
  }

  CreatePlayer(kSbDrmSystemInvalid);
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::OnDemuxerSeeked(PipelineStatus status) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (status == pipeline_status_cast<PipelineStatus>(::media::PIPELINE_OK) &&
      player_bridge_) {
    player_bridge_->Seek(seek_time_);
  }
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::OnDemuxerStopped() {
  TRACE_EVENT0("cobalt::media",
               "SbPlayerPipeline<ChromeMedia>::OnDemuxerStopped");

  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SbPlayerPipeline<ChromeMedia>::OnDemuxerStopped, this));
    return;
  }

  std::move(stop_cb_).Run();
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::OnDemuxerStreamRead(
    typename DemuxerStream::Type type, int max_number_buffers_to_read,
    typename DemuxerStream::Status status,
    std::vector<scoped_refptr<DecoderBuffer>> buffers) {
#if SB_HAS(PLAYER_WITH_URL)
  DCHECK(!is_url_based_);
#endif  // SB_HAS(PLAYER_WITH_URL)
  DCHECK(type == DemuxerStream::AUDIO || type == DemuxerStream::VIDEO)
      << "Unsupported DemuxerStream::Type " << type;

  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SbPlayerPipeline<ChromeMedia>::OnDemuxerStreamRead,
                       this, type, max_number_buffers_to_read, status,
                       std::move(buffers)));
    return;
  }

  if (stopped_) {
    return;
  }

  DCHECK(player_bridge_);

  DemuxerStream* stream =
      type == DemuxerStream::AUDIO ? audio_stream_ : video_stream_;
  DCHECK(stream);

  if (!player_bridge_ || !stream) {
    return;
  }

  if (status == DemuxerStream::kAborted) {
    DCHECK(GetReadInProgress(type));
    SetReadInProgress(type, false);

    if (!seek_cb_.is_null()) {
      CallSeekCB(pipeline_status_cast<PipelineStatus>(::media::PIPELINE_OK),
                 "");
    }
    return;
  }

  if (status == DemuxerStream::kConfigChanged) {
    UpdateDecoderConfig(stream);
    stream->Read(
        max_number_buffers_to_read,
        base::BindOnce(&SbPlayerPipeline<ChromeMedia>::OnDemuxerStreamRead,
                       this, type, max_number_buffers_to_read));
    return;
  }

  if (type == DemuxerStream::AUDIO) {
    for (const auto& buffer : buffers) {
      playback_statistics_.OnAudioAU(buffer);
      if (!buffer->end_of_stream()) {
        last_audio_sample_interval_ =
            buffer->timestamp() - timestamp_of_last_written_audio_;
        timestamp_of_last_written_audio_ = buffer->timestamp();
      }
    }
  } else {
    for (const auto& buffer : buffers) {
      playback_statistics_.OnVideoAU(buffer);
      if (buffer->end_of_stream()) {
        is_video_eos_written_ = true;
      }
    }
  }
  SetReadInProgress(type, false);

  player_bridge_->WriteBuffers(type, buffers);
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::OnNeedData(
    typename DemuxerStream::Type type, int max_number_of_buffers_to_write) {
#if SB_HAS(PLAYER_WITH_URL)
  DCHECK(!is_url_based_);
#endif  // SB_HAS(PLAYER_WITH_URL)
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // In case if Stop() has been called.
  if (!player_bridge_) {
    return;
  }

  int max_buffers = max_audio_samples_per_write_ > 1
                        ? std::min(max_number_of_buffers_to_write,
                                   max_audio_samples_per_write_)
                        : 1;

  if (GetReadInProgress(type)) return;

  if (type == DemuxerStream::AUDIO) {
    if (!audio_stream_) {
      LOG(WARNING)
          << "Calling OnNeedData() for audio data during audioless playback";
      return;
    }

    // If we haven't checked the media time recently, update it now.
    if (Time::Now() - last_time_media_time_retrieved_ >
        kMediaTimeCheckInterval) {
      GetMediaTime();
    }

    // Delay reading audio more than |audio_write_duration_| ahead of playback
    // after the player has received enough audio for preroll, taking into
    // account that our estimate of playback time might be behind by
    // |kMediaTimeCheckInterval|.
    TimeDelta time_ahead_of_playback_for_preroll =
        timestamp_of_last_written_audio_ - seek_time_;
    auto adjusted_write_duration_for_preroll =
        AdjustWriteDurationForPlaybackRate(audio_write_duration_for_preroll_,
                                           playback_rate_);
    // Note when Cobalt uses multiple samples per write, GetDefaultMaxBuffers()
    // returns the exact number of samples computed by
    // |time_ahead_of_playback_for_preroll| and
    // |adjusted_write_duration_for_preroll|. The guard number
    // kPrerollGuardAudioBuffer is used to ensure Cobalt can do one initial
    // write for audio preroll, as preroll condition requires that
    // |time_ahead_of_playback_for_preroll| >
    // |adjusted_write_duration_for_preroll|.
    int estimated_max_buffers = max_buffers;
    if (!is_video_eos_written_ && time_ahead_of_playback_for_preroll >
                                      adjusted_write_duration_for_preroll) {
      // The estimated time ahead of playback may be negative if no audio has
      // been written.
      TimeDelta time_ahead_of_playback =
          timestamp_of_last_written_audio_ - last_media_time_;
      auto adjusted_write_duration = AdjustWriteDurationForPlaybackRate(
          audio_write_duration_, playback_rate_);
      if (time_ahead_of_playback >
          (adjusted_write_duration + kMediaTimeCheckInterval)) {
        task_runner_->PostDelayedTask(
            FROM_HERE,
            base::Bind(&SbPlayerPipeline<ChromeMedia>::DelayedNeedData, this,
                       max_buffers),
            kMediaTimeCheckInterval);
        audio_read_delayed_ = true;
        return;
      }
      if (max_audio_samples_per_write_ > 1 &&
          !time_ahead_of_playback.is_negative()) {
        estimated_max_buffers = GetEstimatedMaxBuffers(adjusted_write_duration,
                                                       time_ahead_of_playback,
                                                       false /* is_preroll */);
      }
    } else if (max_audio_samples_per_write_ > 1) {
      if (!time_ahead_of_playback_for_preroll.is_negative()) {
        estimated_max_buffers = GetEstimatedMaxBuffers(
            adjusted_write_duration_for_preroll,
            time_ahead_of_playback_for_preroll, true /* is_preroll */);
        last_estimated_max_buffers_for_preroll_ = std::max(
            estimated_max_buffers, last_estimated_max_buffers_for_preroll_);
      } else {
        estimated_max_buffers = last_estimated_max_buffers_for_preroll_;
      }
    }
    // When Cobalt uses multiple samples per write, this ensures that
    // |max_buffers| is at most |max_number_of_buffers_to_write|.
    // |max_buffers| is in the range of [1, |max_number_of_buffers_to_write|],
    // where the lower bound 1 is guarded by GetEstimatedMaxBuffers().
    max_buffers = std::min(max_buffers, estimated_max_buffers);

    audio_read_delayed_ = false;
    audio_read_in_progress_ = true;
  } else {
    DCHECK_EQ(type, DemuxerStream::VIDEO);
    video_read_in_progress_ = true;
  }
  DemuxerStream* stream =
      type == DemuxerStream::AUDIO ? audio_stream_ : video_stream_;
  DCHECK(stream);

  stream->Read(
      max_buffers,
      base::BindOnce(&SbPlayerPipeline<ChromeMedia>::OnDemuxerStreamRead, this,
                     type, max_buffers));
}

template <typename ChromeMedia>
int SbPlayerPipeline<ChromeMedia>::GetDefaultMaxBuffers(
    AudioCodec codec, TimeDelta duration_to_write, bool is_preroll) {
  // Return default maximum samples per write to speed up the initial sample
  // write, including guard number of samples per write for audio preroll.
  // The guard number kPrerollGuardAudioBuffer is used to ensure Cobalt
  // can do one initial write for audio preroll.
  int default_max_buffers = static_cast<int>(
      std::ceil(duration_to_write.InSecondsF() *
                audio_stream_->audio_decoder_config().samples_per_second() /
                GetDefaultAudioFramesPerBuffer<ChromeMedia>(codec)));
  if (is_preroll) {
    default_max_buffers += kPrerollGuardAudioBuffer;
  }
  DCHECK_GT(default_max_buffers, 0);
  return default_max_buffers;
}

template <typename ChromeMedia>
int SbPlayerPipeline<ChromeMedia>::GetEstimatedMaxBuffers(
    TimeDelta write_duration, TimeDelta time_ahead_of_playback,
    bool is_preroll) {
  DCHECK_GE(time_ahead_of_playback.InMicroseconds(), 0);

  int estimated_max_buffers = 1;
  if (!(max_audio_samples_per_write_ > 1) ||
      write_duration <= time_ahead_of_playback) {
    return estimated_max_buffers;
  }

  TimeDelta duration_to_write = write_duration - time_ahead_of_playback;
  DCHECK_GT(duration_to_write.InMicroseconds(), 0);
  switch (audio_stream_->audio_decoder_config().codec()) {
    case AudioCodec::kOpus:
    case AudioCodec::kAAC:
    case AudioCodec::kAC3:
    case AudioCodec::kEAC3:
      if (last_audio_sample_interval_.is_zero()) {
        estimated_max_buffers =
            GetDefaultMaxBuffers(audio_stream_->audio_decoder_config().codec(),
                                 duration_to_write, is_preroll);
        break;
      }
    // TODO(b/41486346): Support multiple samples per write on the format IAMF.
    // case AudioCodec::kIAMF:
    default:
      if (!last_audio_sample_interval_.is_zero()) {
        DCHECK_GT(last_audio_sample_interval_.InMicroseconds(), 0);
        estimated_max_buffers =
            duration_to_write.InMillisecondsRoundedUp() /
                last_audio_sample_interval_.InMilliseconds() +
            1;
      }
  }
  DCHECK_GT(estimated_max_buffers, 0);
  // Return 1 if |estimated_max_buffers| is non-positive. This ensures
  // in corner cases, |estimated_max_buffers| falls back to 1.
  // The maximum number samples of write should be guarded by
  // SbPlayerGetMaximumNumberOfSamplesPerWrite() in OnNeedData().
  return estimated_max_buffers > 0 ? estimated_max_buffers : 1;
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::OnPlayerStatus(SbPlayerState state) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // In case if Stop() has been called.
  if (!player_bridge_) {
    return;
  }
  player_state_ = state;
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
      buffering_state_cb_.Run(BufferingState::kHaveMetadata);
      break;
    case kSbPlayerStatePresenting: {
#if SB_HAS(PLAYER_WITH_URL)
      if (is_url_based_) {
        duration_ = player_bridge_->GetDuration();
        start_date_ = player_bridge_->GetStartDate();
        buffering_state_cb_.Run(BufferingState::kHaveMetadata);
        int frame_width;
        int frame_height;
        player_bridge_->GetVideoResolution(&frame_width, &frame_height);
        bool natural_size_changed = (frame_width != natural_size_.width() ||
                                     frame_height != natural_size_.height());
        natural_size_ = gfx::Size(frame_width, frame_height);
        if (natural_size_changed) {
          content_size_change_cb_.Run();
        }
      }
#endif  // SB_HAS(PLAYER_WITH_URL)
      buffering_state_cb_.Run(BufferingState::kPrerollCompleted);
      if (!seek_cb_.is_null()) {
        CallSeekCB(pipeline_status_cast<PipelineStatus>(::media::PIPELINE_OK),
                   "");
      }
      if (video_stream_) {
        playback_statistics_.OnPresenting(
            video_stream_->video_decoder_config());
      }

#if SB_HAS(PLAYER_WITH_URL)
      // Url based player does not support |audio_write_duration_for_preroll_|.
      if (is_url_based_) {
        break;
      }
#endif  // SB_HAS(PLAYER_WITH_URL)

      audio_write_duration_for_preroll_ = audio_write_duration_ =
          HasRemoteAudioOutputs(player_bridge_->GetAudioConfigurations())
              ? audio_write_duration_remote_
              : audio_write_duration_local_;
      LOG(INFO) << "SbPlayerBridge reaches kSbPlayerStatePresenting, with audio"
                << " write duration at " << audio_write_duration_;
      break;
    }
    case kSbPlayerStateEndOfStream:
      ended_cb_.Run(pipeline_status_cast<PipelineStatus>(::media::PIPELINE_OK));
      ended_ = true;
      break;
    case kSbPlayerStateDestroyed:
      break;
  }
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::OnPlayerError(SbPlayerError error,
                                                  const std::string& message) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // In case if Stop() has been called.
  if (!player_bridge_) {
    return;
  }

#if SB_HAS(PLAYER_WITH_URL)
  if (error >= kSbPlayerErrorMax) {
    DCHECK(is_url_based_);
    switch (static_cast<SbUrlPlayerError>(error)) {
      case kSbUrlPlayerErrorNetwork:
        CallErrorCB(pipeline_status_cast<PipelineStatus>(
                        ::media::PIPELINE_ERROR_NETWORK),
                    message);
        break;
      case kSbUrlPlayerErrorSrcNotSupported:
        CallErrorCB(pipeline_status_cast<PipelineStatus>(
                        ::media::DEMUXER_ERROR_COULD_NOT_OPEN),
                    message);

        break;
    }
    return;
  }
#endif  // SB_HAS(PLAYER_WITH_URL)
  switch (error) {
    case kSbPlayerErrorDecode:
      CallErrorCB(
          pipeline_status_cast<PipelineStatus>(::media::PIPELINE_ERROR_DECODE),
          message);
      break;
    case kSbPlayerErrorCapabilityChanged:
      CallErrorCB(
          pipeline_status_cast<PipelineStatus>(::media::PIPELINE_ERROR_DECODE),
          message.empty()
              ? kSbPlayerCapabilityChangedErrorMessage
              : ::starboard::FormatString(
                    "%s: %s", kSbPlayerCapabilityChangedErrorMessage,
                    message.c_str()));
      break;
    case kSbPlayerErrorMax:
      NOTREACHED();
      break;
  }
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::DelayedNeedData(
    int max_number_of_buffers_to_write) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (audio_read_delayed_) {
    OnNeedData(DemuxerStream::AUDIO, max_number_of_buffers_to_write);
  }
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::UpdateDecoderConfig(DemuxerStream* stream) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!player_bridge_) {
    return;
  }

  if (stream->type() == DemuxerStream::AUDIO) {
    const AudioDecoderConfig& decoder_config = stream->audio_decoder_config();
    media_metrics_provider_->SetHasAudio(
        static_cast<::media_m114::AudioCodec>(decoder_config.codec()));
    player_bridge_->UpdateAudioConfig(decoder_config, stream->mime_type());
  } else {
    DCHECK_EQ(stream->type(), DemuxerStream::VIDEO);
    const VideoDecoderConfig& decoder_config = stream->video_decoder_config();
    media_metrics_provider_->SetHasVideo(
        static_cast<::media_m114::VideoCodec>(decoder_config.codec()));
    base::AutoLock auto_lock(lock_);
    bool natural_size_changed =
        (decoder_config.natural_size().width() != natural_size_.width() ||
         decoder_config.natural_size().height() != natural_size_.height());
    natural_size_ = decoder_config.natural_size();
    player_bridge_->UpdateVideoConfig(decoder_config, stream->mime_type());
    if (natural_size_changed) {
      content_size_change_cb_.Run();
    }

    playback_statistics_.UpdateVideoConfig(stream->video_decoder_config());
  }
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::CallSeekCB(
    PipelineStatus status, const std::string& error_message) {
  if (status == pipeline_status_cast<PipelineStatus>(::media::PIPELINE_OK)) {
    DCHECK(error_message.empty());
  }

  SeekCB seek_cb;
  bool is_initial_preroll;
  {
    base::AutoLock auto_lock(lock_);
    DCHECK(!seek_cb_.is_null());
    seek_cb = std::move(seek_cb_);
    is_initial_preroll = is_initial_preroll_;
    is_initial_preroll_ = false;
  }
  seek_cb.Run(status, is_initial_preroll,
              AppendStatisticsString(error_message));
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::CallErrorCB(
    PipelineStatus status, const std::string& error_message) {
  DCHECK_NE(status, pipeline_status_cast<PipelineStatus>(::media::PIPELINE_OK));
  // Only to record the first error.
  if (error_cb_.is_null()) {
    return;
  }
  playback_statistics_.OnError(
      pipeline_status_cast<::media_m114::PipelineStatus>(status),
      error_message);
  ResetAndRunIfNotNull(&error_cb_, status,
                       AppendStatisticsString(error_message));
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::SuspendTask(
    base::WaitableEvent* done_event) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(done_event);
  DCHECK(!suspended_);

  if (suspended_) {
    done_event->Signal();
    return;
  }

  if (player_bridge_) {
    // Cancel pending delayed calls to OnNeedData. After
    // player_bridge_->Resume(), |player_bridge_| will call OnNeedData again.
    audio_read_delayed_ = false;
    player_bridge_->Suspend();
  }

  suspended_ = true;

  done_event->Signal();
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::ResumeTask(
    PipelineWindow window, base::WaitableEvent* done_event) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(done_event);
  DCHECK(suspended_);

  if (!suspended_) {
    last_resume_time_ = Time::Now();
    done_event->Signal();
    return;
  }

  window_ = window;

  bool resumable = true;
  bool resume_to_background_mode = !SbWindowIsValid(window_);
  bool is_audioless = !HasAudio();
  if (resume_to_background_mode && is_audioless) {
    // Avoid resuming an audioless video to background mode. SbPlayerBridge will
    // try to create an SbPlayer with only the video stream disabled, and may
    // crash in this case as SbPlayerCreate() will fail without an audio or
    // video stream.
    resumable = false;
  }
  if (player_bridge_ && resumable) {
    player_bridge_->Resume(window);
    if (!player_bridge_->IsValid()) {
      std::string error_message;
      {
        base::AutoLock auto_lock(lock_);
        error_message = player_bridge_->GetPlayerCreationErrorMessage();
        player_bridge_.reset();
      }
      std::string time_information = GetTimeInformation();
      LOG(INFO) << "SbPlayerPipeline<ChromeMedia>::ResumeTask failed to create "
                   "a valid "
                   "SbPlayerBridge - "
                << time_information << " \'" << error_message << "\'";
      CallErrorCB(
          pipeline_status_cast<PipelineStatus>(
              ::media::DECODER_ERROR_NOT_SUPPORTED),
          "SbPlayerPipeline<ChromeMedia>::ResumeTask failed to create a valid "
          "SbPlayerBridge - " +
              time_information + " \'" + error_message + "\'");
    }
  }

  suspended_ = false;
  last_resume_time_ = Time::Now();

  done_event->Signal();
}

template <typename ChromeMedia>
std::string SbPlayerPipeline<ChromeMedia>::AppendStatisticsString(
    const std::string& message) const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (nullptr == video_stream_) {
    return message + ", playback statistics: n/a.";
  } else {
    return message + ", playback statistics: " +
           playback_statistics_.GetStatistics(
               video_stream_->video_decoder_config()) +
           '.';
  }
}

template <typename ChromeMedia>
std::string SbPlayerPipeline<ChromeMedia>::GetTimeInformation() const {
  auto round_time_in_seconds = [](const TimeDelta time) {
    const int64_t seconds = time.InSeconds();
    if (seconds < 15) {
      return seconds / 5 * 5;
    }
    if (seconds < 60) {
      return seconds / 15 * 15;
    }
    if (seconds < 3600) {
      return std::max(static_cast<int64_t>(60), seconds / 600 * 600);
    }
    return std::max(static_cast<int64_t>(3600), seconds / 18000 * 18000);
  };
  std::string time_since_start =
      std::to_string(round_time_in_seconds(base::StartupTimer::TimeElapsed())) +
      "s";
  std::string time_since_resume = !last_resume_time_.is_null()
                                      ? std::to_string(round_time_in_seconds(
                                            Time::Now() - last_resume_time_)) +
                                            "s"
                                      : "null";
  return "time since app start: " + time_since_start +
         ", time since last resume: " + time_since_resume;
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::RunSetDrmSystemReadyCB(
    DrmSystemReadyCB drm_system_ready_cb) {
  TRACE_EVENT0("cobalt::media",
               "SbPlayerPipeline<ChromeMedia>::RunSetDrmSystemReadyCB");
  set_drm_system_ready_cb_time_ = Time::Now();
  set_drm_system_ready_cb_.Run(drm_system_ready_cb);
}

template <typename ChromeMedia>
void SbPlayerPipeline<ChromeMedia>::SetReadInProgress(
    typename DemuxerStream::Type type, bool in_progress) {
  if (type == DemuxerStream::AUDIO)
    audio_read_in_progress_ = in_progress;
  else
    video_read_in_progress_ = in_progress;
}

template <typename ChromeMedia>
bool SbPlayerPipeline<ChromeMedia>::GetReadInProgress(
    typename DemuxerStream::Type type) const {
  if (type == DemuxerStream::AUDIO) return audio_read_in_progress_;
  return video_read_in_progress_;
}

}  // namespace media
}  // namespace cobalt
