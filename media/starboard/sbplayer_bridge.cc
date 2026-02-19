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

#include "media/starboard/sbplayer_bridge.h"

#include <algorithm>
#include <iomanip>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#if COBALT_MEDIA_ENABLE_STARTUP_LATENCY_TRACKING
#include "cobalt/base/statistics.h"
#endif  // COBALT_MEDIA_ENABLE_STARTUP_LATENCY_TRACKING
#if COBALT_MEDIA_ENABLE_FORMAT_SUPPORT_QUERY_METRICS
#include "cobalt/media/base/format_support_query_metrics.h"
#endif  // COBALT_MEDIA_ENABLE_FORMAT_SUPPORT_QUERY_METRICS
#include "media/starboard/starboard_utils.h"
#include "starboard/common/media.h"
#include "starboard/common/once.h"
#include "starboard/common/player.h"
#include "starboard/common/string.h"
#include "starboard/configuration.h"
#include "starboard/extension/player_configurate_seek.h"
#include "starboard/extension/video_decoder_configuration.h"
#if COBALT_MEDIA_ENABLE_PLAYER_SET_MAX_VIDEO_INPUT_SIZE
#include "starboard/extension/player_set_max_video_input_size.h"
#endif  // COBALT_MEDIA_ENABLE_PLAYER_SET_MAX_VIDEO_INPUT_SIZE
#include "starboard/extension/player_set_video_surface_view.h"

namespace media {

namespace {

using base::Time;
using base::TimeDelta;
using starboard::FormatString;
using starboard::GetPlayerOutputModeName;

#if COBALT_MEDIA_ENABLE_STARTUP_LATENCY_TRACKING
class StatisticsWrapper {
 public:
  static StatisticsWrapper* GetInstance();
  base::Statistics<int64_t, int, 1024> startup_latency{
      "Media.PlaybackStartupLatency"};
};
#endif  // COBALT_MEDIA_ENABLE_STARTUP_LATENCY_TRACKING

void SetStreamInfo(const SbMediaAudioStreamInfo& stream_info,
                   SbMediaAudioSampleInfo* sample_info) {
  DCHECK(sample_info);

  sample_info->stream_info = stream_info;
}

void SetStreamInfo(const SbMediaVideoStreamInfo& stream_info,
                   SbMediaVideoSampleInfo* sample_info) {
  DCHECK(sample_info);
  sample_info->stream_info = stream_info;
}

void SetDiscardPadding(
    const ::media::DecoderBuffer::DiscardPadding& discard_padding,
    SbMediaAudioSampleInfo* sample_info) {
  DCHECK(sample_info);

  sample_info->discarded_duration_from_front =
      discard_padding.first.InMicroseconds();
  sample_info->discarded_duration_from_back =
      discard_padding.second.InMicroseconds();
}

}  // namespace

#if COBALT_MEDIA_ENABLE_STARTUP_LATENCY_TRACKING
SB_ONCE_INITIALIZE_FUNCTION(StatisticsWrapper, StatisticsWrapper::GetInstance);
#endif  // COBALT_MEDIA_ENABLE_STARTUP_LATENCY_TRACKING

SbPlayerBridge::CallbackHelper::CallbackHelper(SbPlayerBridge* player_bridge)
    : player_bridge_(player_bridge) {}

void SbPlayerBridge::CallbackHelper::ClearDecoderBufferCache() {
  if (!player_bridge_) {
    return;
  }
  CHECK(player_bridge_->task_runner_->RunsTasksInCurrentSequence());

  player_bridge_->ClearDecoderBufferCache();
}

void SbPlayerBridge::CallbackHelper::OnDecoderStatus(void* player,
                                                     SbMediaType type,
                                                     SbPlayerDecoderState state,
                                                     int ticket) {
  if (!player_bridge_) {
    return;
  }
  CHECK(player_bridge_->task_runner_->RunsTasksInCurrentSequence());

  player_bridge_->OnDecoderStatus(static_cast<SbPlayer>(player), type, state,
                                  ticket);
}

void SbPlayerBridge::CallbackHelper::OnPlayerStatus(void* player,
                                                    SbPlayerState state,
                                                    int ticket) {
  if (!player_bridge_) {
    return;
  }
  CHECK(player_bridge_->task_runner_->RunsTasksInCurrentSequence());

  player_bridge_->OnPlayerStatus(static_cast<SbPlayer>(player), state, ticket);
}

void SbPlayerBridge::CallbackHelper::OnPlayerError(void* player,
                                                   SbPlayerError error,
                                                   const std::string& message) {
  if (!player_bridge_) {
    return;
  }
  CHECK(player_bridge_->task_runner_->RunsTasksInCurrentSequence());

  player_bridge_->OnPlayerError(static_cast<SbPlayer>(player), error, message);
}

void SbPlayerBridge::CallbackHelper::OnDeallocateSample(
    const void* sample_buffer) {
  if (!player_bridge_) {
    return;
  }
  CHECK(player_bridge_->task_runner_->RunsTasksInCurrentSequence());

  player_bridge_->OnDeallocateSample(sample_buffer);
}

void SbPlayerBridge::CallbackHelper::ResetPlayer() {
  if (!player_bridge_) {
    return;
  }
  CHECK(player_bridge_->task_runner_->RunsTasksInCurrentSequence());

  player_bridge_ = nullptr;
}

#if SB_HAS(PLAYER_WITH_URL)
SbPlayerBridge::SbPlayerBridge(
    SbPlayerInterface* interface,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const std::string& url,
    SbWindow window,
    Host* host,
    SbPlayerSetBoundsHelper* set_bounds_helper,
    bool allow_resume_after_suspend,
    SbPlayerOutputMode default_output_mode,
    const OnEncryptedMediaInitDataEncounteredCB&
        on_encrypted_media_init_data_encountered_cb,
    DecodeTargetProvider* const decode_target_provider,
    std::string pipeline_identifier)
    : url_(url),
      sbplayer_interface_(interface),
      task_runner_(task_runner),
      callback_helper_(
          new CallbackHelper(ALLOW_THIS_IN_INITIALIZER_LIST(this))),
      window_(window),
      host_(host),
      set_bounds_helper_(set_bounds_helper),
      allow_resume_after_suspend_(allow_resume_after_suspend),
      on_encrypted_media_init_data_encountered_cb_(
          on_encrypted_media_init_data_encountered_cb),
      decode_target_provider_(decode_target_provider),
      cval_stats_(&interface->cval_stats_),
      pipeline_identifier_(pipeline_identifier),
      is_url_based_(true) {
  DCHECK(host_);
  DCHECK(set_bounds_helper_);

  output_mode_ = ComputeSbUrlPlayerOutputMode(default_output_mode);

  CreateUrlPlayer(url_);

#if COBALT_MEDIA_ENABLE_SUSPEND_RESUME
  task_runner->PostTask(
      FROM_HERE, base::BindRepeatedly(
                     &SbPlayerBridge::CallbackHelper::ClearDecoderBufferCache,
                     callback_helper_));
#endif  // COBALT_MEDIA_ENABLE_SUSPEND_RESUME
}
#endif  // SB_HAS(PLAYER_WITH_URL)

SbPlayerBridge::SbPlayerBridge(
    SbPlayerInterface* interface,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const GetDecodeTargetGraphicsContextProviderFunc&
        get_decode_target_graphics_context_provider_func,
    const AudioDecoderConfig& audio_config,
    const std::string& audio_mime_type,
    const VideoDecoderConfig& video_config,
    const std::string& video_mime_type,
    SbWindow window,
    SbDrmSystem drm_system,
    Host* host,
    SbPlayerSetBoundsHelper* set_bounds_helper,
    bool allow_resume_after_suspend,
    SbPlayerOutputMode default_output_mode,
#if COBALT_MEDIA_ENABLE_DECODE_TARGET_PROVIDER
    DecodeTargetProvider* const decode_target_provider,
#endif  // COBALT_MEDIA_ENABLE_DECODE_TARGET_PROVIDER
    const std::string& max_video_capabilities,
    int max_video_input_size,
    const ExperimentalFeatures& experimental_features
#if BUILDFLAG(IS_ANDROID)
    ,
    jobject surface_view
#endif  // BUILDFLAG(IS_ANDROID)
#if COBALT_MEDIA_ENABLE_CVAL
    ,
    std::string pipeline_identifier
#endif  // COBALT_MEDIA_ENABLE_CVAL
    )
    : sbplayer_interface_(interface),
      task_runner_(task_runner),
      get_decode_target_graphics_context_provider_func_(
          get_decode_target_graphics_context_provider_func),
      callback_helper_(new CallbackHelper(this)),
      window_(window),
      drm_system_(drm_system),
      host_(host),
      set_bounds_helper_(set_bounds_helper),
      allow_resume_after_suspend_(allow_resume_after_suspend),
      audio_config_(audio_config),
      video_config_(video_config),
#if COBALT_MEDIA_ENABLE_DECODE_TARGET_PROVIDER
      decode_target_provider_(decode_target_provider),
#endif  // COBALT_MEDIA_ENABLE_DECODE_TARGET_PROVIDER
      max_video_capabilities_(max_video_capabilities),
      max_video_input_size_(max_video_input_size),
      experimental_features_(experimental_features)
#if BUILDFLAG(IS_ANDROID)
      ,
      surface_view_(surface_view)
#endif  // BUILDFLAG(IS_ANDROID)
#if COBALT_MEDIA_ENABLE_CVAL
          cval_stats_(&interface->cval_stats_),
      pipeline_identifier_(pipeline_identifier),
#endif  // COBALT_MEDIA_ENABLE_CVAL
#if SB_HAS(PLAYER_WITH_URL)
      is_url_based_(false)
#endif  // SB_HAS(PLAYER_WITH_URL
{
#if COBALT_MEDIA_ENABLE_DECODE_TARGET_PROVIDER
  DCHECK(!get_decode_target_graphics_context_provider_func_.is_null());
#endif  // COBALT_MEDIA_ENABLE_DECODE_TARGET_PROVIDER
  DCHECK(audio_config.IsValidConfig() || video_config.IsValidConfig());
  DCHECK(host_);
  DCHECK(set_bounds_helper_);
#if COBALT_MEDIA_ENABLE_DECODE_TARGET_PROVIDER
  DCHECK(decode_target_provider_);
#endif  // COBALT_MEDIA_ENABLE_DECODE_TARGET_PROVIDER

  audio_stream_info_.codec = kSbMediaAudioCodecNone;
  video_stream_info_.codec = kSbMediaVideoCodecNone;

  if (audio_config.IsValidConfig()) {
    UpdateAudioConfig(audio_config, audio_mime_type);
  }
  if (video_config.IsValidConfig()) {
    UpdateVideoConfig(video_config, video_mime_type);
    SendColorSpaceHistogram();
  }

  output_mode_ = ComputeSbPlayerOutputMode(default_output_mode);

  CreatePlayer();

#if COBALT_MEDIA_ENABLE_SUSPEND_RESUME
  if (SbPlayerIsValid(player_)) {
    task_runner->PostTask(
        FROM_HERE, base::BindRepeatedly(
                       &SbPlayerBridge::CallbackHelper::ClearDecoderBufferCache,
                       callback_helper_));
  }
#endif  // COBALT_MEDIA_ENABLE_SUSPEND_RESUME
}

SbPlayerBridge::~SbPlayerBridge() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  callback_helper_->ResetPlayer();
  set_bounds_helper_->SetPlayerBridge(NULL);

#if COBALT_MEDIA_ENABLE_DECODE_TARGET_PROVIDER
  decode_target_provider_->SetOutputMode(
      DecodeTargetProvider::kOutputModeInvalid);
  decode_target_provider_->ResetGetCurrentSbDecodeTargetFunction();
#endif  // COBALT_MEDIA_ENABLE_DECODE_TARGET_PROVIDER

  if (SbPlayerIsValid(player_)) {
#if COBALT_MEDIA_ENABLE_CVAL
    cval_stats_->StartTimer(MediaTiming::SbPlayerDestroy, pipeline_identifier_);
#endif  // COBALT_MEDIA_ENABLE_CVAL
    sbplayer_interface_->Destroy(player_);
#if COBALT_MEDIA_ENABLE_CVAL
    cval_stats_->StopTimer(MediaTiming::SbPlayerDestroy, pipeline_identifier_);
#endif  // COBALT_MEDIA_ENABLE_CVAL
  }
}

void SbPlayerBridge::UpdateAudioConfig(const AudioDecoderConfig& audio_config,
                                       const std::string& mime_type) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(audio_config.IsValidConfig());

  LOG(INFO) << "Updated AudioDecoderConfig -- "
            << audio_config.AsHumanReadableString();

  audio_config_ = audio_config;
  audio_mime_type_ = mime_type;
  audio_stream_info_ = MediaAudioConfigToSbMediaAudioStreamInfo(
      audio_config_, audio_mime_type_.c_str());
  LOG(INFO) << "Converted to SbMediaAudioStreamInfo -- " << audio_stream_info_;
}

void SbPlayerBridge::UpdateVideoConfig(const VideoDecoderConfig& video_config,
                                       const std::string& mime_type) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(video_config.IsValidConfig());

  LOG(INFO) << "Updated VideoDecoderConfig -- "
            << video_config.AsHumanReadableString();

  video_config_ = video_config;
  video_stream_info_.frame_width =
      static_cast<int>(video_config_.natural_size().width());
  video_stream_info_.frame_height =
      static_cast<int>(video_config_.natural_size().height());
  video_stream_info_.codec =
      MediaVideoCodecToSbMediaVideoCodec(video_config_.codec());
  video_stream_info_.color_metadata =
      MediaToSbMediaColorMetadata(video_config_.color_space_info(),
                                  video_config_.hdr_metadata(), mime_type);
  video_mime_type_ = mime_type;
  video_stream_info_.mime = video_mime_type_.c_str();
  video_stream_info_.max_video_capabilities = max_video_capabilities_.c_str();
  LOG(INFO) << "Converted to SbMediaVideoStreamInfo -- " << video_stream_info_;
}

void SbPlayerBridge::WriteBuffers(
    DemuxerStream::Type type,
    const std::vector<scoped_refptr<DecoderBuffer>>& buffers) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
#if SB_HAS(PLAYER_WITH_URL)
  DCHECK(!is_url_based_);
#endif  // SB_HAS(PLAYER_WITH_URL)

#if COBALT_MEDIA_ENABLE_SUSPEND_RESUME
  if (allow_resume_after_suspend_) {
    if (type == DemuxerStream::Type::AUDIO) {
      decoder_buffer_cache_.AddBuffers(buffers, audio_stream_info_);
    } else {
      DCHECK(type == DemuxerStream::Type::VIDEO);
      decoder_buffer_cache_.AddBuffers(buffers, video_stream_info_);
    }
    if (state_ != kSuspended) {
      WriteNextBuffersFromCache(type, buffers.size());
    }
    return;
  }
#endif  // COBALT_MEDIA_ENABLE_SUSPEND_RESUME

  WriteBuffersInternal(type, buffers, &audio_stream_info_, &video_stream_info_);
}

void SbPlayerBridge::SetBounds(int z_index, const gfx::Rect& rect) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  set_bounds_z_index_ = z_index;
  set_bounds_rect_ = rect;

  if (state_ == kSuspended) {
    return;
  }

  UpdateBounds();
}

void SbPlayerBridge::PrepareForSeek() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  seek_pending_ = true;

  if (state_ == kSuspended) {
    return;
  }

  ++ticket_;
  sbplayer_interface_->SetPlaybackRate(player_, 0.f);
}

void SbPlayerBridge::Seek(TimeDelta time) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

#if COBALT_MEDIA_ENABLE_SUSPEND_RESUME
  decoder_buffer_cache_.ClearAll();
#endif  // COBALT_MEDIA_ENABLE_SUSPEND_RESUME
  seek_pending_ = false;

  pending_audio_eos_buffer_ = false;
  pending_video_eos_buffer_ = false;

  if (state_ == kSuspended) {
    preroll_timestamp_ = time;
    return;
  }

  // If a seek happens during resuming, the pipeline will write samples from the
  // seek target time again so resuming can be aborted.
  if (state_ == kResuming) {
    state_ = kPlaying;
  }

  DCHECK(SbPlayerIsValid(player_));

  ++ticket_;
  sbplayer_interface_->Seek(player_, time, ticket_);

  sbplayer_interface_->SetPlaybackRate(player_, playback_rate_);
}

void SbPlayerBridge::SetVolume(float volume) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  volume_ = volume;

  if (state_ == kSuspended) {
    return;
  }

  DCHECK(SbPlayerIsValid(player_));
  sbplayer_interface_->SetVolume(player_, volume);
}

void SbPlayerBridge::SetPlaybackRate(double playback_rate) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  playback_rate_ = playback_rate;

  if (state_ == kSuspended) {
    return;
  }

  if (seek_pending_) {
    return;
  }

  sbplayer_interface_->SetPlaybackRate(player_, playback_rate);
}

std::vector<SbMediaAudioConfiguration>
SbPlayerBridge::GetAudioConfigurations() {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!SbPlayerIsValid(player_)) {
    return std::vector<SbMediaAudioConfiguration>();
  }

  std::vector<SbMediaAudioConfiguration> configurations;

  // Set a limit to avoid infinite loop.
  constexpr int kMaxAudioConfigurations = 32;

  for (int i = 0; i < kMaxAudioConfigurations; ++i) {
    SbMediaAudioConfiguration configuration;
    if (!sbplayer_interface_->GetAudioConfiguration(player_, i,
                                                    &configuration)) {
      break;
    }

    configurations.push_back(configuration);
  }

  LOG_IF(WARNING, configurations.empty())
      << "Failed to find any audio configurations.";

  return configurations;
}

#if SB_HAS(PLAYER_WITH_URL)
void SbPlayerBridge::GetUrlPlayerBufferedTimeRanges(
    TimeDelta* buffer_start_time,
    TimeDelta* buffer_length_time) {
  DCHECK(buffer_start_time || buffer_length_time);
  DCHECK(is_url_based_);

  if (state_ == kSuspended) {
    *buffer_start_time = TimeDelta();
    *buffer_length_time = TimeDelta();
    return;
  }

  DCHECK(SbPlayerIsValid(player_));

  SbUrlPlayerExtraInfo url_player_info;
  sbplayer_interface_->GetUrlPlayerExtraInfo(player_, &url_player_info);

  if (buffer_start_time) {
    *buffer_start_time =
        TimeDelta::FromMicroseconds(url_player_info.buffer_start_timestamp);
  }
  if (buffer_length_time) {
    *buffer_length_time =
        TimeDelta::FromMicroseconds(url_player_info.buffer_duration);
  }
}

void SbPlayerBridge::GetVideoResolution(int* frame_width, int* frame_height) {
  DCHECK(frame_width);
  DCHECK(frame_height);
  DCHECK(is_url_based_);

  if (state_ == kSuspended) {
    *frame_width = video_stream_info_.frame_width;
    *frame_height = video_stream_info_.frame_height;
    return;
  }

  DCHECK(SbPlayerIsValid(player_));

  SbPlayerInfo out_player_info;
  sbplayer_interface_->GetInfo(player_, &out_player_info);

  video_stream_info_.frame_width = out_player_info.frame_width;
  video_stream_info_.frame_height = out_player_info.frame_height;

  *frame_width = video_stream_info_.frame_width;
  *frame_height = video_stream_info_.frame_height;
}

TimeDelta SbPlayerBridge::GetDuration() {
  DCHECK(is_url_based_);

  if (state_ == kSuspended) {
    return TimeDelta();
  }

  DCHECK(SbPlayerIsValid(player_));

  SbPlayerInfo info;
  sbplayer_interface_->GetInfo(player_, &info);
  if (info.duration == SB_PLAYER_NO_DURATION) {
    // URL-based player may not have loaded asset yet, so map no duration to 0.
    return TimeDelta();
  }
  return TimeDelta::FromMicroseconds(info.duration);
}

TimeDelta SbPlayerBridge::GetStartDate() {
  DCHECK(is_url_based_);

  if (state_ == kSuspended) {
    return TimeDelta();
  }

  DCHECK(SbPlayerIsValid(player_));

  SbPlayerInfo info;
  sbplayer_interface_->GetInfo(player_, &info);
  return TimeDelta::FromMicroseconds(info.start_date);
}

void SbPlayerBridge::SetDrmSystem(SbDrmSystem drm_system) {
  DCHECK(is_url_based_);

  drm_system_ = drm_system;
  sbplayer_interface_->SetUrlPlayerDrmSystem(player_, drm_system);
}
#endif  // SB_HAS(PLAYER_WITH_URL)

void SbPlayerBridge::Suspend() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Check if the player is already suspended.
  if (state_ == kSuspended) {
    return;
  }

  DCHECK(SbPlayerIsValid(player_));

  sbplayer_interface_->SetPlaybackRate(player_, 0.0);

  set_bounds_helper_->SetPlayerBridge(NULL);

  GetInfo(&cached_video_frames_decoded_, &cached_video_frames_dropped_,
          &preroll_timestamp_);

  state_ = kSuspended;

#if COBALT_MEDIA_ENABLE_DECODE_TARGET_PROVIDER
  decode_target_provider_->SetOutputMode(
      DecodeTargetProvider::kOutputModeInvalid);
  decode_target_provider_->ResetGetCurrentSbDecodeTargetFunction();
#endif  // COBALT_MEDIA_ENABLE_DECODE_TARGET_PROVIDER

#if COBALT_MEDIA_ENABLE_CVAL
  cval_stats_->StartTimer(MediaTiming::SbPlayerDestroy, pipeline_identifier_);
#endif  // COBALT_MEDIA_ENABLE_CVAL
  sbplayer_interface_->Destroy(player_);
#if COBALT_MEDIA_ENABLE_CVAL
  cval_stats_->StopTimer(MediaTiming::SbPlayerDestroy, pipeline_identifier_);
#endif  // COBALT_MEDIA_ENABLE_CVAL

  player_ = kSbPlayerInvalid;
}

void SbPlayerBridge::Resume(SbWindow window) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  window_ = window;

  // Check if the player is already resumed.
  if (state_ != kSuspended) {
    DCHECK(SbPlayerIsValid(player_));
    return;
  }

#if COBALT_MEDIA_ENABLE_SUSPEND_RESUME
  decoder_buffer_cache_.StartResuming();
#endif  // COBALT_MEDIA_ENABLE_SUSPEND_RESUME

#if SB_HAS(PLAYER_WITH_URL)
  if (is_url_based_) {
    CreateUrlPlayer(url_);
    if (SbDrmSystemIsValid(drm_system_)) {
      sbplayer_interface_->SetUrlPlayerDrmSystem(player_, drm_system_);
    }
  } else {
    CreatePlayer();
  }
#else   // SB_HAS(PLAYER_WITH_URL)
  CreatePlayer();
#endif  // SB_HAS(PLAYER_WITH_URL)

  if (SbPlayerIsValid(player_)) {
    state_ = kResuming;
    UpdateBounds();
  }
}

#if COBALT_MEDIA_ENABLE_DECODE_TARGET_PROVIDER

namespace {

DecodeTargetProvider::OutputMode ToVideoFrameProviderOutputMode(
    SbPlayerOutputMode output_mode) {
  switch (output_mode) {
    case kSbPlayerOutputModeDecodeToTexture:
      return DecodeTargetProvider::kOutputModeDecodeToTexture;
    case kSbPlayerOutputModePunchOut:
      return DecodeTargetProvider::kOutputModePunchOut;
    case kSbPlayerOutputModeInvalid:
      return DecodeTargetProvider::kOutputModeInvalid;
  }

  NOTREACHED();
  return DecodeTargetProvider::kOutputModeInvalid;
}

}  // namespace

#endif  // COBALT_MEDIA_ENABLE_DECODE_TARGET_PROVIDER

#if SB_HAS(PLAYER_WITH_URL)
// static
void SbPlayerBridge::EncryptedMediaInitDataEncounteredCB(
    SbPlayer player,
    void* context,
    const char* init_data_type,
    const unsigned char* init_data,
    unsigned int init_data_length) {
  SbPlayerBridge* helper = static_cast<SbPlayerBridge*>(context);
  DCHECK(!helper->on_encrypted_media_init_data_encountered_cb_.is_null());
  // TODO: Use callback_helper here.
  helper->on_encrypted_media_init_data_encountered_cb_.Run(
      init_data_type, init_data, init_data_length);
}

void SbPlayerBridge::CreateUrlPlayer(const std::string& url) {
  TRACE_EVENT0("cobalt::media", "SbPlayerBridge::CreateUrlPlayer");
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  DCHECK(!on_encrypted_media_init_data_encountered_cb_.is_null());
  LOG(INFO) << "CreateUrlPlayer passed url " << url;

  if (max_video_capabilities_.empty()) {
    FormatSupportQueryMetrics::PrintAndResetMetrics();
  }

  player_creation_time_ = Time::Now();

  cval_stats_->StartTimer(MediaTiming::SbPlayerCreate, pipeline_identifier_);
  player_ = sbplayer_interface_->CreateUrlPlayer(
      url.c_str(), window_, &SbPlayerBridge::PlayerStatusCB,
      &SbPlayerBridge::EncryptedMediaInitDataEncounteredCB,
      &SbPlayerBridge::PlayerErrorCB, this);
  cval_stats_->StopTimer(MediaTiming::SbPlayerCreate, pipeline_identifier_);
  DCHECK(SbPlayerIsValid(player_));

  if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
    // If the player is setup to decode to texture, then provide Cobalt with
    // a method of querying that texture.
    decode_target_provider_->SetGetCurrentSbDecodeTargetFunction(base::Bind(
        &SbPlayerBridge::GetCurrentSbDecodeTarget, base::Unretained(this)));
    LOG(INFO) << "Playing in decode-to-texture mode.";
  } else {
    LOG(INFO) << "Playing in punch-out mode.";
  }

  decode_target_provider_->SetOutputMode(
      ToVideoFrameProviderOutputMode(output_mode_));

  set_bounds_helper_->SetPlayerBridge(this);

  UpdateBounds();
}
#endif  // SB_HAS(PLAYER_WITH_URL)

void SbPlayerBridge::CreatePlayer() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("media", "SbPlayerBridge::CreatePlayer");

#if COBALT_MEDIA_ENABLE_BACKGROUND_MODE
  bool is_visible = SbWindowIsValid(window_);
#else   // COBALT_MEDIA_ENABLE_BACKGROUND_MODE
  bool is_visible = true;
#endif  // COBALT_MEDIA_ENABLE_BACKGROUND_MODE

  is_creating_player_ = true;

  if (output_mode_ == kSbPlayerOutputModeInvalid) {
    PlayerErrorCB(kSbPlayerInvalid, this, kSbPlayerErrorDecode,
                  "Invalid output mode returned by "
                  "SbPlayerBridge::ComputeSbPlayerOutputMode()");
    is_creating_player_ = false;
    player_ = kSbPlayerInvalid;
    return;
  }

#if COBALT_MEDIA_ENABLE_FORMAT_SUPPORT_QUERY_METRICS
  if (max_video_capabilities_.empty()) {
    FormatSupportQueryMetrics::PrintAndResetMetrics();
  }
#endif  // COBALT_MEDIA_ENABLE_FORMAT_SUPPORT_QUERY_METRICS

  player_creation_time_ = Time::Now();

  SbPlayerCreationParam creation_param = {};
  creation_param.drm_system = drm_system_;
  creation_param.audio_stream_info = audio_stream_info_;
  creation_param.video_stream_info = video_stream_info_;

  // TODO: This is temporary for supporting background media playback.
  //       Need to be removed with media refactor.
  if (!is_visible) {
    creation_param.video_stream_info.codec = kSbMediaVideoCodecNone;
  }
  creation_param.output_mode = output_mode_;
  DCHECK_EQ(sbplayer_interface_->GetPreferredOutputMode(&creation_param),
            output_mode_);
#if COBALT_MEDIA_ENABLE_CVAL
  cval_stats_->StartTimer(MediaTiming::SbPlayerCreate, pipeline_identifier_);
#endif  // COBALT_MEDIA_ENABLE_CVAL
#if COBALT_MEDIA_ENABLE_PLAYER_SET_MAX_VIDEO_INPUT_SIZE
  const StarboardExtensionPlayerSetMaxVideoInputSizeApi*
      player_set_max_video_input_size_extension =
          static_cast<const StarboardExtensionPlayerSetMaxVideoInputSizeApi*>(
              SbSystemGetExtension(
                  kStarboardExtensionPlayerSetMaxVideoInputSizeName));
  if (player_set_max_video_input_size_extension &&
      strcmp(player_set_max_video_input_size_extension->name,
             kStarboardExtensionPlayerSetMaxVideoInputSizeName) == 0 &&
      player_set_max_video_input_size_extension->version >= 1) {
    player_set_max_video_input_size_extension
        ->SetMaxVideoInputSizeForCurrentThread(max_video_input_size_);
  }
#endif  // COBALT_MEDIA_ENABLE_PLAYER_SET_MAX_VIDEO_INPUT_SIZE
  const StarboardExtensionPlayerConfigurateSeekApi*
      player_configurate_seek_extension =
          static_cast<const StarboardExtensionPlayerConfigurateSeekApi*>(
              SbSystemGetExtension(
                  kStarboardExtensionPlayerConfigurateSeekName));
  if (player_configurate_seek_extension &&
      strcmp(player_configurate_seek_extension->name,
             kStarboardExtensionPlayerConfigurateSeekName) == 0 &&
      player_configurate_seek_extension->version >= 1) {
    player_configurate_seek_extension
        ->SetForceFlushDecoderDuringResetForCurrentThread(
            experimental_features_.flush_decoder_during_reset);
    player_configurate_seek_extension
        ->SetForceResetAudioDecoderForCurrentThread(
            experimental_features_.reset_audio_decoder);
  }

  const StarboardExtensionVideoDecoderConfigurationApi*
      video_decoder_configuration_extension =
          static_cast<const StarboardExtensionVideoDecoderConfigurationApi*>(
              SbSystemGetExtension(
                  kStarboardExtensionVideoDecoderConfigurationName));
  if (video_decoder_configuration_extension &&
      strcmp(video_decoder_configuration_extension->name,
             kStarboardExtensionVideoDecoderConfigurationName) == 0 &&
      video_decoder_configuration_extension->version >= 2) {
    StarboardVideoDecoderExperimentalFeatures experimental_features;
    memset(&experimental_features, 0, sizeof(experimental_features));
    if (experimental_features_.initial_max_frames_in_decoder) {
      experimental_features.initial_max_frames_in_decoder = {
          /*is_set=*/true,
          *experimental_features_.initial_max_frames_in_decoder};
    }
    if (experimental_features_.max_pending_input_frames) {
      experimental_features.max_pending_input_frames = {
          /*is_set=*/true, *experimental_features_.max_pending_input_frames};
    }
    if (experimental_features_.video_decoder_poll_interval_ms) {
      experimental_features.video_decoder_poll_interval_ms = {
          /*is_set=*/true,
          *experimental_features_.video_decoder_poll_interval_ms};
    }
    video_decoder_configuration_extension
        ->SetExperimentalFeaturesForCurrentThread(&experimental_features);
  }

#if BUILDFLAG(IS_ANDROID)
  const StarboardExtensionPlayerSetVideoSurfaceViewApi*
      player_set_video_surface_view_extension =
          static_cast<const StarboardExtensionPlayerSetVideoSurfaceViewApi*>(
              SbSystemGetExtension(
                  kStarboardExtensionPlayerSetVideoSurfaceViewName));
  if (player_set_video_surface_view_extension &&
      strcmp(player_set_video_surface_view_extension->name,
             kStarboardExtensionPlayerSetVideoSurfaceViewName) == 0 &&
      player_set_video_surface_view_extension->version >= 1) {
    player_set_video_surface_view_extension
        ->SetVideoSurfaceViewForCurrentThread(surface_view_);
  }
#endif  // BUILDFLAG(IS_ANDROID)
  player_ = sbplayer_interface_->Create(
      window_, &creation_param, &SbPlayerBridge::DeallocateSampleCB,
      &SbPlayerBridge::DecoderStatusCB, &SbPlayerBridge::PlayerStatusCB,
      &SbPlayerBridge::PlayerErrorCB, this,
#if COBALT_MEDIA_ENABLE_DECODE_TARGET_PROVIDER
      get_decode_target_graphics_context_provider_func_.Run());
#else   // COBALT_MEDIA_ENABLE_DECODE_TARGET_PROVIDER
      nullptr);
#endif  // COBALT_MEDIA_ENABLE_DECODE_TARGET_PROVIDER
#if COBALT_MEDIA_ENABLE_CVAL
  cval_stats_->StopTimer(MediaTiming::SbPlayerCreate, pipeline_identifier_);
#endif  // COBALT_MEDIA_ENABLE_CVAL

  is_creating_player_ = false;

  if (!SbPlayerIsValid(player_)) {
    return;
  }

  if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
    // If the player is setup to decode to texture, then provide Cobalt with
    // a method of querying that texture.
#if COBALT_MEDIA_ENABLE_DECODE_TARGET_PROVIDER
    decode_target_provider_->SetGetCurrentSbDecodeTargetFunction(base::Bind(
        &SbPlayerBridge::GetCurrentSbDecodeTarget, base::Unretained(this)));
#endif  // COBALT_MEDIA_ENABLE_DECODE_TARGET_PROVIDER
    LOG(INFO) << "Playing in decode-to-texture mode.";
  } else {
    LOG(INFO) << "Playing in punch-out mode.";
  }

#if COBALT_MEDIA_ENABLE_DECODE_TARGET_PROVIDER
  decode_target_provider_->SetOutputMode(
      ToVideoFrameProviderOutputMode(output_mode_));
#endif  // COBALT_MEDIA_ENABLE_DECODE_TARGET_PROVIDER
  set_bounds_helper_->SetPlayerBridge(this);

  UpdateBounds();
}

#if COBALT_MEDIA_ENABLE_SUSPEND_RESUME
void SbPlayerBridge::WriteNextBuffersFromCache(DemuxerStream::Type type,
                                               int max_buffers_per_write) {
  DCHECK(state_ != kSuspended);
#if SB_HAS(PLAYER_WITH_URL)
  DCHECK(!is_url_based_);
#endif  // SB_HAS(PLAYER_WITH_URL)

  DCHECK(SbPlayerIsValid(player_));

  if (type == DemuxerStream::AUDIO) {
    std::vector<scoped_refptr<DecoderBuffer>> buffers;
    SbMediaAudioStreamInfo stream_info;
    decoder_buffer_cache_.ReadBuffers(&buffers, max_buffers_per_write,
                                      &stream_info);
    if (buffers.size() > 0) {
      WriteBuffersInternal(type, buffers, &stream_info, nullptr);
    }
  } else {
    DCHECK_EQ(type, DemuxerStream::VIDEO);
    std::vector<scoped_refptr<DecoderBuffer>> buffers;
    SbMediaVideoStreamInfo stream_info;
    decoder_buffer_cache_.ReadBuffers(&buffers, max_buffers_per_write,
                                      &stream_info);
    if (buffers.size() > 0) {
      WriteBuffersInternal(type, buffers, nullptr, &stream_info);
    }
  }
}
#endif  // COBALT_MEDIA_ENABLE_SUSPEND_RESUME

void SbPlayerBridge::WriteBuffersInternal(
    DemuxerStream::Type type,
    const std::vector<scoped_refptr<DecoderBuffer>>& buffers,
    const SbMediaAudioStreamInfo* audio_stream_info,
    const SbMediaVideoStreamInfo* video_stream_info) {
#if SB_HAS(PLAYER_WITH_URL)
  DCHECK(!is_url_based_);
#endif  // SB_HAS(PLAYER_WITH_URL)

  auto sample_type = DemuxerStreamTypeToSbMediaType(type);
  if (buffers.size() == 1 && buffers[0]->end_of_stream()) {
    sbplayer_interface_->WriteEndOfStream(player_,
                                          DemuxerStreamTypeToSbMediaType(type));
    return;
  }

  std::vector<SbPlayerSampleInfo> gathered_sbplayer_sample_infos;
  std::vector<SbDrmSampleInfo> gathered_sbplayer_sample_infos_drm_info;
  std::vector<SbDrmSubSampleMapping>
      gathered_sbplayer_sample_infos_subsample_mapping;
  std::vector<SbPlayerSampleSideData> gathered_sbplayer_sample_infos_side_data;

  gathered_sbplayer_sample_infos.reserve(buffers.size());
  gathered_sbplayer_sample_infos_drm_info.reserve(buffers.size());
  gathered_sbplayer_sample_infos_subsample_mapping.reserve(buffers.size());
  gathered_sbplayer_sample_infos_side_data.reserve(buffers.size());

  for (int i = 0; i < static_cast<int>(buffers.size()); i++) {
    const auto& buffer = buffers[i];
    if (buffer->end_of_stream()) {
      DCHECK_EQ(static_cast<size_t>(i), buffers.size() - 1);
      if (type == DemuxerStream::AUDIO) {
        pending_audio_eos_buffer_ = true;
      } else {
        pending_video_eos_buffer_ = true;
      }
      break;
    }

    DecodingBuffers::iterator iter = decoding_buffers_.find(buffer->handle());
    if (iter == decoding_buffers_.end()) {
      decoding_buffers_[buffer->handle()] = std::make_pair(buffer, 1);
    } else {
      ++iter->second.second;
    }

    if (sample_type == kSbMediaTypeAudio &&
        first_audio_sample_time_.is_null()) {
      first_audio_sample_time_ = Time::Now();
    } else if (sample_type == kSbMediaTypeVideo &&
               first_video_sample_time_.is_null()) {
      first_video_sample_time_ = Time::Now();
    }

    gathered_sbplayer_sample_infos_drm_info.push_back(SbDrmSampleInfo());
    SbDrmSampleInfo* drm_info = &gathered_sbplayer_sample_infos_drm_info[i];

    gathered_sbplayer_sample_infos_subsample_mapping.push_back(
        SbDrmSubSampleMapping());
    SbDrmSubSampleMapping* subsample_mapping =
        &gathered_sbplayer_sample_infos_subsample_mapping[i];

    drm_info->subsample_count = 0;
    if (buffer->decrypt_config()) {
      FillDrmSampleInfo(buffer, drm_info, subsample_mapping);
    }

    gathered_sbplayer_sample_infos_side_data.push_back(
        SbPlayerSampleSideData());
    SbPlayerSampleSideData* side_data =
        &gathered_sbplayer_sample_infos_side_data[i];

    SbPlayerSampleInfo sample_info = {};
    sample_info.type = sample_type;
    // Cast the handle to void* to reuse the existing SbPlayerWriteSamples().
    sample_info.buffer = reinterpret_cast<void*>(buffer->handle());
    sample_info.buffer_size = buffer->data_size();
    sample_info.timestamp = buffer->timestamp().InMicroseconds();

    if (buffer->side_data_size() > 0) {
      // We only support at most one side data currently.
      side_data->type = kMatroskaBlockAdditional;
      side_data->data = buffer->side_data();
      side_data->size = buffer->side_data_size();
      sample_info.side_data = side_data;
      sample_info.side_data_count = 1;
    }

    if (sample_type == kSbMediaTypeAudio) {
      DCHECK(audio_stream_info);
      SetStreamInfo(*audio_stream_info, &sample_info.audio_sample_info);
      SetDiscardPadding(buffer->discard_padding(),
                        &sample_info.audio_sample_info);
    } else {
      DCHECK(sample_type == kSbMediaTypeVideo);
      DCHECK(video_stream_info);
      SetStreamInfo(*video_stream_info, &sample_info.video_sample_info);
      sample_info.video_sample_info.is_key_frame = buffer->is_key_frame();
    }
    if (drm_info->subsample_count > 0) {
      sample_info.drm_info = drm_info;
    } else {
      sample_info.drm_info = NULL;
    }
    gathered_sbplayer_sample_infos.push_back(sample_info);
  }

  if (!gathered_sbplayer_sample_infos.empty()) {
#if COBALT_MEDIA_ENABLE_CVAL
    cval_stats_->StartTimer(MediaTiming::SbPlayerWriteSamples,
                            pipeline_identifier_);
#endif  // COBALT_MEDIA_ENABLE_CVAL
    sbplayer_interface_->WriteSamples(player_, sample_type,
                                      gathered_sbplayer_sample_infos.data(),
                                      gathered_sbplayer_sample_infos.size());
#if COBALT_MEDIA_ENABLE_CVAL
    cval_stats_->StopTimer(MediaTiming::SbPlayerWriteSamples,
                           pipeline_identifier_);
#endif  // COBALT_MEDIA_ENABLE_CVAL
  }
}

SbDecodeTarget SbPlayerBridge::GetCurrentSbDecodeTarget() {
  return sbplayer_interface_->GetCurrentFrame(player_);
}

SbPlayerOutputMode SbPlayerBridge::GetSbPlayerOutputMode() {
  return output_mode_;
}

void SbPlayerBridge::GetInfo(uint32_t* video_frames_decoded,
                             uint32_t* video_frames_dropped,
                             TimeDelta* media_time) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  if (state_ == kSuspended) {
    if (video_frames_decoded) {
      *video_frames_decoded = cached_video_frames_decoded_;
    }
    if (video_frames_dropped) {
      *video_frames_dropped = cached_video_frames_dropped_;
    }
    if (media_time) {
      *media_time = preroll_timestamp_;
    }
    return;
  }

  DCHECK(SbPlayerIsValid(player_));

  SbPlayerInfo info;
  sbplayer_interface_->GetInfo(player_, &info);

  if (media_time) {
    *media_time = base::Microseconds(info.current_media_timestamp);
  }
  if (video_frames_decoded) {
    *video_frames_decoded = info.total_video_frames;
  }
  if (video_frames_dropped) {
    *video_frames_dropped = info.dropped_video_frames;
  }
}

void SbPlayerBridge::UpdateBounds() {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(SbPlayerIsValid(player_));

  if (!set_bounds_z_index_ || !set_bounds_rect_) {
    return;
  }

  auto& rect = *set_bounds_rect_;
  sbplayer_interface_->SetBounds(player_, *set_bounds_z_index_, rect.x(),
                                 rect.y(), rect.width(), rect.height());
}

void SbPlayerBridge::ClearDecoderBufferCache() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

#if COBALT_MEDIA_ENABLE_SUSPEND_RESUME
  if (state_ != kResuming) {
    TimeDelta media_time;
    GetInfo(NULL, NULL, &media_time);
    decoder_buffer_cache_.ClearSegmentsBeforeMediaTime(media_time);
  }

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&SbPlayerBridge::CallbackHelper::ClearDecoderBufferCache,
                 callback_helper_),
      TimeDelta::FromMilliseconds(kClearDecoderCacheIntervalInMilliseconds));
#endif  // COBALT_MEDIA_ENABLE_SUSPEND_RESUME
}

void SbPlayerBridge::OnDecoderStatus(SbPlayer player,
                                     SbMediaType type,
                                     SbPlayerDecoderState state,
                                     int ticket) {
#if SB_HAS(PLAYER_WITH_URL)
  DCHECK(!is_url_based_);
#endif  // SB_HAS(PLAYER_WITH_URL)
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (player_ != player || ticket != ticket_) {
    return;
  }

  DCHECK_NE(state_, kSuspended);

  switch (state) {
    case kSbPlayerDecoderStateNeedsData:
      break;
  }

  DemuxerStream::Type stream_type =
      ::media::SbMediaTypeToDemuxerStreamType(type);

  if (stream_type == DemuxerStream::AUDIO && pending_audio_eos_buffer_) {
    SbPlayerWriteEndOfStream(player_, type);
    pending_audio_eos_buffer_ = false;
    return;
  } else if (stream_type == DemuxerStream::VIDEO && pending_video_eos_buffer_) {
    SbPlayerWriteEndOfStream(player_, type);
    pending_video_eos_buffer_ = false;
    return;
  }

  auto max_number_of_samples_to_write =
      SbPlayerGetMaximumNumberOfSamplesPerWrite(player_, type);
  if (state_ == kResuming) {
#if COBALT_MEDIA_ENABLE_SUSPEND_RESUME
    if (decoder_buffer_cache_.HasMoreBuffers(stream_type)) {
      WriteNextBuffersFromCache(stream_type, max_number_of_samples_to_write);
      return;
    }
    if (!decoder_buffer_cache_.HasMoreBuffers(DemuxerStream::AUDIO) &&
        !decoder_buffer_cache_.HasMoreBuffers(DemuxerStream::VIDEO)) {
      state_ = kPlaying;
    }
#else   // COBALT_MEDIA_ENABLE_SUSPEND_RESUME
    state_ = kPlaying;
#endif  // COBALT_MEDIA_ENABLE_SUSPEND_RESUME
  }
  host_->OnNeedData(stream_type, max_number_of_samples_to_write);
}

void SbPlayerBridge::OnPlayerStatus(SbPlayer player,
                                    SbPlayerState state,
                                    int ticket) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT1("media", "SbPlayerBridge::OnPlayerStatus", "state", state);

  if (player_ != player) {
    return;
  }

  DCHECK_NE(state_, kSuspended);

  if (ticket != SB_PLAYER_INITIAL_TICKET && ticket != ticket_) {
    return;
  }

  if (state == kSbPlayerStateInitialized) {
    if (ticket_ == SB_PLAYER_INITIAL_TICKET) {
      ++ticket_;
    }
    if (sb_player_state_initialized_time_.is_null()) {
      sb_player_state_initialized_time_ = Time::Now();
    }
    // TODO(b/326497953): revisit if we need to call Seek() here once we support
    // suspend and resume.
    PrepareForSeek();
  } else if (state == kSbPlayerStatePrerolling &&
             sb_player_state_prerolling_time_.is_null()) {
    sb_player_state_prerolling_time_ = Time::Now();
  } else if (state == kSbPlayerStatePresenting &&
             sb_player_state_presenting_time_.is_null()) {
    sb_player_state_presenting_time_ = Time::Now();
#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
    LogStartupLatency();
#endif  // !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  }
  host_->OnPlayerStatus(state);
}

void SbPlayerBridge::OnPlayerError(SbPlayer player,
                                   SbPlayerError error,
                                   const std::string& message) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (player_ != player) {
    return;
  }
  host_->OnPlayerError(error, message);
}

void SbPlayerBridge::OnDeallocateSample(const void* sample_buffer) {
#if SB_HAS(PLAYER_WITH_URL)
  DCHECK(!is_url_based_);
#endif  // SB_HAS(PLAYER_WITH_URL)
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  DecodingBuffers::iterator iter = decoding_buffers_.find(
      reinterpret_cast<DecoderBuffer::Allocator::Handle>(sample_buffer));
  DCHECK(iter != decoding_buffers_.end());
  if (iter == decoding_buffers_.end()) {
    LOG(ERROR) << "SbPlayerBridge::OnDeallocateSample encounters unknown "
               << "sample_buffer " << sample_buffer;
    return;
  }
  --iter->second.second;
  if (iter->second.second == 0) {
    decoding_buffers_.erase(iter);
  }
}

bool SbPlayerBridge::TryToSetPlayerCreationErrorMessage(
    const std::string& message) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (is_creating_player_) {
    player_creation_error_message_ = message;
    return true;
  }
  LOG(INFO) << "TryToSetPlayerCreationErrorMessage() "
               "is called when |is_creating_player_| "
               "is false. Error message is ignored.";
  return false;
}

// static
void SbPlayerBridge::DecoderStatusCB(SbPlayer player,
                                     void* context,
                                     SbMediaType type,
                                     SbPlayerDecoderState state,
                                     int ticket) {
  SbPlayerBridge* helper = static_cast<SbPlayerBridge*>(context);
  helper->task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SbPlayerBridge::CallbackHelper::OnDecoderStatus,
                     helper->callback_helper_, static_cast<void*>(player), type,
                     state, ticket));
}

// static
void SbPlayerBridge::PlayerStatusCB(SbPlayer player,
                                    void* context,
                                    SbPlayerState state,
                                    int ticket) {
  SbPlayerBridge* helper = static_cast<SbPlayerBridge*>(context);
  helper->task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SbPlayerBridge::CallbackHelper::OnPlayerStatus,
                                helper->callback_helper_,
                                static_cast<void*>(player), state, ticket));
}

// static
void SbPlayerBridge::PlayerErrorCB(SbPlayer player,
                                   void* context,
                                   SbPlayerError error,
                                   const char* message) {
  SbPlayerBridge* helper = static_cast<SbPlayerBridge*>(context);
  if (player == kSbPlayerInvalid) {
    // TODO: Simplify by combining the functionality of
    // TryToSetPlayerCreationErrorMessage() with OnPlayerError().
    if (helper->TryToSetPlayerCreationErrorMessage(message)) {
      return;
    }
  }
  helper->task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SbPlayerBridge::CallbackHelper::OnPlayerError,
                     helper->callback_helper_, static_cast<void*>(player),
                     error, message ? std::string(message) : ""));
}

// static
void SbPlayerBridge::DeallocateSampleCB(SbPlayer player,
                                        void* context,
                                        const void* sample_buffer) {
  SbPlayerBridge* helper = static_cast<SbPlayerBridge*>(context);
  helper->task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SbPlayerBridge::CallbackHelper::OnDeallocateSample,
                     helper->callback_helper_, sample_buffer));
}

#if SB_HAS(PLAYER_WITH_URL)
SbPlayerOutputMode SbPlayerBridge::ComputeSbUrlPlayerOutputMode(
    SbPlayerOutputMode default_output_mode) {
  // Try to choose the output mode according to the passed in value of
  // |prefer_decode_to_texture|.  If the preferred output mode is unavailable
  // though, fallback to an output mode that is available.
  SbPlayerOutputMode output_mode = kSbPlayerOutputModeInvalid;
  if (sbplayer_interface_->GetUrlPlayerOutputModeSupported(
          kSbPlayerOutputModePunchOut)) {
    output_mode = kSbPlayerOutputModePunchOut;
  }
  if ((default_output_mode == kSbPlayerOutputModeDecodeToTexture ||
       output_mode == kSbPlayerOutputModeInvalid) &&
      sbplayer_interface_->GetUrlPlayerOutputModeSupported(
          kSbPlayerOutputModeDecodeToTexture)) {
    output_mode = kSbPlayerOutputModeDecodeToTexture;
  }
  CHECK_NE(kSbPlayerOutputModeInvalid, output_mode);

  return output_mode;
}
#endif  // SB_HAS(PLAYER_WITH_URL)

SbPlayerOutputMode SbPlayerBridge::ComputeSbPlayerOutputMode(
    SbPlayerOutputMode default_output_mode) const {
  SbPlayerCreationParam creation_param = {};
  creation_param.drm_system = drm_system_;
  creation_param.audio_stream_info = audio_stream_info_;
  creation_param.video_stream_info = video_stream_info_;

  if (default_output_mode != kSbPlayerOutputModeDecodeToTexture &&
      video_stream_info_.codec != kSbMediaVideoCodecNone) {
    DCHECK(video_stream_info_.mime);
    DCHECK(video_stream_info_.max_video_capabilities);

    // Set the `default_output_mode` to `kSbPlayerOutputModeDecodeToTexture` if
    // any of the mime associated with it has `decode-to-texture=true` set.
    // TODO(b/232559177): Make the check below more flexible, to work with
    //                    other well formed inputs (e.g. with extra space).
    bool is_decode_to_texture_preferred =
        strstr(video_stream_info_.mime, "decode-to-texture=true") ||
        strstr(video_stream_info_.max_video_capabilities,
               "decode-to-texture=true");

    if (is_decode_to_texture_preferred) {
      LOG(INFO) << "Setting `default_output_mode` from \""
                << GetPlayerOutputModeName(default_output_mode) << "\" to \""
                << GetPlayerOutputModeName(kSbPlayerOutputModeDecodeToTexture)
                << "\" because mime is set to \"" << video_stream_info_.mime
                << "\", and max_video_capabilities is set to \""
                << video_stream_info_.max_video_capabilities << "\"";
      default_output_mode = kSbPlayerOutputModeDecodeToTexture;
    }
  }

  creation_param.output_mode = default_output_mode;

  auto output_mode =
      sbplayer_interface_->GetPreferredOutputMode(&creation_param);

  LOG(INFO) << "Output mode is set to " << GetPlayerOutputModeName(output_mode);

  return output_mode;
}

void SbPlayerBridge::LogStartupLatency() const {
  std::string first_events_str;
  if (set_drm_system_ready_cb_time_.is_null()) {
    first_events_str = FormatString("%-40s0 us", "SbPlayerCreate() called");
  } else if (set_drm_system_ready_cb_time_ < player_creation_time_) {
    first_events_str = FormatString(
        "%-40s0 us\n%-40s%" PRId64 " us", "set_drm_system_ready_cb called",
        "SbPlayerCreate() called",
        (player_creation_time_ - set_drm_system_ready_cb_time_)
            .InMicroseconds());
  } else {
    first_events_str = FormatString(
        "%-40s0 us\n%-40s%" PRId64 " us", "SbPlayerCreate() called",
        "set_drm_system_ready_cb called",
        (set_drm_system_ready_cb_time_ - player_creation_time_)
            .InMicroseconds());
  }

  Time first_event_time =
      std::max(player_creation_time_, set_drm_system_ready_cb_time_);
  TimeDelta player_initialization_time_delta =
      sb_player_state_initialized_time_ - first_event_time;
  TimeDelta player_preroll_time_delta =
      sb_player_state_prerolling_time_ - sb_player_state_initialized_time_;
  TimeDelta first_audio_sample_time_delta = std::max(
      first_audio_sample_time_ - sb_player_state_prerolling_time_, TimeDelta());
  TimeDelta first_video_sample_time_delta = std::max(
      first_video_sample_time_ - sb_player_state_prerolling_time_, TimeDelta());
  TimeDelta player_presenting_time_delta =
      sb_player_state_presenting_time_ -
      std::max(first_audio_sample_time_, first_video_sample_time_);
  TimeDelta startup_latency =
      sb_player_state_presenting_time_ - first_event_time;

#if COBALT_MEDIA_ENABLE_STARTUP_LATENCY_TRACKING
  StatisticsWrapper::GetInstance()->startup_latency.AddSample(
      startup_latency.InMicroseconds(), 1);
#endif  // COBALT_MEDIA_ENABLE_STARTUP_LATENCY_TRACKING

  // clang-format off
  LOG(INFO) << FormatString(
      "\nSbPlayer startup latencies: %" PRId64 " us\n"
      "  Event name                              time since last event\n"
      "  %s\n"  // |first_events_str| populated above
      "  kSbPlayerStateInitialized received      %" PRId64 " us\n"
      "  kSbPlayerStatePrerolling received       %" PRId64 " us\n"
      "  First media sample(s) written [a/v]     %" PRId64 "/%" PRId64 " us\n"
      "  kSbPlayerStatePresenting received       %" PRId64 " us\n"
      "  Startup latency statistics (us):"
      "        min: %" PRId64 ", median: %" PRId64 ", average: %" PRId64
      ", max: %" PRId64,
      startup_latency.InMicroseconds(),
      first_events_str.c_str(), player_initialization_time_delta.InMicroseconds(),
      player_preroll_time_delta.InMicroseconds(), first_audio_sample_time_delta.InMicroseconds(),
      first_video_sample_time_delta.InMicroseconds(), player_presenting_time_delta.InMicroseconds(),
#if COBALT_MEDIA_ENABLE_STARTUP_LATENCY_TRACKING
      StatisticsWrapper::GetInstance()->startup_latency.min(),
      StatisticsWrapper::GetInstance()->startup_latency.GetMedian(),
      StatisticsWrapper::GetInstance()->startup_latency.average(),
      StatisticsWrapper::GetInstance()->startup_latency.max());
#else // COBALT_MEDIA_ENABLE_STARTUP_LATENCY_TRACKING
      static_cast<int64_t>(0),
      static_cast<int64_t>(0),
      static_cast<int64_t>(0),
      static_cast<int64_t>(0));
#endif // COBALT_MEDIA_ENABLE_STARTUP_LATENCY_TRACKING
  // clang-format on
}

void SbPlayerBridge::SendColorSpaceHistogram() const {
  using base::UmaHistogramEnumeration;

  const auto& cs_info = video_config_.color_space_info();

  if (video_stream_info_.color_metadata.bits_per_channel > 8) {
    UmaHistogramEnumeration("Cobalt.Media.HDR.Primaries", cs_info.primaries);
    UmaHistogramEnumeration("Cobalt.Media.HDR.Transfer", cs_info.transfer);
    UmaHistogramEnumeration("Cobalt.Media.HDR.Matrix", cs_info.matrix);
    UmaHistogramEnumeration("Cobalt.Media.HDR.Range", cs_info.range);
  } else {
    UmaHistogramEnumeration("Cobalt.Media.SDR.Primaries", cs_info.primaries);
    UmaHistogramEnumeration("Cobalt.Media.SDR.Transfer", cs_info.transfer);
    UmaHistogramEnumeration("Cobalt.Media.SDR.Matrix", cs_info.matrix);
    UmaHistogramEnumeration("Cobalt.Media.SDR.Range", cs_info.range);
  }
}

}  // namespace media
