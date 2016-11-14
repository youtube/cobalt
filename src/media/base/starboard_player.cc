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

#include "media/base/starboard_player.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "media/base/starboard_utils.h"

namespace media {

StarboardPlayer::StarboardPlayer(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    const AudioDecoderConfig& audio_config,
    const VideoDecoderConfig& video_config,
    SbWindow window,
    SbDrmSystem drm_system,
    Host* host,
    SbPlayerSetBoundsHelper* set_bounds_helper)
    : message_loop_(message_loop),
      window_(window),
      drm_system_(drm_system),
      host_(host),
      set_bounds_helper_(set_bounds_helper),
      weak_this_(AsWeakPtr()),
      frame_width_(1),
      frame_height_(1),
      ticket_(SB_PLAYER_INITIAL_TICKET),
      volume_(1.0),
      paused_(true),
      state_(kPlaying) {
  DCHECK(audio_config.IsValidConfig());
  DCHECK(video_config.IsValidConfig());
  DCHECK(host_);
  DCHECK(set_bounds_helper_);

  audio_config_.CopyFrom(audio_config);
  video_config_.CopyFrom(video_config);

  CreatePlayer();

  message_loop->PostTask(
      FROM_HERE,
      base::Bind(&StarboardPlayer::ClearDecoderBufferCache, weak_this_));
}

StarboardPlayer::~StarboardPlayer() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  set_bounds_helper_->SetPlayer(NULL);
  SbPlayerDestroy(player_);
}

void StarboardPlayer::UpdateVideoResolution(int frame_width, int frame_height) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  frame_width_ = frame_width;
  frame_height_ = frame_height;
}

void StarboardPlayer::WriteBuffer(DemuxerStream::Type type,
                                  const scoped_refptr<DecoderBuffer>& buffer) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // When |state_| is kPlaying, cache all buffer appended.  When |state_| is
  // kSuspended, there may still be left-over buffers appended from the pipeline
  // so they also should be cached.
  // When |state_| is resuming, all buffers come from the cache and shouldn't be
  // cached.
  if (state_ != kResuming) {
    decoder_buffer_cache_.AddBuffer(type, buffer);
  }

  if (state_ == kSuspended) {
    DCHECK(!SbPlayerIsValid(player_));
    return;
  }

  DCHECK(SbPlayerIsValid(player_));

  if (buffer->IsEndOfStream()) {
    SbPlayerWriteEndOfStream(player_, DemuxerStreamTypeToSbMediaType(type));
    return;
  }

  DecodingBuffers::iterator iter = decoding_buffers_.find(buffer->GetData());
  if (iter == decoding_buffers_.end()) {
    decoding_buffers_[buffer->GetData()] = std::make_pair(buffer, 1);
  } else {
    ++iter->second.second;
  }

  SbDrmSampleInfo drm_info;
  SbDrmSubSampleMapping subsample_mapping;
  bool is_encrypted = buffer->GetDecryptConfig();
  SbMediaVideoSampleInfo video_info;

  drm_info.subsample_count = 0;
  video_info.is_key_frame = buffer->IsKeyframe();
  video_info.frame_width = frame_width_;
  video_info.frame_height = frame_height_;

  if (is_encrypted) {
    FillDrmSampleInfo(buffer, &drm_info, &subsample_mapping);
  }
  SbPlayerWriteSample(player_, DemuxerStreamTypeToSbMediaType(type),
                      buffer->GetData(), buffer->GetDataSize(),
                      TimeDeltaToSbMediaTime(buffer->GetTimestamp()),
                      type == DemuxerStream::VIDEO ? &video_info : NULL,
                      drm_info.subsample_count > 0 ? &drm_info : NULL);
}

void StarboardPlayer::SetBounds(const gfx::Rect& rect) {
  DCHECK(SbPlayerIsValid(player_));

  SbPlayerSetBounds(player_, rect.x(), rect.y(), rect.width(), rect.height());
}

void StarboardPlayer::PrepareForSeek() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  ++ticket_;
}

void StarboardPlayer::Seek(base::TimeDelta time) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  decoder_buffer_cache_.ClearAll();

  if (state_ == kSuspended) {
    DCHECK(!SbPlayerIsValid(player_));
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
}

void StarboardPlayer::SetVolume(float volume) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  volume_ = volume;

  if (state_ == kSuspended) {
    DCHECK(!SbPlayerIsValid(player_));
    return;
  }

  DCHECK(SbPlayerIsValid(player_));
  SbPlayerSetVolume(player_, volume);
}

void StarboardPlayer::SetPause(bool pause) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(SbPlayerIsValid(player_));

  SbPlayerSetPause(player_, pause);
  paused_ = pause;
}

void StarboardPlayer::GetInfo(uint32* video_frames_decoded,
                              uint32* video_frames_dropped,
                              base::TimeDelta* media_time) {
  DCHECK(video_frames_decoded || video_frames_dropped || media_time);

  base::AutoLock auto_lock(lock_);
  if (state_ == kSuspended) {
    DCHECK(!SbPlayerIsValid(player_));

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

void StarboardPlayer::Suspend() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // Check if the player is already suspended.
  if (state_ == kSuspended) {
    DCHECK(!SbPlayerIsValid(player_));
    return;
  }

  DCHECK(SbPlayerIsValid(player_));

  SbPlayerSetPause(player_, true);

  base::AutoLock auto_lock(lock_);

  state_ = kSuspended;

  SbPlayerInfo info;
  SbPlayerGetInfo(player_, &info);
  cached_video_frames_decoded_ = info.total_video_frames;
  cached_video_frames_dropped_ = info.dropped_video_frames;
  preroll_timestamp_ = SbMediaTimeToTimeDelta(info.current_media_pts);

  set_bounds_helper_->SetPlayer(NULL);
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

  DCHECK(!SbPlayerIsValid(player_));

  decoder_buffer_cache_.StartResuming();

  CreatePlayer();

  base::AutoLock auto_lock(lock_);
  state_ = kResuming;
}

void StarboardPlayer::CreatePlayer() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  SbMediaAudioHeader audio_header;
  // TODO: Make this work with non AAC audio.
  audio_header.format_tag = 0x00ff;
  audio_header.number_of_channels =
      ChannelLayoutToChannelCount(audio_config_.channel_layout());
  audio_header.samples_per_second = audio_config_.samples_per_second();
  audio_header.average_bytes_per_second = 1;
  audio_header.block_alignment = 4;
  audio_header.bits_per_sample = audio_config_.bits_per_channel();
  audio_header.audio_specific_config_size = 0;

  SbMediaAudioCodec audio_codec =
      MediaAudioCodecToSbMediaAudioCodec(audio_config_.codec());
  SbMediaVideoCodec video_codec =
      MediaVideoCodecToSbMediaVideoCodec(video_config_.codec());

  player_ = SbPlayerCreate(window_, video_codec, audio_codec,
                           SB_PLAYER_NO_DURATION, drm_system_, &audio_header,
                           &StarboardPlayer::DeallocateSampleCB,
                           &StarboardPlayer::DecoderStatusCB,
                           &StarboardPlayer::PlayerStatusCB, this);
  set_bounds_helper_->SetPlayer(this);
}

void StarboardPlayer::ClearDecoderBufferCache() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  base::TimeDelta media_time;
  GetInfo(NULL, NULL, &media_time);
  decoder_buffer_cache_.ClearSegmentsBeforeMediaTime(media_time);

  message_loop_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&StarboardPlayer::ClearDecoderBufferCache, weak_this_),
      base::TimeDelta::FromMilliseconds(
          kClearDecoderCacheIntervalInMilliseconds));
}

void StarboardPlayer::OnDecoderStatus(SbPlayer player,
                                      SbMediaType type,
                                      SbPlayerDecoderState state,
                                      int ticket) {
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
      WriteBuffer(stream_type, decoder_buffer_cache_.GetBuffer(stream_type));
      decoder_buffer_cache_.AdvanceToNextBuffer(stream_type);
      return;
    }
    if (!decoder_buffer_cache_.GetBuffer(DemuxerStream::AUDIO) &&
        !decoder_buffer_cache_.GetBuffer(DemuxerStream::VIDEO)) {
      state_ = kPlaying;
    }
  }

  host_->OnNeedData(SbMediaTypeToDemuxerStreamType(type));
}

void StarboardPlayer::OnPlayerStatus(SbPlayer player,
                                     SbPlayerState state,
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
    if (paused_) {
      SetPause(paused_);
    }
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
void StarboardPlayer::DecoderStatusCB(SbPlayer player,
                                      void* context,
                                      SbMediaType type,
                                      SbPlayerDecoderState state,
                                      int ticket) {
  StarboardPlayer* helper = reinterpret_cast<StarboardPlayer*>(context);
  helper->message_loop_->PostTask(
      FROM_HERE, base::Bind(&StarboardPlayer::OnDecoderStatus,
                            helper->weak_this_, player, type, state, ticket));
}

// static
void StarboardPlayer::PlayerStatusCB(SbPlayer player,
                                     void* context,
                                     SbPlayerState state,
                                     int ticket) {
  StarboardPlayer* helper = reinterpret_cast<StarboardPlayer*>(context);
  helper->message_loop_->PostTask(
      FROM_HERE, base::Bind(&StarboardPlayer::OnPlayerStatus,
                            helper->weak_this_, player, state, ticket));
}

// static
void StarboardPlayer::DeallocateSampleCB(SbPlayer player,
                                         void* context,
                                         const void* sample_buffer) {
  StarboardPlayer* helper = reinterpret_cast<StarboardPlayer*>(context);
  helper->message_loop_->PostTask(
      FROM_HERE, base::Bind(&StarboardPlayer::OnDeallocateSample,
                            helper->weak_this_, sample_buffer));
}

}  // namespace media
