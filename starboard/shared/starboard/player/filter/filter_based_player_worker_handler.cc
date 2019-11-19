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

#include "starboard/shared/starboard/player/filter/filter_based_player_worker_handler.h"

#include "starboard/audio_sink.h"
#include "starboard/common/log.h"
#include "starboard/memory.h"
#include "starboard/shared/starboard/drm/drm_system_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/player_components.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

namespace {

typedef MediaTimeProviderImpl::MonotonicSystemTimeProvider
    MonotonicSystemTimeProvider;

using std::placeholders::_1;
using std::placeholders::_2;

// TODO: Make this configurable inside SbPlayerCreate().
const SbTimeMonotonic kUpdateInterval = 200 * kSbTimeMillisecond;

class MonotonicSystemTimeProviderImpl : public MonotonicSystemTimeProvider {
  SbTimeMonotonic GetMonotonicNow() const override {
    return SbTimeGetMonotonicNow();
  }
};

}  // namespace

FilterBasedPlayerWorkerHandler::FilterBasedPlayerWorkerHandler(
    SbMediaVideoCodec video_codec,
    SbMediaAudioCodec audio_codec,
    SbDrmSystem drm_system,
    const SbMediaAudioSampleInfo* audio_sample_info,
    SbPlayerOutputMode output_mode,
    SbDecodeTargetGraphicsContextProvider* provider)
    : JobOwner(kDetached),
      video_codec_(video_codec),
      audio_codec_(audio_codec),
      drm_system_(drm_system),
      output_mode_(output_mode),
      decode_target_graphics_context_provider_(provider) {
  if (audio_codec != kSbMediaAudioCodecNone) {
    audio_sample_info_ = *audio_sample_info;

    if (audio_sample_info_.audio_specific_config_size > 0) {
      audio_specific_config_.reset(
          new int8_t[audio_sample_info_.audio_specific_config_size]);
      SbMemoryCopy(audio_specific_config_.get(),
                   audio_sample_info->audio_specific_config,
                   audio_sample_info->audio_specific_config_size);
      audio_sample_info_.audio_specific_config = audio_specific_config_.get();
    }
  }

  update_job_ = std::bind(&FilterBasedPlayerWorkerHandler::Update, this);
  bounds_ = PlayerWorker::Bounds();
}

bool FilterBasedPlayerWorkerHandler::IsPunchoutMode() const {
  return (output_mode_ == kSbPlayerOutputModePunchOut);
}

bool FilterBasedPlayerWorkerHandler::Init(
    SbPlayer player,
    UpdateMediaInfoCB update_media_info_cb,
    GetPlayerStateCB get_player_state_cb,
    UpdatePlayerStateCB update_player_state_cb,
    UpdatePlayerErrorCB update_player_error_cb) {
  // This function should only be called once.
  SB_DCHECK(update_media_info_cb_ == NULL);

  // All parameters have to be valid.
  SB_DCHECK(SbPlayerIsValid(player));
  SB_DCHECK(update_media_info_cb);
  SB_DCHECK(get_player_state_cb);
  SB_DCHECK(update_player_state_cb);

  AttachToCurrentThread();

  player_ = player;
  update_media_info_cb_ = update_media_info_cb;
  get_player_state_cb_ = get_player_state_cb;
  update_player_state_cb_ = update_player_state_cb;
#if SB_HAS(PLAYER_ERROR_MESSAGE)
  update_player_error_cb_ = update_player_error_cb;
#else   // SB_HAS(PLAYER_ERROR_MESSAGE)
  SB_DCHECK(!update_player_error_cb);
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)

  scoped_ptr<PlayerComponents> player_components = PlayerComponents::Create();
  SB_DCHECK(player_components);

  if (audio_codec_ != kSbMediaAudioCodecNone) {
    // TODO: This is not ideal as we should really handle the creation failure
    // of audio sink inside the audio renderer to give the renderer a chance to
    // resample the decoded audio.
    const int required_audio_channels = audio_sample_info_.number_of_channels;
    const int supported_audio_channels = SbAudioSinkGetMaxChannels();
    if (required_audio_channels > supported_audio_channels) {
      SB_LOG(ERROR) << "Audio channels requested " << required_audio_channels
                    << ", but currently supported less than or equal to "
                    << supported_audio_channels;
      return false;
    }

    PlayerComponents::AudioParameters audio_parameters = {
        audio_codec_, audio_sample_info_, drm_system_};

    audio_renderer_ = player_components->CreateAudioRenderer(audio_parameters);
    if (!audio_renderer_) {
      SB_DLOG(ERROR) << "Failed to create audio renderer";
      return false;
    }
    audio_renderer_->Initialize(
#if SB_HAS(PLAYER_ERROR_MESSAGE)
        std::bind(&FilterBasedPlayerWorkerHandler::OnError, this, _1, _2),
#else   // SB_HAS(PLAYER_ERROR_MESSAGE)
        std::bind(&FilterBasedPlayerWorkerHandler::OnError, this),
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)
        std::bind(&FilterBasedPlayerWorkerHandler::OnPrerolled, this,
                  kSbMediaTypeAudio),
        std::bind(&FilterBasedPlayerWorkerHandler::OnEnded, this,
                  kSbMediaTypeAudio));
    audio_renderer_->SetPlaybackRate(playback_rate_);
    audio_renderer_->SetVolume(volume_);
  } else {
    media_time_provider_impl_.reset(
        new MediaTimeProviderImpl(scoped_ptr<MonotonicSystemTimeProvider>(
            new MonotonicSystemTimeProviderImpl)));
    media_time_provider_impl_->Initialize(
#if SB_HAS(PLAYER_ERROR_MESSAGE)
        std::bind(&FilterBasedPlayerWorkerHandler::OnError, this, _1, _2),
#else   // SB_HAS(PLAYER_ERROR_MESSAGE)
        std::bind(&FilterBasedPlayerWorkerHandler::OnError, this),
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)
        std::bind(&FilterBasedPlayerWorkerHandler::OnPrerolled, this,
                  kSbMediaTypeAudio),
        std::bind(&FilterBasedPlayerWorkerHandler::OnEnded, this,
                  kSbMediaTypeAudio));
    media_time_provider_impl_->SetPlaybackRate(playback_rate_);
  }

  if (video_codec_ != kSbMediaVideoCodecNone) {
    PlayerComponents::VideoParameters video_parameters = {
        player_, video_codec_, drm_system_, output_mode_,
        decode_target_graphics_context_provider_};

    ::starboard::ScopedLock lock(video_renderer_existence_mutex_);

    auto media_time_provider = GetMediaTimeProvider();
    SB_DCHECK(media_time_provider);

    video_renderer_ = player_components->CreateVideoRenderer(
        video_parameters, media_time_provider);
    if (!video_renderer_) {
      SB_DLOG(ERROR) << "Failed to create video renderer";
      return false;
    }
    video_renderer_->Initialize(
#if SB_HAS(PLAYER_ERROR_MESSAGE)
        std::bind(&FilterBasedPlayerWorkerHandler::OnError, this, _1, _2),
#else   // SB_HAS(PLAYER_ERROR_MESSAGE)
        std::bind(&FilterBasedPlayerWorkerHandler::OnError, this),
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)
        std::bind(&FilterBasedPlayerWorkerHandler::OnPrerolled, this,
                  kSbMediaTypeVideo),
        std::bind(&FilterBasedPlayerWorkerHandler::OnEnded, this,
                  kSbMediaTypeVideo));
  }

  update_job_token_ = Schedule(update_job_, kUpdateInterval);

  return true;
}

bool FilterBasedPlayerWorkerHandler::Seek(SbTime seek_to_time, int ticket) {
  SB_UNREFERENCED_PARAMETER(ticket);
  SB_DCHECK(BelongsToCurrentThread());

  if (!GetMediaTimeProvider()) {
    return false;
  }

  if (seek_to_time < 0) {
    SB_DLOG(ERROR) << "Try to seek to negative timestamp " << seek_to_time;
    seek_to_time = 0;
  }

  GetMediaTimeProvider()->Pause();
  if (video_renderer_) {
    video_renderer_->Seek(seek_to_time);
  }
  GetMediaTimeProvider()->Seek(seek_to_time);
  audio_prerolled_ = false;
  video_prerolled_ = false;
  return true;
}

bool FilterBasedPlayerWorkerHandler::WriteSample(
    const scoped_refptr<InputBuffer>& input_buffer,
    bool* written) {
  SB_DCHECK(input_buffer);
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(written != NULL);

  if (input_buffer->sample_type() == kSbMediaTypeAudio) {
    if (!audio_renderer_) {
      return false;
    }

    *written = true;

    if (audio_renderer_->IsEndOfStreamWritten()) {
      SB_LOG(WARNING) << "Try to write audio sample after EOS is reached";
    } else {
      if (!audio_renderer_->CanAcceptMoreData()) {
        *written = false;
        return true;
      }

      if (input_buffer->drm_info()) {
        if (!SbDrmSystemIsValid(drm_system_)) {
          return false;
        }
        SbDrmSystemPrivate::DecryptStatus decrypt_status =
            drm_system_->Decrypt(input_buffer);
        if (decrypt_status == SbDrmSystemPrivate::kRetry) {
          *written = false;
          return true;
        }
        if (decrypt_status == SbDrmSystemPrivate::kFailure) {
          *written = false;
          return false;
        }
      }
      audio_renderer_->WriteSample(input_buffer);
    }
  } else {
    SB_DCHECK(input_buffer->sample_type() == kSbMediaTypeVideo);

    if (!video_renderer_) {
      return false;
    }

    *written = true;

    if (video_renderer_->IsEndOfStreamWritten()) {
      SB_LOG(WARNING) << "Try to write video sample after EOS is reached";
    } else {
      if (!video_renderer_->CanAcceptMoreData()) {
        *written = false;
        return true;
      }
      if (input_buffer->drm_info()) {
        if (!SbDrmSystemIsValid(drm_system_)) {
          return false;
        }
        SbDrmSystemPrivate::DecryptStatus decrypt_status =
            drm_system_->Decrypt(input_buffer);
        if (decrypt_status == SbDrmSystemPrivate::kRetry) {
          *written = false;
          return true;
        }
        if (decrypt_status == SbDrmSystemPrivate::kFailure) {
          *written = false;
          return false;
        }
      }
      if (media_time_provider_impl_) {
        media_time_provider_impl_->UpdateVideoDuration(
            input_buffer->timestamp());
      }
      video_renderer_->WriteSample(input_buffer);
    }
  }

  return true;
}

bool FilterBasedPlayerWorkerHandler::WriteEndOfStream(SbMediaType sample_type) {
  SB_DCHECK(BelongsToCurrentThread());

  if (sample_type == kSbMediaTypeAudio) {
    if (!audio_renderer_) {
      return false;
    }
    if (audio_renderer_->IsEndOfStreamWritten()) {
      SB_LOG(WARNING) << "Try to write audio EOS after EOS is enqueued";
    } else {
      SB_LOG(INFO) << "Audio EOS enqueued";
      audio_renderer_->WriteEndOfStream();
    }
  } else {
    if (!video_renderer_) {
      return false;
    }
    if (video_renderer_->IsEndOfStreamWritten()) {
      SB_LOG(WARNING) << "Try to write video EOS after EOS is enqueued";
    } else {
      SB_LOG(INFO) << "Video EOS enqueued";
      if (media_time_provider_impl_) {
        media_time_provider_impl_->VideoEndOfStreamReached();
      }
      video_renderer_->WriteEndOfStream();
    }
  }

  return true;
}

bool FilterBasedPlayerWorkerHandler::SetPause(bool pause) {
  SB_DCHECK(BelongsToCurrentThread());

  if (!GetMediaTimeProvider()) {
    return false;
  }

  paused_ = pause;

  if (pause) {
    GetMediaTimeProvider()->Pause();
    SB_DLOG(INFO) << "Playback paused.";
  } else {
    GetMediaTimeProvider()->Play();
    SB_DLOG(INFO) << "Playback started.";
  }

  return true;
}

bool FilterBasedPlayerWorkerHandler::SetPlaybackRate(double playback_rate) {
  SB_DCHECK(BelongsToCurrentThread());

  playback_rate_ = playback_rate;

  if (!GetMediaTimeProvider()) {
    return false;
  }

  GetMediaTimeProvider()->SetPlaybackRate(playback_rate_);
  return true;
}

void FilterBasedPlayerWorkerHandler::SetVolume(double volume) {
  SB_DCHECK(BelongsToCurrentThread());

  volume_ = volume;
  if (audio_renderer_) {
    audio_renderer_->SetVolume(volume_);
  }
}

bool FilterBasedPlayerWorkerHandler::SetBounds(
    const PlayerWorker::Bounds& bounds) {
  SB_DCHECK(BelongsToCurrentThread());

  if (SbMemoryCompare(&bounds_, &bounds, sizeof(bounds_)) != 0) {
    bounds_ = bounds;
    if (video_renderer_) {
      // TODO: Force a frame update
      video_renderer_->SetBounds(bounds.z_index, bounds.x, bounds.y,
                                 bounds.width, bounds.height);
    }
  }

  return true;
}

#if SB_HAS(PLAYER_ERROR_MESSAGE)
void FilterBasedPlayerWorkerHandler::OnError(SbPlayerError error,
                                             const std::string& error_message) {
  if (!BelongsToCurrentThread()) {
    Schedule(std::bind(&FilterBasedPlayerWorkerHandler::OnError, this, error,
                       error_message));
    return;
  }

  if (update_player_error_cb_) {
    update_player_error_cb_(error, error_message.empty()
                                       ? "FilterBasedPlayerWorkerHandler error."
                                       : error_message);
  }
}
#else   // SB_HAS(PLAYER_ERROR_MESSAGE)
void FilterBasedPlayerWorkerHandler::OnError() {
  if (!BelongsToCurrentThread()) {
    Schedule(std::bind(&FilterBasedPlayerWorkerHandler::OnError, this));
    return;
  }

  update_player_state_cb_(kSbPlayerStateError);
}
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)

void FilterBasedPlayerWorkerHandler::OnPrerolled(SbMediaType media_type) {
  if (!BelongsToCurrentThread()) {
    Schedule(std::bind(&FilterBasedPlayerWorkerHandler::OnPrerolled, this,
                       media_type));
    return;
  }

  SB_DCHECK(get_player_state_cb_() == kSbPlayerStatePrerolling)
      << "Invalid player state " << get_player_state_cb_();

  audio_prerolled_ |= media_type == kSbMediaTypeAudio;
  video_prerolled_ |= media_type == kSbMediaTypeVideo;

  if (audio_prerolled_ && (!video_renderer_ || video_prerolled_)) {
    update_player_state_cb_(kSbPlayerStatePresenting);
    if (!paused_) {
      GetMediaTimeProvider()->Play();
    }
  }
}

void FilterBasedPlayerWorkerHandler::OnEnded(SbMediaType media_type) {
  if (!BelongsToCurrentThread()) {
    Schedule(
        std::bind(&FilterBasedPlayerWorkerHandler::OnEnded, this, media_type));
    return;
  }
  audio_ended_ |= media_type == kSbMediaTypeAudio;
  video_ended_ |= media_type == kSbMediaTypeVideo;

  if (audio_ended_ && (!video_renderer_ || video_ended_)) {
    update_player_state_cb_(kSbPlayerStateEndOfStream);
  }
}

void FilterBasedPlayerWorkerHandler::Update() {
  SB_DCHECK(BelongsToCurrentThread());

  if (!GetMediaTimeProvider()) {
    return;
  }

  if (get_player_state_cb_() == kSbPlayerStatePresenting) {
    int dropped_frames = 0;
    if (video_renderer_) {
      dropped_frames = video_renderer_->GetDroppedFrames();
    }
    bool is_playing;
    bool is_eos_played;
    bool is_underflow;
    auto media_time = GetMediaTimeProvider()->GetCurrentMediaTime(
        &is_playing, &is_eos_played, &is_underflow);
    update_media_info_cb_(media_time, dropped_frames, is_underflow);
  }

  update_job_token_ = Schedule(update_job_, kUpdateInterval);
}

void FilterBasedPlayerWorkerHandler::Stop() {
  SB_DCHECK(BelongsToCurrentThread());

  RemoveJobByToken(update_job_token_);

  scoped_ptr<VideoRenderer> video_renderer;
  {
    // Set |video_renderer_| to null with the lock, but we actually destroy
    // it outside of the lock.  This is because the VideoRenderer destructor
    // may post a task to destroy the SbDecodeTarget to the same thread that
    // might call GetCurrentDecodeTarget(), which would try to take this lock.
    ::starboard::ScopedLock lock(video_renderer_existence_mutex_);
    video_renderer = video_renderer_.Pass();
  }
  video_renderer.reset();
  audio_renderer_.reset();
  media_time_provider_impl_.reset();
}

SbDecodeTarget FilterBasedPlayerWorkerHandler::GetCurrentDecodeTarget() {
  SbDecodeTarget decode_target = kSbDecodeTargetInvalid;
  if (video_renderer_existence_mutex_.AcquireTry()) {
    if (video_renderer_) {
      decode_target = video_renderer_->GetCurrentDecodeTarget();
    }
    video_renderer_existence_mutex_.Release();
  }
  return decode_target;
}

MediaTimeProvider* FilterBasedPlayerWorkerHandler::GetMediaTimeProvider()
    const {
  if (audio_renderer_) {
    return audio_renderer_.get();
  }
  return media_time_provider_impl_.get();
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
