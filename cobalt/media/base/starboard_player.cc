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

#include "cobalt/media/base/starboard_player.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/debug/trace_event.h"
#include "base/location.h"
#include "base/logging.h"
#include "cobalt/media/base/shell_media_platform.h"
#include "cobalt/media/base/starboard_utils.h"
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
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    const std::string& url, SbWindow window, Host* host,
    SbPlayerSetBoundsHelper* set_bounds_helper, bool prefer_decode_to_texture,
    const OnEncryptedMediaInitDataEncounteredCB&
        on_encrypted_media_init_data_encountered_cb,
    VideoFrameProvider* const video_frame_provider)
    : url_(url),
      message_loop_(message_loop),
      callback_helper_(
          new CallbackHelper(ALLOW_THIS_IN_INITIALIZER_LIST(this))),
      window_(window),
      drm_system_(kSbDrmSystemInvalid),
      host_(host),
      set_bounds_helper_(set_bounds_helper),
      frame_width_(1),
      frame_height_(1),
      ticket_(SB_PLAYER_INITIAL_TICKET),
      volume_(1.0),
      playback_rate_(0.0),
      seek_pending_(false),
      state_(kPlaying),
      on_encrypted_media_init_data_encountered_cb_(
          on_encrypted_media_init_data_encountered_cb),
      video_frame_provider_(video_frame_provider) {
  DCHECK(host_);
  DCHECK(set_bounds_helper_);

  output_mode_ = ComputeSbPlayerOutputModeWithUrl(prefer_decode_to_texture);

  CreatePlayerWithUrl(url_);

  message_loop->PostTask(
      FROM_HERE,
      base::Bind(&StarboardPlayer::CallbackHelper::ClearDecoderBufferCache,
                 callback_helper_));
}
#else

StarboardPlayer::StarboardPlayer(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    const AudioDecoderConfig& audio_config,
    const VideoDecoderConfig& video_config, SbWindow window,
    SbDrmSystem drm_system, Host* host,
    SbPlayerSetBoundsHelper* set_bounds_helper, bool prefer_decode_to_texture,
    VideoFrameProvider* const video_frame_provider)
    : message_loop_(message_loop),
      callback_helper_(
          new CallbackHelper(ALLOW_THIS_IN_INITIALIZER_LIST(this))),
      audio_config_(audio_config),
      video_config_(video_config),
      window_(window),
      drm_system_(drm_system),
      host_(host),
      set_bounds_helper_(set_bounds_helper),
      frame_width_(1),
      frame_height_(1),
      ticket_(SB_PLAYER_INITIAL_TICKET),
      volume_(1.0),
      playback_rate_(0.0),
      seek_pending_(false),
      state_(kPlaying),
      video_frame_provider_(video_frame_provider) {
  DCHECK(video_config.IsValidConfig());
  DCHECK(host_);
  DCHECK(set_bounds_helper_);
  DCHECK(video_frame_provider_);

  output_mode_ = ComputeSbPlayerOutputMode(
      MediaVideoCodecToSbMediaVideoCodec(video_config.codec()), drm_system,
      prefer_decode_to_texture);

  CreatePlayer();

  message_loop->PostTask(
      FROM_HERE,
      base::Bind(&StarboardPlayer::CallbackHelper::ClearDecoderBufferCache,
                 callback_helper_));
}
#endif  // SB_HAS(PLAYER_WITH_URL)

StarboardPlayer::~StarboardPlayer() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  callback_helper_->ResetPlayer();
  set_bounds_helper_->SetPlayer(NULL);

  video_frame_provider_->SetOutputMode(VideoFrameProvider::kOutputModeInvalid);
  video_frame_provider_->ResetGetCurrentSbDecodeTargetFunction();

  if (SbPlayerIsValid(player_)) {
    SbPlayerDestroy(player_);
  }
}

void StarboardPlayer::UpdateVideoResolution(int frame_width, int frame_height) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  frame_width_ = frame_width;
  frame_height_ = frame_height;
}

void StarboardPlayer::GetVideoResolution(int* frame_width, int* frame_height) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  DCHECK(frame_width);
  DCHECK(frame_height);
  DCHECK(SbPlayerIsValid(player_));

  SbPlayerInfo out_player_info;
  SbPlayerGetInfo(player_, &out_player_info);
  frame_width_ = out_player_info.frame_width;
  frame_height_ = out_player_info.frame_height;

  *frame_width = frame_width_;
  *frame_height = frame_height_;
}

#if !SB_HAS(PLAYER_WITH_URL)

void StarboardPlayer::WriteBuffer(DemuxerStream::Type type,
                                  const scoped_refptr<DecoderBuffer>& buffer) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(buffer);

  decoder_buffer_cache_.AddBuffer(type, buffer);

  if (state_ != kSuspended) {
    WriteNextBufferFromCache(type);
  }
}

#endif  // !SB_HAS(PLAYER_WITH_URL)

void StarboardPlayer::SetBounds(int z_index, const gfx::Rect& rect) {
  base::AutoLock auto_lock(lock_);

  if (state_ == kSuspended) {
    pending_set_bounds_z_index_ = z_index;
    pending_set_bounds_rect_ = rect;
    return;
  }

  pending_set_bounds_z_index_ = base::nullopt_t();
  pending_set_bounds_rect_ = base::nullopt_t();

  DCHECK(SbPlayerIsValid(player_));
  SbPlayerSetBounds(player_, z_index, rect.x(), rect.y(), rect.width(),
                    rect.height());
}

void StarboardPlayer::PrepareForSeek() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  seek_pending_ = true;

  if (state_ == kSuspended) {
    return;
  }

  ++ticket_;
  SbPlayerSetPlaybackRate(player_, 0.f);
}

void StarboardPlayer::Seek(base::TimeDelta time) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  decoder_buffer_cache_.ClearAll();

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
  SbPlayerSeek(player_, TimeDeltaToSbMediaTime(time), ticket_);
  seek_pending_ = false;
  SbPlayerSetPlaybackRate(player_, playback_rate_);
}

void StarboardPlayer::SetVolume(float volume) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  volume_ = volume;

  if (state_ == kSuspended) {
    return;
  }

  DCHECK(SbPlayerIsValid(player_));
  SbPlayerSetVolume(player_, volume);
}

void StarboardPlayer::SetPlaybackRate(double playback_rate) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  playback_rate_ = playback_rate;

  if (state_ == kSuspended) {
    return;
  }

  if (seek_pending_) {
    return;
  }

  SbPlayerSetPlaybackRate(player_, playback_rate);
}

#if SB_HAS(PLAYER_WITH_URL)
void StarboardPlayer::GetInfo(uint32* video_frames_decoded,
                              uint32* video_frames_dropped,
                              base::TimeDelta* media_time,
                              base::TimeDelta* buffer_start_time,
                              base::TimeDelta* buffer_length_time,
                              int* frame_width, int* frame_height) {
  DCHECK(video_frames_decoded || video_frames_dropped || media_time);

  base::AutoLock auto_lock(lock_);
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
  SbPlayerGetInfo(player_, &info);
  if (video_frames_decoded) {
    *video_frames_decoded = info.total_video_frames;
  }
  if (video_frames_dropped) {
    *video_frames_dropped = info.dropped_video_frames;
  }
  if (media_time) {
    *media_time = SbMediaTimeToTimeDelta(info.current_media_pts);
  }
  if (buffer_start_time) {
    *buffer_start_time = SbMediaTimeToTimeDelta(info.buffer_start_pts);
  }
  if (buffer_length_time) {
    *buffer_length_time = SbMediaTimeToTimeDelta(info.buffer_duration_pts);
  }
  if (frame_width) {
    *frame_width = info.frame_width;
  }
  if (frame_height) {
    *frame_height = info.frame_height;
  }
}
#endif  // SB_HAS(PLAYER_WITH_URL)

void StarboardPlayer::GetInfo(uint32* video_frames_decoded,
                              uint32* video_frames_dropped,
                              base::TimeDelta* media_time) {
  DCHECK(video_frames_decoded || video_frames_dropped || media_time);

  base::AutoLock auto_lock(lock_);
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
  SbPlayerGetInfo(player_, &info);
  if (video_frames_decoded) {
    *video_frames_decoded = info.total_video_frames;
  }
  if (video_frames_dropped) {
    *video_frames_dropped = info.dropped_video_frames;
  }
  if (media_time) {
    *media_time = SbMediaTimeToTimeDelta(info.current_media_pts);
  }
}

#if SB_HAS(PLAYER_WITH_URL)
base::TimeDelta StarboardPlayer::GetDuration() {
  base::AutoLock auto_lock(lock_);
  if (state_ == kSuspended) {
    return base::TimeDelta();
  }

  DCHECK(SbPlayerIsValid(player_));

  SbPlayerInfo info;
  SbPlayerGetInfo(player_, &info);
  DCHECK_NE(info.duration_pts, SB_PLAYER_NO_DURATION);
  return SbMediaTimeToTimeDelta(info.duration_pts);
}

base::TimeDelta StarboardPlayer::GetStartDate() {
  base::AutoLock auto_lock(lock_);
  if (state_ == kSuspended) {
    return base::TimeDelta();
  }

  DCHECK(SbPlayerIsValid(player_));

  SbPlayerInfo info = {};
  SbPlayerGetInfo(player_, &info);
  return base::TimeDelta::FromMicroseconds(info.start_date);
}

void StarboardPlayer::SetDrmSystem(SbDrmSystem drm_system) {
  drm_system_ = drm_system;
  SbPlayerSetDrmSystem(player_, drm_system);
}
#endif  // SB_HAS(PLAYER_WITH_URL)

void StarboardPlayer::Suspend() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // Check if the player is already suspended.
  if (state_ == kSuspended) {
    return;
  }

  DCHECK(SbPlayerIsValid(player_));

  SbPlayerSetPlaybackRate(player_, 0.0);

  base::AutoLock auto_lock(lock_);

  state_ = kSuspended;

  SbPlayerInfo info;
  SbPlayerGetInfo(player_, &info);
  cached_video_frames_decoded_ = info.total_video_frames;
  cached_video_frames_dropped_ = info.dropped_video_frames;
  preroll_timestamp_ = SbMediaTimeToTimeDelta(info.current_media_pts);

  set_bounds_helper_->SetPlayer(NULL);
  video_frame_provider_->SetOutputMode(VideoFrameProvider::kOutputModeInvalid);
  video_frame_provider_->ResetGetCurrentSbDecodeTargetFunction();

  SbPlayerDestroy(player_);

  player_ = kSbPlayerInvalid;
}

void StarboardPlayer::Resume() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // Check if the player is already resumed.
  if (state_ != kSuspended) {
    DCHECK(SbPlayerIsValid(player_));
    return;
  }

  decoder_buffer_cache_.StartResuming();

#if SB_HAS(PLAYER_WITH_URL)
  CreatePlayerWithUrl(url_);
  if (SbDrmSystemIsValid(drm_system_)) {
    SbPlayerSetDrmSystem(player_, drm_system_);
  }
#else   // SB_HAS(PLAYER_WITH_URL)
  CreatePlayer();
#endif  // SB_HAS(PLAYER_WITH_URL)

  {
    base::AutoLock auto_lock(lock_);
    state_ = kResuming;
  }

  if (pending_set_bounds_z_index_ && pending_set_bounds_rect_) {
    SetBounds(*pending_set_bounds_z_index_, *pending_set_bounds_rect_);
    pending_set_bounds_z_index_ = base::nullopt_t();
    pending_set_bounds_rect_ = base::nullopt_t();
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
  helper->on_encrypted_media_init_data_encountered_cb_.Run(
      init_data_type, init_data, init_data_length);
}

void StarboardPlayer::CreatePlayerWithUrl(const std::string& url) {
  TRACE_EVENT0("cobalt::media", "StarboardPlayer::CreatePlayerWithUrl");
  DCHECK(message_loop_->BelongsToCurrentThread());

  DCHECK(!on_encrypted_media_init_data_encountered_cb_.is_null());
  DLOG(INFO) << "SbPlayerCreateWithUrl passed url " << url;
  player_ = SbPlayerCreateWithUrl(
      url.c_str(), window_, SB_PLAYER_NO_DURATION,
      &StarboardPlayer::PlayerStatusCB,
      &StarboardPlayer::EncryptedMediaInitDataEncounteredCB, this);
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

  if (pending_set_bounds_z_index_ && pending_set_bounds_rect_) {
    SetBounds(*pending_set_bounds_z_index_, *pending_set_bounds_rect_);
    pending_set_bounds_z_index_ = base::nullopt_t();
    pending_set_bounds_rect_ = base::nullopt_t();
  }
}

#else

void StarboardPlayer::CreatePlayer() {
  TRACE_EVENT0("cobalt::media", "StarboardPlayer::CreatePlayer");
  DCHECK(message_loop_->BelongsToCurrentThread());

  SbMediaAudioCodec audio_codec = kSbMediaAudioCodecNone;
  SbMediaAudioHeader audio_header;
  bool has_audio = audio_config_.IsValidConfig();
  if (has_audio) {
    audio_header = MediaAudioConfigToSbMediaAudioHeader(audio_config_);
    audio_codec = MediaAudioCodecToSbMediaAudioCodec(audio_config_.codec());
  }

  SbMediaVideoCodec video_codec =
      MediaVideoCodecToSbMediaVideoCodec(video_config_.codec());

  DCHECK(SbPlayerOutputModeSupported(output_mode_, video_codec, drm_system_));

  player_ = SbPlayerCreate(
      window_, video_codec, audio_codec, SB_PLAYER_NO_DURATION, drm_system_,
      has_audio ? &audio_header : NULL, &StarboardPlayer::DeallocateSampleCB,
      &StarboardPlayer::DecoderStatusCB, &StarboardPlayer::PlayerStatusCB, this,
      output_mode_,
      ShellMediaPlatform::Instance()
          ->GetSbDecodeTargetGraphicsContextProvider());
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

  if (pending_set_bounds_z_index_ && pending_set_bounds_rect_) {
    SetBounds(*pending_set_bounds_z_index_, *pending_set_bounds_rect_);
    pending_set_bounds_z_index_ = base::nullopt_t();
    pending_set_bounds_rect_ = base::nullopt_t();
  }
}

void StarboardPlayer::WriteNextBufferFromCache(DemuxerStream::Type type) {
  DCHECK(state_ != kSuspended);

  const scoped_refptr<DecoderBuffer>& buffer =
      decoder_buffer_cache_.GetBuffer(type);
  DCHECK(buffer);
  decoder_buffer_cache_.AdvanceToNextBuffer(type);

  DCHECK(SbPlayerIsValid(player_));

  if (buffer->end_of_stream()) {
    SbPlayerWriteEndOfStream(player_, DemuxerStreamTypeToSbMediaType(type));
    return;
  }

  const auto& allocations = buffer->allocations();
  DCHECK_GT(allocations.number_of_buffers(), 0);

  DecodingBuffers::iterator iter =
      decoding_buffers_.find(allocations.buffers()[0]);
  if (iter == decoding_buffers_.end()) {
    decoding_buffers_[allocations.buffers()[0]] = std::make_pair(buffer, 1);
  } else {
    ++iter->second.second;
  }

  SbDrmSampleInfo drm_info;
  SbDrmSubSampleMapping subsample_mapping;
  bool is_encrypted = buffer->decrypt_config();
  SbMediaVideoSampleInfo video_info;

  drm_info.subsample_count = 0;
  video_info.is_key_frame = buffer->is_key_frame();
  video_info.frame_width = frame_width_;
  video_info.frame_height = frame_height_;

  SbMediaColorMetadata sb_media_color_metadata =
      MediaToSbMediaColorMetadata(video_config_.webm_color_metadata());
  video_info.color_metadata = &sb_media_color_metadata;

  if (is_encrypted) {
    FillDrmSampleInfo(buffer, &drm_info, &subsample_mapping);
  }

  SbPlayerWriteSample(player_, DemuxerStreamTypeToSbMediaType(type),
#if SB_API_VERSION >= 6
                      allocations.buffers(), allocations.buffer_sizes(),
#else   // SB_API_VERSION >= 6
                      const_cast<const void**>(allocations.buffers()),
                      const_cast<int*>(allocations.buffer_sizes()),
#endif  // SB_API_VERSION >= 6
                      allocations.number_of_buffers(),
                      TimeDeltaToSbMediaTime(buffer->timestamp()),
                      type == DemuxerStream::VIDEO ? &video_info : NULL,
                      drm_info.subsample_count > 0 ? &drm_info : NULL);
}

#endif  // SB_HAS(PLAYER_WITH_URL)

SbDecodeTarget StarboardPlayer::GetCurrentSbDecodeTarget() {
  return SbPlayerGetCurrentFrame(player_);
}

SbPlayerOutputMode StarboardPlayer::GetSbPlayerOutputMode() {
  return output_mode_;
}

void StarboardPlayer::ClearDecoderBufferCache() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (state_ != kResuming) {
    base::TimeDelta media_time;
    GetInfo(NULL, NULL, &media_time);
    decoder_buffer_cache_.ClearSegmentsBeforeMediaTime(media_time);
  }

  message_loop_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&StarboardPlayer::CallbackHelper::ClearDecoderBufferCache,
                 callback_helper_),
      base::TimeDelta::FromMilliseconds(
          kClearDecoderCacheIntervalInMilliseconds));
}

void StarboardPlayer::OnDecoderStatus(SbPlayer player, SbMediaType type,
                                      SbPlayerDecoderState state, int ticket) {
// TODO: Remove decoder status related function on a broader scope when
//       PLAYER_WITH_URL is defined.
#if !SB_HAS(PLAYER_WITH_URL)
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (player_ != player || ticket != ticket_) {
    return;
  }

  DCHECK_NE(state_, kSuspended);

  switch (state) {
    case kSbPlayerDecoderStateNeedsData:
      break;
    case kSbPlayerDecoderStateBufferFull:
      DLOG(WARNING) << "kSbPlayerDecoderStateBufferFull has been deprecated.";
      return;
    case kSbPlayerDecoderStateDestroyed:
      return;
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
#endif  // !SB_HAS(PLAYER_WITH_URL)
}

void StarboardPlayer::OnPlayerStatus(SbPlayer player, SbPlayerState state,
                                     int ticket) {
  DCHECK(message_loop_->BelongsToCurrentThread());

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
    SbPlayerSeek(player_, TimeDeltaToSbMediaTime(preroll_timestamp_), ticket_);
    SetVolume(volume_);
    SbPlayerSetPlaybackRate(player_, playback_rate_);
    return;
  }
  host_->OnPlayerStatus(state);
}

void StarboardPlayer::OnDeallocateSample(const void* sample_buffer) {
  DCHECK(message_loop_->BelongsToCurrentThread());

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
  StarboardPlayer* helper = reinterpret_cast<StarboardPlayer*>(context);
  helper->message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&StarboardPlayer::CallbackHelper::OnDecoderStatus,
                 helper->callback_helper_, player, type, state, ticket));
}

// static
void StarboardPlayer::PlayerStatusCB(SbPlayer player, void* context,
                                     SbPlayerState state, int ticket) {
  StarboardPlayer* helper = reinterpret_cast<StarboardPlayer*>(context);
  helper->message_loop_->PostTask(
      FROM_HERE, base::Bind(&StarboardPlayer::CallbackHelper::OnPlayerStatus,
                            helper->callback_helper_, player, state, ticket));
}

// static
void StarboardPlayer::DeallocateSampleCB(SbPlayer player, void* context,
                                         const void* sample_buffer) {
  StarboardPlayer* helper = reinterpret_cast<StarboardPlayer*>(context);
  helper->message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&StarboardPlayer::CallbackHelper::OnDeallocateSample,
                 helper->callback_helper_, sample_buffer));
}

#if SB_HAS(PLAYER_WITH_URL)
// static
SbPlayerOutputMode StarboardPlayer::ComputeSbPlayerOutputModeWithUrl(
    bool prefer_decode_to_texture) {
  // Try to choose the output mode according to the passed in value of
  // |prefer_decode_to_texture|.  If the preferred output mode is unavailable
  // though, fallback to an output mode that is available.
  SbPlayerOutputMode output_mode = kSbPlayerOutputModeInvalid;
  if (SbPlayerOutputModeSupportedWithUrl(kSbPlayerOutputModePunchOut)) {
    output_mode = kSbPlayerOutputModePunchOut;
  }
  if ((prefer_decode_to_texture || output_mode == kSbPlayerOutputModeInvalid) &&
      SbPlayerOutputModeSupportedWithUrl(kSbPlayerOutputModeDecodeToTexture)) {
    output_mode = kSbPlayerOutputModeDecodeToTexture;
  }
  CHECK_NE(kSbPlayerOutputModeInvalid, output_mode);

#if defined(COBALT_ENABLE_LIB)
  // When Cobalt is used as a library, it doesn't control the
  // screen and so punch-out cannot be assumed.  You will need
  // to implement decode-to-texture support if you wish to use
  // Cobalt as a library and play videos with it.
  CHECK_EQ(kSbPlayerOutputModeDecodeToTexture, output_mode)
#endif  // defined(COBALT_ENABLE_LIB)

  return output_mode;
}
#else

// static
SbPlayerOutputMode StarboardPlayer::ComputeSbPlayerOutputMode(
    SbMediaVideoCodec codec, SbDrmSystem drm_system,
    bool prefer_decode_to_texture) {
  // Try to choose the output mode according to the passed in value of
  // |prefer_decode_to_texture|.  If the preferred output mode is unavailable
  // though, fallback to an output mode that is available.
  SbPlayerOutputMode output_mode = kSbPlayerOutputModeInvalid;
  if (SbPlayerOutputModeSupported(kSbPlayerOutputModePunchOut, codec,
                                  drm_system)) {
    output_mode = kSbPlayerOutputModePunchOut;
  }
  if ((prefer_decode_to_texture || output_mode == kSbPlayerOutputModeInvalid) &&
      SbPlayerOutputModeSupported(kSbPlayerOutputModeDecodeToTexture, codec,
                                  drm_system)) {
    output_mode = kSbPlayerOutputModeDecodeToTexture;
  }
  CHECK_NE(kSbPlayerOutputModeInvalid, output_mode);

#if defined(COBALT_ENABLE_LIB)
  // When Cobalt is used as a library, it doesn't control the
  // screen and so punch-out cannot be assumed.  You will need
  // to implement decode-to-texture support if you wish to use
  // Cobalt as a library and play videos with it.
  CHECK_EQ(kSbPlayerOutputModeDecodeToTexture, output_mode)
#endif  // defined(COBALT_ENABLE_LIB)

  return output_mode;
}
#endif  // SB_HAS(PLAYER_WITH_URL)
}  // namespace media
}  // namespace cobalt
