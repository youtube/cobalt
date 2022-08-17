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

#include "cobalt/media/base/sbplayer_bridge.h"

#include <algorithm>
#include <iomanip>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/statistics.h"
#include "cobalt/media/base/format_support_query_metrics.h"
#include "starboard/common/media.h"
#include "starboard/common/string.h"
#include "starboard/configuration.h"
#include "starboard/memory.h"
#include "starboard/once.h"
#include "third_party/chromium/media/base/starboard_utils.h"

namespace cobalt {
namespace media {

namespace {

class StatisticsWrapper {
 public:
  static StatisticsWrapper* GetInstance();
  base::Statistics<SbTime, int, 1024> startup_latency{
      "Media.PlaybackStartupLatency"};
};
}  // namespace
SB_ONCE_INITIALIZE_FUNCTION(StatisticsWrapper, StatisticsWrapper::GetInstance);

SbPlayerBridge::CallbackHelper::CallbackHelper(SbPlayerBridge* player_bridge)
    : player_bridge_(player_bridge) {}

void SbPlayerBridge::CallbackHelper::ClearDecoderBufferCache() {
  base::AutoLock auto_lock(lock_);
  if (player_bridge_) {
    player_bridge_->ClearDecoderBufferCache();
  }
}

void SbPlayerBridge::CallbackHelper::OnDecoderStatus(SbPlayer player,
                                                     SbMediaType type,
                                                     SbPlayerDecoderState state,
                                                     int ticket) {
  base::AutoLock auto_lock(lock_);
  if (player_bridge_) {
    player_bridge_->OnDecoderStatus(player, type, state, ticket);
  }
}

void SbPlayerBridge::CallbackHelper::OnPlayerStatus(SbPlayer player,
                                                    SbPlayerState state,
                                                    int ticket) {
  base::AutoLock auto_lock(lock_);
  if (player_bridge_) {
    player_bridge_->OnPlayerStatus(player, state, ticket);
  }
}

void SbPlayerBridge::CallbackHelper::OnPlayerError(SbPlayer player,
                                                   SbPlayerError error,
                                                   const std::string& message) {
  base::AutoLock auto_lock(lock_);
  if (player_bridge_) {
    player_bridge_->OnPlayerError(player, error, message);
  }
}

void SbPlayerBridge::CallbackHelper::OnDeallocateSample(
    const void* sample_buffer) {
  base::AutoLock auto_lock(lock_);
  if (player_bridge_) {
    player_bridge_->OnDeallocateSample(sample_buffer);
  }
}

void SbPlayerBridge::CallbackHelper::ResetPlayer() {
  base::AutoLock auto_lock(lock_);
  player_bridge_ = NULL;
}

#if SB_HAS(PLAYER_WITH_URL)
SbPlayerBridge::SbPlayerBridge(
    SbPlayerInterface* interface,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const std::string& url, SbWindow window, Host* host,
    SbPlayerSetBoundsHelper* set_bounds_helper, bool allow_resume_after_suspend,
    bool prefer_decode_to_texture,
    const OnEncryptedMediaInitDataEncounteredCB&
        on_encrypted_media_init_data_encountered_cb,
    DecodeTargetProvider* const decode_target_provider)
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
      is_url_based_(true) {
  DCHECK(host_);
  DCHECK(set_bounds_helper_);

  output_mode_ = ComputeSbUrlPlayerOutputMode(prefer_decode_to_texture);

  CreateUrlPlayer(url_);

  task_runner->PostTask(
      FROM_HERE,
      base::Bind(&SbPlayerBridge::CallbackHelper::ClearDecoderBufferCache,
                 callback_helper_));
}
#endif  // SB_HAS(PLAYER_WITH_URL)

SbPlayerBridge::SbPlayerBridge(
    SbPlayerInterface* interface,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const GetDecodeTargetGraphicsContextProviderFunc&
        get_decode_target_graphics_context_provider_func,
    const AudioDecoderConfig& audio_config, const std::string& audio_mime_type,
    const VideoDecoderConfig& video_config, const std::string& video_mime_type,
    SbWindow window, SbDrmSystem drm_system, Host* host,
    SbPlayerSetBoundsHelper* set_bounds_helper, bool allow_resume_after_suspend,
    bool prefer_decode_to_texture,
    DecodeTargetProvider* const decode_target_provider,
    const std::string& max_video_capabilities)
    : sbplayer_interface_(interface),
      task_runner_(task_runner),
      get_decode_target_graphics_context_provider_func_(
          get_decode_target_graphics_context_provider_func),
      callback_helper_(
          new CallbackHelper(ALLOW_THIS_IN_INITIALIZER_LIST(this))),
      window_(window),
      drm_system_(drm_system),
      host_(host),
      set_bounds_helper_(set_bounds_helper),
      allow_resume_after_suspend_(allow_resume_after_suspend),
      audio_config_(audio_config),
      video_config_(video_config),
      decode_target_provider_(decode_target_provider),
      max_video_capabilities_(max_video_capabilities)
#if SB_HAS(PLAYER_WITH_URL)
      ,
      is_url_based_(false)
#endif  // SB_HAS(PLAYER_WITH_URL
{
  DCHECK(!get_decode_target_graphics_context_provider_func_.is_null());
  DCHECK(audio_config.IsValidConfig() || video_config.IsValidConfig());
  DCHECK(host_);
  DCHECK(set_bounds_helper_);
  DCHECK(decode_target_provider_);

  audio_sample_info_.codec = kSbMediaAudioCodecNone;
  video_sample_info_.codec = kSbMediaVideoCodecNone;

  if (audio_config.IsValidConfig()) {
    UpdateAudioConfig(audio_config, audio_mime_type);
  }
  if (video_config.IsValidConfig()) {
    UpdateVideoConfig(video_config, video_mime_type);
  }

  output_mode_ = ComputeSbPlayerOutputMode(prefer_decode_to_texture);

  CreatePlayer();

  if (SbPlayerIsValid(player_)) {
    task_runner->PostTask(
        FROM_HERE,
        base::Bind(&SbPlayerBridge::CallbackHelper::ClearDecoderBufferCache,
                   callback_helper_));
  }
}

SbPlayerBridge::~SbPlayerBridge() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  callback_helper_->ResetPlayer();
  set_bounds_helper_->SetPlayerBridge(NULL);

  decode_target_provider_->SetOutputMode(
      DecodeTargetProvider::kOutputModeInvalid);
  decode_target_provider_->ResetGetCurrentSbDecodeTargetFunction();

  if (SbPlayerIsValid(player_)) {
    sbplayer_interface_->Destroy(player_);
  }
}

void SbPlayerBridge::UpdateAudioConfig(const AudioDecoderConfig& audio_config,
                                       const std::string& mime_type) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(audio_config.IsValidConfig());

  LOG(INFO) << "Updated AudioDecoderConfig -- "
            << audio_config.AsHumanReadableString();

  audio_config_ = audio_config;
  audio_mime_type_ = mime_type;
  audio_sample_info_ = MediaAudioConfigToSbMediaAudioSampleInfo(
      audio_config_, audio_mime_type_.c_str());
  LOG(INFO) << "Converted to SbMediaAudioSampleInfo -- " << audio_sample_info_;
}

void SbPlayerBridge::UpdateVideoConfig(const VideoDecoderConfig& video_config,
                                       const std::string& mime_type) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(video_config.IsValidConfig());

  LOG(INFO) << "Updated VideoDecoderConfig -- "
            << video_config.AsHumanReadableString();

  video_config_ = video_config;
  video_sample_info_.frame_width =
      static_cast<int>(video_config_.natural_size().width());
  video_sample_info_.frame_height =
      static_cast<int>(video_config_.natural_size().height());
  video_sample_info_.codec =
      MediaVideoCodecToSbMediaVideoCodec(video_config_.codec());
  video_sample_info_.color_metadata =
      MediaToSbMediaColorMetadata(video_config_.color_space_info(),
                                  video_config_.hdr_metadata(), mime_type);
  video_mime_type_ = mime_type;
  video_sample_info_.mime = video_mime_type_.c_str();
  video_sample_info_.max_video_capabilities = max_video_capabilities_.c_str();
  LOG(INFO) << "Converted to SbMediaVideoSampleInfo -- " << video_sample_info_;
}

void SbPlayerBridge::WriteBuffers(
    DemuxerStream::Type type,
    const std::vector<scoped_refptr<DecoderBuffer>>& buffers) {
  DCHECK(task_runner_->BelongsToCurrentThread());
#if SB_HAS(PLAYER_WITH_URL)
  DCHECK(!is_url_based_);
#endif  // SB_HAS(PLAYER_WITH_URL)

  if (allow_resume_after_suspend_) {
    for (const auto& buffer : buffers) {
      DCHECK(buffer);
      decoder_buffer_cache_.AddBuffer(type, buffer);
    }
    if (state_ != kSuspended) {
      WriteNextBuffersFromCache(type, buffers.size());
    }
    return;
  }

  WriteBuffersInternal(type, buffers);
}

void SbPlayerBridge::SetBounds(int z_index, const gfx::Rect& rect) {
  base::AutoLock auto_lock(lock_);

  set_bounds_z_index_ = z_index;
  set_bounds_rect_ = rect;

  if (state_ == kSuspended) {
    return;
  }

  UpdateBounds_Locked();
}

void SbPlayerBridge::PrepareForSeek() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  seek_pending_ = true;

  if (state_ == kSuspended) {
    return;
  }

  ++ticket_;
  sbplayer_interface_->SetPlaybackRate(player_, 0.f);
}

void SbPlayerBridge::Seek(base::TimeDelta time) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  decoder_buffer_cache_.ClearAll();
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
  sbplayer_interface_->Seek(player_, time.InMicroseconds(), ticket_);

  sbplayer_interface_->SetPlaybackRate(player_, playback_rate_);
}

void SbPlayerBridge::SetVolume(float volume) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  volume_ = volume;

  if (state_ == kSuspended) {
    return;
  }

  DCHECK(SbPlayerIsValid(player_));
  sbplayer_interface_->SetVolume(player_, volume);
}

void SbPlayerBridge::SetPlaybackRate(double playback_rate) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  playback_rate_ = playback_rate;

  if (state_ == kSuspended) {
    return;
  }

  if (seek_pending_) {
    return;
  }

  sbplayer_interface_->SetPlaybackRate(player_, playback_rate);
}

void SbPlayerBridge::GetInfo(uint32* video_frames_decoded,
                             uint32* video_frames_dropped,
                             base::TimeDelta* media_time) {
  DCHECK(video_frames_decoded || video_frames_dropped || media_time);

  base::AutoLock auto_lock(lock_);
  GetInfo_Locked(video_frames_decoded, video_frames_dropped, media_time);
}

#if SB_HAS(PLAYER_WITH_URL)
void SbPlayerBridge::GetUrlPlayerBufferedTimeRanges(
    base::TimeDelta* buffer_start_time, base::TimeDelta* buffer_length_time) {
  DCHECK(buffer_start_time || buffer_length_time);
  DCHECK(is_url_based_);

  if (state_ == kSuspended) {
    *buffer_start_time = base::TimeDelta();
    *buffer_length_time = base::TimeDelta();
    return;
  }

  DCHECK(SbPlayerIsValid(player_));

  SbUrlPlayerExtraInfo url_player_info;
  sbplayer_interface_->GetUrlPlayerExtraInfo(player_, &url_player_info);

  if (buffer_start_time) {
    *buffer_start_time = base::TimeDelta::FromMicroseconds(
        url_player_info.buffer_start_timestamp);
  }
  if (buffer_length_time) {
    *buffer_length_time =
        base::TimeDelta::FromMicroseconds(url_player_info.buffer_duration);
  }
}

void SbPlayerBridge::GetVideoResolution(int* frame_width, int* frame_height) {
  DCHECK(frame_width);
  DCHECK(frame_height);
  DCHECK(is_url_based_);

  if (state_ == kSuspended) {
    *frame_width = video_sample_info_.frame_width;
    *frame_height = video_sample_info_.frame_height;
    return;
  }

  DCHECK(SbPlayerIsValid(player_));

  SbPlayerInfo2 out_player_info;
  sbplayer_interface_->GetInfo(player_, &out_player_info);

  video_sample_info_.frame_width = out_player_info.frame_width;
  video_sample_info_.frame_height = out_player_info.frame_height;

  *frame_width = video_sample_info_.frame_width;
  *frame_height = video_sample_info_.frame_height;
}

base::TimeDelta SbPlayerBridge::GetDuration() {
  DCHECK(is_url_based_);

  if (state_ == kSuspended) {
    return base::TimeDelta();
  }

  DCHECK(SbPlayerIsValid(player_));

  SbPlayerInfo2 info;
  sbplayer_interface_->GetInfo(player_, &info);
  if (info.duration == SB_PLAYER_NO_DURATION) {
    // URL-based player may not have loaded asset yet, so map no duration to 0.
    return base::TimeDelta();
  }
  return base::TimeDelta::FromMicroseconds(info.duration);
}

base::TimeDelta SbPlayerBridge::GetStartDate() {
  DCHECK(is_url_based_);

  if (state_ == kSuspended) {
    return base::TimeDelta();
  }

  DCHECK(SbPlayerIsValid(player_));

  SbPlayerInfo2 info;
  sbplayer_interface_->GetInfo(player_, &info);
  return base::TimeDelta::FromMicroseconds(info.start_date);
}

void SbPlayerBridge::SetDrmSystem(SbDrmSystem drm_system) {
  DCHECK(is_url_based_);

  drm_system_ = drm_system;
  sbplayer_interface_->SetUrlPlayerDrmSystem(player_, drm_system);
}
#endif  // SB_HAS(PLAYER_WITH_URL)

void SbPlayerBridge::Suspend() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Check if the player is already suspended.
  if (state_ == kSuspended) {
    return;
  }

  DCHECK(SbPlayerIsValid(player_));

  sbplayer_interface_->SetPlaybackRate(player_, 0.0);

  set_bounds_helper_->SetPlayerBridge(NULL);

  base::AutoLock auto_lock(lock_);
  GetInfo_Locked(&cached_video_frames_decoded_, &cached_video_frames_dropped_,
                 &preroll_timestamp_);

  state_ = kSuspended;

  decode_target_provider_->SetOutputMode(
      DecodeTargetProvider::kOutputModeInvalid);
  decode_target_provider_->ResetGetCurrentSbDecodeTargetFunction();

  sbplayer_interface_->Destroy(player_);

  player_ = kSbPlayerInvalid;
}

void SbPlayerBridge::Resume(SbWindow window) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  window_ = window;

  // Check if the player is already resumed.
  if (state_ != kSuspended) {
    DCHECK(SbPlayerIsValid(player_));
    return;
  }

  decoder_buffer_cache_.StartResuming();

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
    base::AutoLock auto_lock(lock_);
    state_ = kResuming;
    UpdateBounds_Locked();
  }
}

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

#if SB_HAS(PLAYER_WITH_URL)
// static
void SbPlayerBridge::EncryptedMediaInitDataEncounteredCB(
    SbPlayer player, void* context, const char* init_data_type,
    const unsigned char* init_data, unsigned int init_data_length) {
  SbPlayerBridge* helper = static_cast<SbPlayerBridge*>(context);
  DCHECK(!helper->on_encrypted_media_init_data_encountered_cb_.is_null());
  // TODO: Use callback_helper here.
  helper->on_encrypted_media_init_data_encountered_cb_.Run(
      init_data_type, init_data, init_data_length);
}

void SbPlayerBridge::CreateUrlPlayer(const std::string& url) {
  TRACE_EVENT0("cobalt::media", "SbPlayerBridge::CreateUrlPlayer");
  DCHECK(task_runner_->BelongsToCurrentThread());

  DCHECK(!on_encrypted_media_init_data_encountered_cb_.is_null());
  LOG(INFO) << "CreateUrlPlayer passed url " << url;

  if (max_video_capabilities_.empty()) {
    FormatSupportQueryMetrics::PrintAndResetMetrics();
  }

  player_creation_time_ = SbTimeGetMonotonicNow();

  player_ = sbplayer_interface_->CreateUrlPlayer(
      url.c_str(), window_, &SbPlayerBridge::PlayerStatusCB,
      &SbPlayerBridge::EncryptedMediaInitDataEncounteredCB,
      &SbPlayerBridge::PlayerErrorCB, this);
  DCHECK(SbPlayerIsValid(player_));

  if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
    // If the player is setup to decode to texture, then provide Cobalt with
    // a method of querying that texture.
    decode_target_provider_->SetGetCurrentSbDecodeTargetFunction(base::Bind(
        &SbPlayerBridge::GetCurrentSbDecodeTarget, base::Unretained(this)));
  }
  decode_target_provider_->SetOutputMode(
      ToVideoFrameProviderOutputMode(output_mode_));

  set_bounds_helper_->SetPlayerBridge(this);

  base::AutoLock auto_lock(lock_);
  UpdateBounds_Locked();
}
#endif  // SB_HAS(PLAYER_WITH_URL)
void SbPlayerBridge::CreatePlayer() {
  TRACE_EVENT0("cobalt::media", "SbPlayerBridge::CreatePlayer");
  DCHECK(task_runner_->BelongsToCurrentThread());

  bool is_visible = SbWindowIsValid(window_);
  SbMediaAudioCodec audio_codec = audio_sample_info_.codec;

  bool has_audio = audio_codec != kSbMediaAudioCodecNone;

  is_creating_player_ = true;

  if (max_video_capabilities_.empty()) {
    FormatSupportQueryMetrics::PrintAndResetMetrics();
  }

  player_creation_time_ = SbTimeGetMonotonicNow();

  SbPlayerCreationParam creation_param = {};
  creation_param.drm_system = drm_system_;
  creation_param.audio_sample_info = audio_sample_info_;
  creation_param.video_sample_info = video_sample_info_;
  // TODO: This is temporary for supporting background media playback.
  //       Need to be removed with media refactor.
  if (!is_visible) {
    creation_param.video_sample_info.codec = kSbMediaVideoCodecNone;
  }
  creation_param.output_mode = output_mode_;
  DCHECK_EQ(sbplayer_interface_->GetPreferredOutputMode(&creation_param),
            output_mode_);
  player_ = sbplayer_interface_->Create(
      window_, &creation_param, &SbPlayerBridge::DeallocateSampleCB,
      &SbPlayerBridge::DecoderStatusCB, &SbPlayerBridge::PlayerStatusCB,
      &SbPlayerBridge::PlayerErrorCB, this,
      get_decode_target_graphics_context_provider_func_.Run());

  is_creating_player_ = false;

  if (!SbPlayerIsValid(player_)) {
    return;
  }

  if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
    // If the player is setup to decode to texture, then provide Cobalt with
    // a method of querying that texture.
    decode_target_provider_->SetGetCurrentSbDecodeTargetFunction(base::Bind(
        &SbPlayerBridge::GetCurrentSbDecodeTarget, base::Unretained(this)));
  }
  decode_target_provider_->SetOutputMode(
      ToVideoFrameProviderOutputMode(output_mode_));
  set_bounds_helper_->SetPlayerBridge(this);

  base::AutoLock auto_lock(lock_);
  UpdateBounds_Locked();
}

void SbPlayerBridge::WriteNextBuffersFromCache(DemuxerStream::Type type,
                                               int max_buffers_per_write) {
  DCHECK(state_ != kSuspended);
#if SB_HAS(PLAYER_WITH_URL)
  DCHECK(!is_url_based_);
#endif  // SB_HAS(PLAYER_WITH_URL)

  DCHECK(SbPlayerIsValid(player_));

  std::vector<scoped_refptr<DecoderBuffer>> buffers;
  buffers.reserve(max_buffers_per_write);

  // TODO: DecoderBufferCache doesn't respect config change during resume
  // b/243308409
  for (int i = 0; i < max_buffers_per_write; i++) {
    const scoped_refptr<DecoderBuffer>& buffer =
        decoder_buffer_cache_.GetBuffer(type);
    if (!buffer) {
      break;
    }
    decoder_buffer_cache_.AdvanceToNextBuffer(type);
    buffers.push_back(buffer);
  }

  WriteBuffersInternal(type, buffers);
}

void SbPlayerBridge::WriteBuffersInternal(
    DemuxerStream::Type type,
    const std::vector<scoped_refptr<DecoderBuffer>>& buffers) {
#if SB_HAS(PLAYER_WITH_URL)
  DCHECK(!is_url_based_);
#endif  // SB_HAS(PLAYER_WITH_URL)

  auto sample_type = DemuxerStreamTypeToSbMediaType(type);

  std::vector<SbPlayerSampleInfo> gathered_sbplayer_sample_infos;
  std::vector<SbDrmSampleInfo> gathered_sbplayer_sample_infos_drm_info;
  std::vector<SbDrmSubSampleMapping>
      gathered_sbplayer_sample_infos_subsample_mapping;
  std::vector<SbPlayerSampleSideData> gathered_sbplayer_sample_infos_side_data;

  gathered_sbplayer_sample_infos.reserve(buffers.size());
  gathered_sbplayer_sample_infos_drm_info.reserve(buffers.size());
  gathered_sbplayer_sample_infos_subsample_mapping.reserve(buffers.size());
  gathered_sbplayer_sample_infos_side_data.reserve(buffers.size());

  for (int i = 0; i < buffers.size(); i++) {
    const auto& buffer = buffers[i];
    if (buffer->end_of_stream()) {
      DCHECK_EQ(i, buffers.size() - 1);
      if (buffers.size() > 1) {
        if (type == DemuxerStream::AUDIO) {
          pending_audio_eos_buffer_ = true;
        } else {
          pending_video_eos_buffer_ = true;
        }

        DCHECK(!gathered_sbplayer_sample_infos.empty());
        sbplayer_interface_->WriteSample(player_, sample_type,
                                         gathered_sbplayer_sample_infos.data(),
                                         gathered_sbplayer_sample_infos.size());
      } else {
        sbplayer_interface_->WriteEndOfStream(
            player_, DemuxerStreamTypeToSbMediaType(type));
      }
      return;
    }

    DecodingBuffers::iterator iter = decoding_buffers_.find(buffer->data());
    if (iter == decoding_buffers_.end()) {
      decoding_buffers_[buffer->data()] = std::make_pair(buffer, 1);
    } else {
      ++iter->second.second;
    }

    if (sample_type == kSbMediaTypeAudio && first_audio_sample_time_ == 0) {
      first_audio_sample_time_ = SbTimeGetMonotonicNow();
    } else if (sample_type == kSbMediaTypeVideo &&
               first_video_sample_time_ == 0) {
      first_video_sample_time_ = SbTimeGetMonotonicNow();
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
    sample_info.buffer = buffer->data();
    sample_info.buffer_size = buffer->data_size();
    sample_info.timestamp = buffer->timestamp().InMicroseconds();

    if (buffer->side_data_size() > 0) {
      // We only support at most one side data currently.
      side_data->data = buffer->side_data();
      side_data->size = buffer->side_data_size();
      sample_info.side_data = side_data;
      sample_info.side_data_count = 1;
    }

    if (sample_type == kSbMediaTypeAudio) {
      sample_info.audio_sample_info = audio_sample_info_;
    } else {
      DCHECK(sample_type == kSbMediaTypeVideo);
      sample_info.video_sample_info = video_sample_info_;
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
    sbplayer_interface_->WriteSample(player_, sample_type,
                                     gathered_sbplayer_sample_infos.data(),
                                     gathered_sbplayer_sample_infos.size());
  }
}

SbDecodeTarget SbPlayerBridge::GetCurrentSbDecodeTarget() {
  return sbplayer_interface_->GetCurrentFrame(player_);
}

SbPlayerOutputMode SbPlayerBridge::GetSbPlayerOutputMode() {
  return output_mode_;
}

void SbPlayerBridge::GetInfo_Locked(uint32* video_frames_decoded,
                                    uint32* video_frames_dropped,
                                    base::TimeDelta* media_time) {
  lock_.AssertAcquired();
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

  SbPlayerInfo2 info;
  sbplayer_interface_->GetInfo(player_, &info);

  if (media_time) {
    *media_time =
        base::TimeDelta::FromMicroseconds(info.current_media_timestamp);
  }
  if (video_frames_decoded) {
    *video_frames_decoded = info.total_video_frames;
  }
  if (video_frames_dropped) {
    *video_frames_dropped = info.dropped_video_frames;
  }
}

void SbPlayerBridge::UpdateBounds_Locked() {
  lock_.AssertAcquired();
  DCHECK(SbPlayerIsValid(player_));

  if (!set_bounds_z_index_ || !set_bounds_rect_) {
    return;
  }

  auto& rect = *set_bounds_rect_;
  sbplayer_interface_->SetBounds(player_, *set_bounds_z_index_, rect.x(),
                                 rect.y(), rect.width(), rect.height());
}

void SbPlayerBridge::ClearDecoderBufferCache() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ != kResuming) {
    base::TimeDelta media_time;
    GetInfo(NULL, NULL, &media_time);
    decoder_buffer_cache_.ClearSegmentsBeforeMediaTime(media_time);
  }

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&SbPlayerBridge::CallbackHelper::ClearDecoderBufferCache,
                 callback_helper_),
      base::TimeDelta::FromMilliseconds(
          kClearDecoderCacheIntervalInMilliseconds));
}

void SbPlayerBridge::OnDecoderStatus(SbPlayer player, SbMediaType type,
                                     SbPlayerDecoderState state, int ticket) {
#if SB_HAS(PLAYER_WITH_URL)
  DCHECK(!is_url_based_);
#endif  // SB_HAS(PLAYER_WITH_URL)
  DCHECK(task_runner_->BelongsToCurrentThread());

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
    if (decoder_buffer_cache_.GetBuffer(stream_type)) {
      WriteNextBuffersFromCache(stream_type, max_number_of_samples_to_write);
      return;
    }
    if (!decoder_buffer_cache_.GetBuffer(DemuxerStream::AUDIO) &&
        !decoder_buffer_cache_.GetBuffer(DemuxerStream::VIDEO)) {
      state_ = kPlaying;
    }
  }

  host_->OnNeedData(stream_type, max_number_of_samples_to_write);
}

void SbPlayerBridge::OnPlayerStatus(SbPlayer player, SbPlayerState state,
                                    int ticket) {
  TRACE_EVENT1("cobalt::media", "SbPlayerBridge::OnPlayerStatus", "state",
               state);
  DCHECK(task_runner_->BelongsToCurrentThread());

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
    if (sb_player_state_initialized_time_ == 0) {
      sb_player_state_initialized_time_ = SbTimeGetMonotonicNow();
    }
    sbplayer_interface_->Seek(player_, preroll_timestamp_.InMicroseconds(),
                              ticket_);
    SetVolume(volume_);
    sbplayer_interface_->SetPlaybackRate(player_, playback_rate_);
    return;
  }
  if (state == kSbPlayerStatePrerolling &&
      sb_player_state_prerolling_time_ == 0) {
    sb_player_state_prerolling_time_ = SbTimeGetMonotonicNow();
  } else if (state == kSbPlayerStatePresenting &&
             sb_player_state_presenting_time_ == 0) {
    sb_player_state_presenting_time_ = SbTimeGetMonotonicNow();
#if !defined(COBALT_BUILD_TYPE_GOLD)
    LogStartupLatency();
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)
  }
  host_->OnPlayerStatus(state);
}

void SbPlayerBridge::OnPlayerError(SbPlayer player, SbPlayerError error,
                                   const std::string& message) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (player_ != player) {
    return;
  }
  host_->OnPlayerError(error, message);
}

void SbPlayerBridge::OnDeallocateSample(const void* sample_buffer) {
#if SB_HAS(PLAYER_WITH_URL)
  DCHECK(!is_url_based_);
#endif  // SB_HAS(PLAYER_WITH_URL)
  DCHECK(task_runner_->BelongsToCurrentThread());

  DecodingBuffers::iterator iter = decoding_buffers_.find(sample_buffer);
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
  DCHECK(task_runner_->BelongsToCurrentThread());
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
void SbPlayerBridge::DecoderStatusCB(SbPlayer player, void* context,
                                     SbMediaType type,
                                     SbPlayerDecoderState state, int ticket) {
  SbPlayerBridge* helper = static_cast<SbPlayerBridge*>(context);
  helper->task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SbPlayerBridge::CallbackHelper::OnDecoderStatus,
                 helper->callback_helper_, player, type, state, ticket));
}

// static
void SbPlayerBridge::PlayerStatusCB(SbPlayer player, void* context,
                                    SbPlayerState state, int ticket) {
  SbPlayerBridge* helper = static_cast<SbPlayerBridge*>(context);
  helper->task_runner_->PostTask(
      FROM_HERE, base::Bind(&SbPlayerBridge::CallbackHelper::OnPlayerStatus,
                            helper->callback_helper_, player, state, ticket));
}

// static
void SbPlayerBridge::PlayerErrorCB(SbPlayer player, void* context,
                                   SbPlayerError error, const char* message) {
  SbPlayerBridge* helper = static_cast<SbPlayerBridge*>(context);
  if (player == kSbPlayerInvalid) {
    // TODO: Simplify by combining the functionality of
    // TryToSetPlayerCreationErrorMessage() with OnPlayerError().
    if (helper->TryToSetPlayerCreationErrorMessage(message)) {
      return;
    }
  }
  helper->task_runner_->PostTask(
      FROM_HERE, base::Bind(&SbPlayerBridge::CallbackHelper::OnPlayerError,
                            helper->callback_helper_, player, error,
                            message ? std::string(message) : ""));
}

// static
void SbPlayerBridge::DeallocateSampleCB(SbPlayer player, void* context,
                                        const void* sample_buffer) {
  SbPlayerBridge* helper = static_cast<SbPlayerBridge*>(context);
  helper->task_runner_->PostTask(
      FROM_HERE, base::Bind(&SbPlayerBridge::CallbackHelper::OnDeallocateSample,
                            helper->callback_helper_, sample_buffer));
}

#if SB_HAS(PLAYER_WITH_URL)
SbPlayerOutputMode SbPlayerBridge::ComputeSbUrlPlayerOutputMode(
    bool prefer_decode_to_texture) {
  // Try to choose the output mode according to the passed in value of
  // |prefer_decode_to_texture|.  If the preferred output mode is unavailable
  // though, fallback to an output mode that is available.
  SbPlayerOutputMode output_mode = kSbPlayerOutputModeInvalid;
  if (sbplayer_interface_->GetUrlPlayerOutputModeSupported(
          kSbPlayerOutputModePunchOut)) {
    output_mode = kSbPlayerOutputModePunchOut;
  }
  if ((prefer_decode_to_texture || output_mode == kSbPlayerOutputModeInvalid) &&
      sbplayer_interface_->GetUrlPlayerOutputModeSupported(
          kSbPlayerOutputModeDecodeToTexture)) {
    output_mode = kSbPlayerOutputModeDecodeToTexture;
  }
  CHECK_NE(kSbPlayerOutputModeInvalid, output_mode);

  return output_mode;
}
#endif  // SB_HAS(PLAYER_WITH_URL)

SbPlayerOutputMode SbPlayerBridge::ComputeSbPlayerOutputMode(
    bool prefer_decode_to_texture) const {
  SbPlayerCreationParam creation_param = {};
  creation_param.drm_system = drm_system_;
  creation_param.audio_sample_info = audio_sample_info_;
  creation_param.video_sample_info = video_sample_info_;

  // Try to choose |kSbPlayerOutputModeDecodeToTexture| when
  // |prefer_decode_to_texture| is true.
  if (prefer_decode_to_texture) {
    creation_param.output_mode = kSbPlayerOutputModeDecodeToTexture;
  } else {
    creation_param.output_mode = kSbPlayerOutputModePunchOut;
  }
  auto output_mode =
      sbplayer_interface_->GetPreferredOutputMode(&creation_param);
  CHECK_NE(kSbPlayerOutputModeInvalid, output_mode);
  return output_mode;
}

void SbPlayerBridge::LogStartupLatency() const {
  std::string first_events_str;
  if (set_drm_system_ready_cb_time_ == -1) {
    first_events_str =
        starboard::FormatString("%-40s0 us", "SbPlayerCreate() called");
  } else if (set_drm_system_ready_cb_time_ < player_creation_time_) {
    first_events_str = starboard::FormatString(
        "%-40s0 us\n%-40s%" PRId64 " us", "set_drm_system_ready_cb called",
        "SbPlayerCreate() called",
        player_creation_time_ - set_drm_system_ready_cb_time_);
  } else {
    first_events_str = starboard::FormatString(
        "%-40s0 us\n%-40s%" PRId64 " us", "SbPlayerCreate() called",
        "set_drm_system_ready_cb called",
        set_drm_system_ready_cb_time_ - player_creation_time_);
  }

  SbTime first_event_time =
      std::max(player_creation_time_, set_drm_system_ready_cb_time_);
  SbTime player_initialization_time_delta =
      sb_player_state_initialized_time_ - first_event_time;
  SbTime player_preroll_time_delta =
      sb_player_state_prerolling_time_ - sb_player_state_initialized_time_;
  SbTime first_audio_sample_time_delta = std::max(
      first_audio_sample_time_ - sb_player_state_prerolling_time_, SbTime(0));
  SbTime first_video_sample_time_delta = std::max(
      first_video_sample_time_ - sb_player_state_prerolling_time_, SbTime(0));
  SbTime player_presenting_time_delta =
      sb_player_state_presenting_time_ -
      std::max(first_audio_sample_time_, first_video_sample_time_);
  SbTime startup_latency = sb_player_state_presenting_time_ - first_event_time;

  StatisticsWrapper::GetInstance()->startup_latency.AddSample(startup_latency,
                                                              1);

  // clang-format off
  LOG(INFO) << starboard::FormatString(
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
      startup_latency,
      first_events_str.c_str(), player_initialization_time_delta,
      player_preroll_time_delta, first_audio_sample_time_delta,
      first_video_sample_time_delta, player_presenting_time_delta,
      StatisticsWrapper::GetInstance()->startup_latency.min(),
      StatisticsWrapper::GetInstance()->startup_latency.GetMedian(),
      StatisticsWrapper::GetInstance()->startup_latency.average(),
      StatisticsWrapper::GetInstance()->startup_latency.max());
  // clang-format on
}

}  // namespace media
}  // namespace cobalt
