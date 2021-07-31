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

#include "cobalt/media/base/starboard_player.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/media/base/starboard_utils.h"
#include "starboard/common/media.h"
#include "starboard/configuration.h"
#include "starboard/memory.h"

namespace cobalt {
namespace media {

StarboardPlayer::CallbackHelper::CallbackHelper(StarboardPlayer* player)
    : player_(player) {}

void StarboardPlayer::CallbackHelper::ClearDecoderBufferCache() {
  base::AutoLock auto_lock(lock_);
  if (player_) {
    player_->ClearDecoderBufferCache();
  }
}

void StarboardPlayer::CallbackHelper::OnDecoderStatus(
    SbPlayer player, SbMediaType type, SbPlayerDecoderState state, int ticket) {
  base::AutoLock auto_lock(lock_);
  if (player_) {
    player_->OnDecoderStatus(player, type, state, ticket);
  }
}

void StarboardPlayer::CallbackHelper::OnPlayerStatus(SbPlayer player,
                                                     SbPlayerState state,
                                                     int ticket) {
  base::AutoLock auto_lock(lock_);
  if (player_) {
    player_->OnPlayerStatus(player, state, ticket);
  }
}

void StarboardPlayer::CallbackHelper::OnPlayerError(
    SbPlayer player, SbPlayerError error, const std::string& message) {
  base::AutoLock auto_lock(lock_);
  if (player_) {
    player_->OnPlayerError(player, error, message);
  }
}

void StarboardPlayer::CallbackHelper::OnDeallocateSample(
    const void* sample_buffer) {
  base::AutoLock auto_lock(lock_);
  if (player_) {
    player_->OnDeallocateSample(sample_buffer);
  }
}

void StarboardPlayer::CallbackHelper::ResetPlayer() {
  base::AutoLock auto_lock(lock_);
  player_ = NULL;
}

#if SB_HAS(PLAYER_WITH_URL)
StarboardPlayer::StarboardPlayer(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const std::string& url, SbWindow window, Host* host,
    SbPlayerSetBoundsHelper* set_bounds_helper, bool allow_resume_after_suspend,
    bool prefer_decode_to_texture,
    const OnEncryptedMediaInitDataEncounteredCB&
        on_encrypted_media_init_data_encountered_cb,
    VideoFrameProvider* const video_frame_provider)
    : url_(url),
      task_runner_(task_runner),
      callback_helper_(
          new CallbackHelper(ALLOW_THIS_IN_INITIALIZER_LIST(this))),
      window_(window),
      host_(host),
      set_bounds_helper_(set_bounds_helper),
      allow_resume_after_suspend_(allow_resume_after_suspend),
      on_encrypted_media_init_data_encountered_cb_(
          on_encrypted_media_init_data_encountered_cb),
      video_frame_provider_(video_frame_provider),
      is_url_based_(true) {
  DCHECK(host_);
  DCHECK(set_bounds_helper_);

  output_mode_ = ComputeSbUrlPlayerOutputMode(prefer_decode_to_texture);

  CreateUrlPlayer(url_);

  task_runner->PostTask(
      FROM_HERE,
      base::Bind(&StarboardPlayer::CallbackHelper::ClearDecoderBufferCache,
                 callback_helper_));
}
#endif  // SB_HAS(PLAYER_WITH_URL)

StarboardPlayer::StarboardPlayer(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const GetDecodeTargetGraphicsContextProviderFunc&
        get_decode_target_graphics_context_provider_func,
    const AudioDecoderConfig& audio_config,
    const VideoDecoderConfig& video_config, SbWindow window,
    SbDrmSystem drm_system, Host* host,
    SbPlayerSetBoundsHelper* set_bounds_helper, bool allow_resume_after_suspend,
    bool prefer_decode_to_texture,
    VideoFrameProvider* const video_frame_provider,
    const std::string& max_video_capabilities)
    : task_runner_(task_runner),
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
      video_frame_provider_(video_frame_provider),
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
  DCHECK(video_frame_provider_);

  audio_sample_info_.codec = kSbMediaAudioCodecNone;
  video_sample_info_.codec = kSbMediaVideoCodecNone;

  if (audio_config.IsValidConfig()) {
    UpdateAudioConfig(audio_config);
  }
  if (video_config.IsValidConfig()) {
    UpdateVideoConfig(video_config);
  }

  output_mode_ = ComputeSbPlayerOutputMode(prefer_decode_to_texture);

  CreatePlayer();

  if (SbPlayerIsValid(player_)) {
    task_runner->PostTask(
        FROM_HERE,
        base::Bind(&StarboardPlayer::CallbackHelper::ClearDecoderBufferCache,
                   callback_helper_));
  }
}

StarboardPlayer::~StarboardPlayer() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  callback_helper_->ResetPlayer();
  set_bounds_helper_->SetPlayer(NULL);

  video_frame_provider_->SetOutputMode(VideoFrameProvider::kOutputModeInvalid);
  video_frame_provider_->ResetGetCurrentSbDecodeTargetFunction();

  if (SbPlayerIsValid(player_)) {
    SbPlayerDestroy(player_);
  }
}

void StarboardPlayer::UpdateAudioConfig(
    const AudioDecoderConfig& audio_config) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(audio_config.IsValidConfig());

  LOG(INFO) << "Updated AudioDecoderConfig -- "
            << audio_config.AsHumanReadableString();

  audio_config_ = audio_config;
  audio_sample_info_ = MediaAudioConfigToSbMediaAudioSampleInfo(audio_config_);
  LOG(INFO) << "Converted to SbMediaAudioSampleInfo -- " << audio_sample_info_;
}

void StarboardPlayer::UpdateVideoConfig(
    const VideoDecoderConfig& video_config) {
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
      MediaToSbMediaColorMetadata(video_config_.webm_color_metadata());
#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  video_sample_info_.mime = video_config_.mime().c_str();
  video_sample_info_.max_video_capabilities = max_video_capabilities_.c_str();
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  LOG(INFO) << "Converted to SbMediaVideoSampleInfo -- " << video_sample_info_;
}

void StarboardPlayer::WriteBuffer(DemuxerStream::Type type,
                                  const scoped_refptr<DecoderBuffer>& buffer) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(buffer);
#if SB_HAS(PLAYER_WITH_URL)
  DCHECK(!is_url_based_);
#endif  // SB_HAS(PLAYER_WITH_URL)

  if (allow_resume_after_suspend_) {
    decoder_buffer_cache_.AddBuffer(type, buffer);

    if (state_ != kSuspended) {
      WriteNextBufferFromCache(type);
    }

    return;
  }
  WriteBufferInternal(type, buffer);
}

void StarboardPlayer::SetBounds(int z_index, const math::Rect& rect) {
  base::AutoLock auto_lock(lock_);

  set_bounds_z_index_ = z_index;
  set_bounds_rect_ = rect;

  if (state_ == kSuspended) {
    return;
  }

  UpdateBounds_Locked();
}

void StarboardPlayer::PrepareForSeek() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  seek_pending_ = true;

  if (state_ == kSuspended) {
    return;
  }

  ++ticket_;
  SbPlayerSetPlaybackRate(player_, 0.f);
}

void StarboardPlayer::Seek(base::TimeDelta time) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  decoder_buffer_cache_.ClearAll();
  seek_pending_ = false;

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
  SbPlayerSeek2(player_, time.InMicroseconds(), ticket_);

  SbPlayerSetPlaybackRate(player_, playback_rate_);
}

void StarboardPlayer::SetVolume(float volume) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  volume_ = volume;

  if (state_ == kSuspended) {
    return;
  }

  DCHECK(SbPlayerIsValid(player_));
  SbPlayerSetVolume(player_, volume);
}

void StarboardPlayer::SetPlaybackRate(double playback_rate) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  playback_rate_ = playback_rate;

  if (state_ == kSuspended) {
    return;
  }

  if (seek_pending_) {
    return;
  }

  SbPlayerSetPlaybackRate(player_, playback_rate);
}

void StarboardPlayer::GetInfo(uint32* video_frames_decoded,
                              uint32* video_frames_dropped,
                              base::TimeDelta* media_time) {
  DCHECK(video_frames_decoded || video_frames_dropped || media_time);

  base::AutoLock auto_lock(lock_);
  GetInfo_Locked(video_frames_decoded, video_frames_dropped, media_time);
}

#if SB_HAS(PLAYER_WITH_URL)
void StarboardPlayer::GetUrlPlayerBufferedTimeRanges(
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
  SbUrlPlayerGetExtraInfo(player_, &url_player_info);

  if (buffer_start_time) {
    *buffer_start_time = base::TimeDelta::FromMicroseconds(
        url_player_info.buffer_start_timestamp);
  }
  if (buffer_length_time) {
    *buffer_length_time =
        base::TimeDelta::FromMicroseconds(url_player_info.buffer_duration);
  }
}

void StarboardPlayer::GetVideoResolution(int* frame_width, int* frame_height) {
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
  SbPlayerGetInfo2(player_, &out_player_info);

  video_sample_info_.frame_width = out_player_info.frame_width;
  video_sample_info_.frame_height = out_player_info.frame_height;

  *frame_width = video_sample_info_.frame_width;
  *frame_height = video_sample_info_.frame_height;
}

base::TimeDelta StarboardPlayer::GetDuration() {
  DCHECK(is_url_based_);

  if (state_ == kSuspended) {
    return base::TimeDelta();
  }

  DCHECK(SbPlayerIsValid(player_));

  SbPlayerInfo2 info;
  SbPlayerGetInfo2(player_, &info);
  if (info.duration == SB_PLAYER_NO_DURATION) {
    // URL-based player may not have loaded asset yet, so map no duration to 0.
    return base::TimeDelta();
  }
  return base::TimeDelta::FromMicroseconds(info.duration);
}

base::TimeDelta StarboardPlayer::GetStartDate() {
  DCHECK(is_url_based_);

  if (state_ == kSuspended) {
    return base::TimeDelta();
  }

  DCHECK(SbPlayerIsValid(player_));

  SbPlayerInfo2 info;
  SbPlayerGetInfo2(player_, &info);
  return base::TimeDelta::FromMicroseconds(info.start_date);
}

void StarboardPlayer::SetDrmSystem(SbDrmSystem drm_system) {
  DCHECK(is_url_based_);

  drm_system_ = drm_system;
  SbUrlPlayerSetDrmSystem(player_, drm_system);
}
#endif  // SB_HAS(PLAYER_WITH_URL)

void StarboardPlayer::Suspend() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Check if the player is already suspended.
  if (state_ == kSuspended) {
    return;
  }

  DCHECK(SbPlayerIsValid(player_));

  SbPlayerSetPlaybackRate(player_, 0.0);

  set_bounds_helper_->SetPlayer(NULL);

  base::AutoLock auto_lock(lock_);
  GetInfo_Locked(&cached_video_frames_decoded_, &cached_video_frames_dropped_,
                 &preroll_timestamp_);

  state_ = kSuspended;

  video_frame_provider_->SetOutputMode(VideoFrameProvider::kOutputModeInvalid);
  video_frame_provider_->ResetGetCurrentSbDecodeTargetFunction();

  SbPlayerDestroy(player_);

  player_ = kSbPlayerInvalid;
}

void StarboardPlayer::Resume(SbWindow window) {
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
      SbUrlPlayerSetDrmSystem(player_, drm_system_);
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
VideoFrameProvider::OutputMode ToVideoFrameProviderOutputMode(
    SbPlayerOutputMode output_mode) {
  switch (output_mode) {
    case kSbPlayerOutputModeDecodeToTexture:
      return VideoFrameProvider::kOutputModeDecodeToTexture;
    case kSbPlayerOutputModePunchOut:
      return VideoFrameProvider::kOutputModePunchOut;
    case kSbPlayerOutputModeInvalid:
      return VideoFrameProvider::kOutputModeInvalid;
  }

  NOTREACHED();
  return VideoFrameProvider::kOutputModeInvalid;
}

}  // namespace

#if SB_HAS(PLAYER_WITH_URL)
// static
void StarboardPlayer::EncryptedMediaInitDataEncounteredCB(
    SbPlayer player, void* context, const char* init_data_type,
    const unsigned char* init_data, unsigned int init_data_length) {
  StarboardPlayer* helper = static_cast<StarboardPlayer*>(context);
  DCHECK(!helper->on_encrypted_media_init_data_encountered_cb_.is_null());
  // TODO: Use callback_helper here.
  helper->on_encrypted_media_init_data_encountered_cb_.Run(
      init_data_type, init_data, init_data_length);
}

void StarboardPlayer::CreateUrlPlayer(const std::string& url) {
  TRACE_EVENT0("cobalt::media", "StarboardPlayer::CreateUrlPlayer");
  DCHECK(task_runner_->BelongsToCurrentThread());

  DCHECK(!on_encrypted_media_init_data_encountered_cb_.is_null());
  LOG(INFO) << "CreateUrlPlayer passed url " << url;
  player_ =
      SbUrlPlayerCreate(url.c_str(), window_, &StarboardPlayer::PlayerStatusCB,
                        &StarboardPlayer::EncryptedMediaInitDataEncounteredCB,
                        &StarboardPlayer::PlayerErrorCB, this);
  DCHECK(SbPlayerIsValid(player_));

  if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
    // If the player is setup to decode to texture, then provide Cobalt with
    // a method of querying that texture.
    video_frame_provider_->SetGetCurrentSbDecodeTargetFunction(base::Bind(
        &StarboardPlayer::GetCurrentSbDecodeTarget, base::Unretained(this)));
  }
  video_frame_provider_->SetOutputMode(
      ToVideoFrameProviderOutputMode(output_mode_));

  set_bounds_helper_->SetPlayer(this);

  base::AutoLock auto_lock(lock_);
  UpdateBounds_Locked();
}
#endif  // SB_HAS(PLAYER_WITH_URL)
void StarboardPlayer::CreatePlayer() {
  TRACE_EVENT0("cobalt::media", "StarboardPlayer::CreatePlayer");
  DCHECK(task_runner_->BelongsToCurrentThread());

  bool is_visible = SbWindowIsValid(window_);
  SbMediaAudioCodec audio_codec = audio_sample_info_.codec;
  SbMediaVideoCodec video_codec = kSbMediaVideoCodecNone;
  // TODO: This is temporary for supporting background media playback.
  //       Need to be removed with media refactor.
  if (is_visible) {
    video_codec = video_sample_info_.codec;
  }

  bool has_audio = audio_codec != kSbMediaAudioCodecNone;

#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

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
  DCHECK_EQ(SbPlayerGetPreferredOutputMode(&creation_param), output_mode_);
  player_ = SbPlayerCreate(
      window_, &creation_param, &StarboardPlayer::DeallocateSampleCB,
      &StarboardPlayer::DecoderStatusCB, &StarboardPlayer::PlayerStatusCB,
      &StarboardPlayer::PlayerErrorCB, this,
      get_decode_target_graphics_context_provider_func_.Run());

#else  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

  DCHECK(SbPlayerOutputModeSupported(output_mode_, video_codec, drm_system_));
  // TODO: This is temporary for supporting background media playback.
  //       Need to be removed with media refactor.
  if (!is_visible) {
    DCHECK(audio_codec != kSbMediaAudioCodecNone);
  }
  player_ = SbPlayerCreate(
      window_, video_codec, audio_codec, drm_system_,
      has_audio ? &audio_sample_info_ : NULL,
      max_video_capabilities_.length() > 0 ? max_video_capabilities_.c_str()
                                           : NULL,
      &StarboardPlayer::DeallocateSampleCB, &StarboardPlayer::DecoderStatusCB,
      &StarboardPlayer::PlayerStatusCB, &StarboardPlayer::PlayerErrorCB, this,
      output_mode_, get_decode_target_graphics_context_provider_func_.Run());

#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

  if (!SbPlayerIsValid(player_)) {
    return;
  }

  if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
    // If the player is setup to decode to texture, then provide Cobalt with
    // a method of querying that texture.
    video_frame_provider_->SetGetCurrentSbDecodeTargetFunction(base::Bind(
        &StarboardPlayer::GetCurrentSbDecodeTarget, base::Unretained(this)));
  }
  video_frame_provider_->SetOutputMode(
      ToVideoFrameProviderOutputMode(output_mode_));
  set_bounds_helper_->SetPlayer(this);

  base::AutoLock auto_lock(lock_);
  UpdateBounds_Locked();
}

void StarboardPlayer::WriteNextBufferFromCache(DemuxerStream::Type type) {
  DCHECK(state_ != kSuspended);
#if SB_HAS(PLAYER_WITH_URL)
  DCHECK(!is_url_based_);
#endif  // SB_HAS(PLAYER_WITH_URL)

  const scoped_refptr<DecoderBuffer>& buffer =
      decoder_buffer_cache_.GetBuffer(type);
  DCHECK(buffer);
  decoder_buffer_cache_.AdvanceToNextBuffer(type);

  DCHECK(SbPlayerIsValid(player_));

  WriteBufferInternal(type, buffer);
}

void StarboardPlayer::WriteBufferInternal(
    DemuxerStream::Type type, const scoped_refptr<DecoderBuffer>& buffer) {
#if SB_HAS(PLAYER_WITH_URL)
  DCHECK(!is_url_based_);
#endif  // SB_HAS(PLAYER_WITH_URL)

  if (buffer->end_of_stream()) {
    SbPlayerWriteEndOfStream(player_, DemuxerStreamTypeToSbMediaType(type));
    return;
  }

  const auto& allocations = buffer->allocations();
  DCHECK_EQ(allocations.number_of_buffers(), 1);

  DecodingBuffers::iterator iter =
      decoding_buffers_.find(allocations.buffers()[0]);
  if (iter == decoding_buffers_.end()) {
    decoding_buffers_[allocations.buffers()[0]] = std::make_pair(buffer, 1);
  } else {
    ++iter->second.second;
  }

  auto sample_type = DemuxerStreamTypeToSbMediaType(type);
  SbDrmSampleInfo drm_info;
  SbDrmSubSampleMapping subsample_mapping;
  drm_info.subsample_count = 0;
  if (buffer->decrypt_config()) {
    FillDrmSampleInfo(buffer, &drm_info, &subsample_mapping);
  }

  DCHECK_GT(SbPlayerGetMaximumNumberOfSamplesPerWrite(player_, sample_type), 0);

  SbPlayerSampleSideData side_data = {};
  SbPlayerSampleInfo sample_info = {};
  sample_info.type = sample_type;
  sample_info.buffer = allocations.buffers()[0];
  sample_info.buffer_size = allocations.buffer_sizes()[0];
  sample_info.timestamp = buffer->timestamp().InMicroseconds();

  if (buffer->side_data_size() > 0) {
    // We only support at most one side data currently.
    side_data.data = buffer->side_data();
    side_data.size = buffer->side_data_size();
    sample_info.side_data = &side_data;
    sample_info.side_data_count = 1;
  }

  if (sample_type == kSbMediaTypeAudio) {
    sample_info.audio_sample_info = audio_sample_info_;
  } else {
    DCHECK(sample_type == kSbMediaTypeVideo);
    sample_info.video_sample_info = video_sample_info_;
    sample_info.video_sample_info.is_key_frame = buffer->is_key_frame();
  }
  if (drm_info.subsample_count > 0) {
    sample_info.drm_info = &drm_info;
  } else {
    sample_info.drm_info = NULL;
  }
  SbPlayerWriteSample2(player_, sample_type, &sample_info, 1);
}

SbDecodeTarget StarboardPlayer::GetCurrentSbDecodeTarget() {
  return SbPlayerGetCurrentFrame(player_);
}

SbPlayerOutputMode StarboardPlayer::GetSbPlayerOutputMode() {
  return output_mode_;
}

void StarboardPlayer::GetInfo_Locked(uint32* video_frames_decoded,
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
  SbPlayerGetInfo2(player_, &info);

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

void StarboardPlayer::UpdateBounds_Locked() {
  lock_.AssertAcquired();
  DCHECK(SbPlayerIsValid(player_));

  if (!set_bounds_z_index_ || !set_bounds_rect_) {
    return;
  }

  auto& rect = *set_bounds_rect_;
  SbPlayerSetBounds(player_, *set_bounds_z_index_, rect.x(), rect.y(),
                    rect.width(), rect.height());
}

void StarboardPlayer::ClearDecoderBufferCache() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ != kResuming) {
    base::TimeDelta media_time;
    GetInfo(NULL, NULL, &media_time);
    decoder_buffer_cache_.ClearSegmentsBeforeMediaTime(media_time);
  }

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&StarboardPlayer::CallbackHelper::ClearDecoderBufferCache,
                 callback_helper_),
      base::TimeDelta::FromMilliseconds(
          kClearDecoderCacheIntervalInMilliseconds));
}

void StarboardPlayer::OnDecoderStatus(SbPlayer player, SbMediaType type,
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
#if SB_API_VERSION < 12
    case kSbPlayerDecoderStateBufferFull:
      LOG(WARNING) << "kSbPlayerDecoderStateBufferFull has been deprecated.";
      return;
    case kSbPlayerDecoderStateDestroyed:
      LOG(WARNING) << "kSbPlayerDecoderStateDestroyed has been deprecated.";
      return;
#endif  // SB_API_VERSION < 12
  }

  if (state_ == kResuming) {
    DemuxerStream::Type stream_type = SbMediaTypeToDemuxerStreamType(type);
    if (decoder_buffer_cache_.GetBuffer(stream_type)) {
      WriteNextBufferFromCache(stream_type);
      return;
    }
    if (!decoder_buffer_cache_.GetBuffer(DemuxerStream::AUDIO) &&
        !decoder_buffer_cache_.GetBuffer(DemuxerStream::VIDEO)) {
      state_ = kPlaying;
    }
  }

  host_->OnNeedData(SbMediaTypeToDemuxerStreamType(type));
}

void StarboardPlayer::OnPlayerStatus(SbPlayer player, SbPlayerState state,
                                     int ticket) {
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
    SbPlayerSeek2(player_, preroll_timestamp_.InMicroseconds(), ticket_);
    SetVolume(volume_);
    SbPlayerSetPlaybackRate(player_, playback_rate_);
    return;
  }
  host_->OnPlayerStatus(state);
}

void StarboardPlayer::OnPlayerError(SbPlayer player, SbPlayerError error,
                                    const std::string& message) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (player_ != player) {
    return;
  }
  host_->OnPlayerError(error, message);
}

void StarboardPlayer::OnDeallocateSample(const void* sample_buffer) {
#if SB_HAS(PLAYER_WITH_URL)
  DCHECK(!is_url_based_);
#endif  // SB_HAS(PLAYER_WITH_URL)
  DCHECK(task_runner_->BelongsToCurrentThread());

  DecodingBuffers::iterator iter = decoding_buffers_.find(sample_buffer);
  DCHECK(iter != decoding_buffers_.end());
  if (iter == decoding_buffers_.end()) {
    LOG(ERROR) << "StarboardPlayer::OnDeallocateSample encounters unknown "
               << "sample_buffer " << sample_buffer;
    return;
  }
  --iter->second.second;
  if (iter->second.second == 0) {
    decoding_buffers_.erase(iter);
  }
}

// static
void StarboardPlayer::DecoderStatusCB(SbPlayer player, void* context,
                                      SbMediaType type,
                                      SbPlayerDecoderState state, int ticket) {
  StarboardPlayer* helper = static_cast<StarboardPlayer*>(context);
  helper->task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&StarboardPlayer::CallbackHelper::OnDecoderStatus,
                 helper->callback_helper_, player, type, state, ticket));
}

// static
void StarboardPlayer::PlayerStatusCB(SbPlayer player, void* context,
                                     SbPlayerState state, int ticket) {
  StarboardPlayer* helper = static_cast<StarboardPlayer*>(context);
  helper->task_runner_->PostTask(
      FROM_HERE, base::Bind(&StarboardPlayer::CallbackHelper::OnPlayerStatus,
                            helper->callback_helper_, player, state, ticket));
}

// static
void StarboardPlayer::PlayerErrorCB(SbPlayer player, void* context,
                                    SbPlayerError error, const char* message) {
  StarboardPlayer* helper = static_cast<StarboardPlayer*>(context);
  helper->task_runner_->PostTask(
      FROM_HERE, base::Bind(&StarboardPlayer::CallbackHelper::OnPlayerError,
                            helper->callback_helper_, player, error,
                            message ? std::string(message) : ""));
}

// static
void StarboardPlayer::DeallocateSampleCB(SbPlayer player, void* context,
                                         const void* sample_buffer) {
  StarboardPlayer* helper = static_cast<StarboardPlayer*>(context);
  helper->task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&StarboardPlayer::CallbackHelper::OnDeallocateSample,
                 helper->callback_helper_, sample_buffer));
}

#if SB_HAS(PLAYER_WITH_URL)
// static
SbPlayerOutputMode StarboardPlayer::ComputeSbUrlPlayerOutputMode(
    bool prefer_decode_to_texture) {
  // Try to choose the output mode according to the passed in value of
  // |prefer_decode_to_texture|.  If the preferred output mode is unavailable
  // though, fallback to an output mode that is available.
  SbPlayerOutputMode output_mode = kSbPlayerOutputModeInvalid;
  if (SbUrlPlayerOutputModeSupported(kSbPlayerOutputModePunchOut)) {
    output_mode = kSbPlayerOutputModePunchOut;
  }
  if ((prefer_decode_to_texture || output_mode == kSbPlayerOutputModeInvalid) &&
      SbUrlPlayerOutputModeSupported(kSbPlayerOutputModeDecodeToTexture)) {
    output_mode = kSbPlayerOutputModeDecodeToTexture;
  }
  CHECK_NE(kSbPlayerOutputModeInvalid, output_mode);

  return output_mode;
}
#endif  // SB_HAS(PLAYER_WITH_URL)

// static
SbPlayerOutputMode StarboardPlayer::ComputeSbPlayerOutputMode(
    bool prefer_decode_to_texture) const {
#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
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
  auto output_mode = SbPlayerGetPreferredOutputMode(&creation_param);
  CHECK_NE(kSbPlayerOutputModeInvalid, output_mode);
  return output_mode;
#else   // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  SbMediaVideoCodec video_codec = kSbMediaVideoCodecNone;

  video_codec = video_sample_info_.codec;

  // Try to choose |kSbPlayerOutputModeDecodeToTexture| when
  // |prefer_decode_to_texture| is true.
  if (prefer_decode_to_texture) {
    if (SbPlayerOutputModeSupported(kSbPlayerOutputModeDecodeToTexture,
                                    video_codec, drm_system_)) {
      return kSbPlayerOutputModeDecodeToTexture;
    }
  }

  if (SbPlayerOutputModeSupported(kSbPlayerOutputModePunchOut, video_codec,
                                  drm_system_)) {
    return kSbPlayerOutputModePunchOut;
  }
  CHECK(SbPlayerOutputModeSupported(kSbPlayerOutputModeDecodeToTexture,
                                    video_codec, drm_system_));
  return kSbPlayerOutputModeDecodeToTexture;
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
}

}  // namespace media
}  // namespace cobalt
