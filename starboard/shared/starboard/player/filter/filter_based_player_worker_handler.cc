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

#include "starboard/shared/starboard/player/filter/filter_based_player_worker_handler.h"

#include "starboard/log.h"
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

// TODO: Make this configurable inside SbPlayerCreate().
const SbTimeMonotonic kUpdateInterval = 5 * kSbTimeMillisecond;

}  // namespace

FilterBasedPlayerWorkerHandler::FilterBasedPlayerWorkerHandler(
    SbMediaVideoCodec video_codec,
    SbMediaAudioCodec audio_codec,
    SbDrmSystem drm_system,
    const SbMediaAudioHeader& audio_header,
    SbPlayerOutputMode output_mode,
    SbDecodeTargetGraphicsContextProvider* provider)
    : player_worker_(NULL),
      job_queue_(NULL),
      player_(kSbPlayerInvalid),
      update_media_time_cb_(NULL),
      get_player_state_cb_(NULL),
      update_player_state_cb_(NULL),
      video_codec_(video_codec),
      audio_codec_(audio_codec),
      drm_system_(drm_system),
      audio_header_(audio_header),
      paused_(false),
      playback_rate_(1.0),
      volume_(1.0),
      output_mode_(output_mode),
      decode_target_graphics_context_provider_(provider) {
#if SB_API_VERSION >= 6
  if (audio_header_.audio_specific_config_size > 0) {
    audio_specific_config_.reset(
        new int8_t[audio_header_.audio_specific_config_size]);
    audio_header_.audio_specific_config = audio_specific_config_.get();
    SbMemoryCopy(audio_specific_config_.get(),
                 audio_header.audio_specific_config,
                 audio_header.audio_specific_config_size);
  }
#endif  // SB_API_VERSION >= 6

  update_job_ = std::bind(&FilterBasedPlayerWorkerHandler::Update, this);
  bounds_ = PlayerWorker::Bounds();
}

bool FilterBasedPlayerWorkerHandler::IsPunchoutMode() const {
  return (output_mode_ == kSbPlayerOutputModePunchOut);
}

bool FilterBasedPlayerWorkerHandler::Init(
    PlayerWorker* player_worker,
    JobQueue* job_queue,
    SbPlayer player,
    UpdateMediaTimeCB update_media_time_cb,
    GetPlayerStateCB get_player_state_cb,
    UpdatePlayerStateCB update_player_state_cb) {
  // This function should only be called once.
  SB_DCHECK(player_worker_ == NULL);

  // All parameters have to be valid.
  SB_DCHECK(player_worker);
  SB_DCHECK(job_queue);
  SB_DCHECK(job_queue->BelongsToCurrentThread());
  SB_DCHECK(SbPlayerIsValid(player));
  SB_DCHECK(update_media_time_cb);
  SB_DCHECK(get_player_state_cb);
  SB_DCHECK(update_player_state_cb);

  player_worker_ = player_worker;
  job_queue_ = job_queue;
  player_ = player;
  update_media_time_cb_ = update_media_time_cb;
  get_player_state_cb_ = get_player_state_cb;
  update_player_state_cb_ = update_player_state_cb;

  AudioParameters audio_parameters = {audio_codec_, audio_header_, drm_system_,
                                      job_queue_};
  VideoParameters video_parameters = {
      player_,    video_codec_, drm_system_,
      job_queue_, output_mode_, decode_target_graphics_context_provider_};

  scoped_ptr<PlayerComponents> media_components =
      PlayerComponents::Create(audio_parameters, video_parameters);
  if (!media_components) {
    return false;
  }
  SB_DCHECK(media_components->is_valid());

  ::starboard::ScopedLock lock(video_renderer_existence_mutex_);
  media_components->GetRenderers(&audio_renderer_, &video_renderer_);

  audio_renderer_->SetPlaybackRate(playback_rate_);
  audio_renderer_->SetVolume(volume_);
  audio_renderer_->Initialize(
      std::bind(&FilterBasedPlayerWorkerHandler::OnError, this));
  video_renderer_->Initialize(
      std::bind(&FilterBasedPlayerWorkerHandler::OnError, this));

  update_job_token_ = job_queue_->Schedule(update_job_, kUpdateInterval);

  return true;
}

bool FilterBasedPlayerWorkerHandler::Seek(SbMediaTime seek_to_pts, int ticket) {
  SB_UNREFERENCED_PARAMETER(ticket);
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  if (!audio_renderer_ || !video_renderer_) {
    return false;
  }

  if (seek_to_pts < 0) {
    SB_DLOG(ERROR) << "Try to seek to negative timestamp " << seek_to_pts;
    seek_to_pts = 0;
  }

  audio_renderer_->Pause();
  video_renderer_->Seek(seek_to_pts);
  audio_renderer_->Seek(seek_to_pts);
  return true;
}

bool FilterBasedPlayerWorkerHandler::WriteSample(
    const scoped_refptr<InputBuffer>& input_buffer,
    bool* written) {
  SB_DCHECK(input_buffer);
  SB_DCHECK(job_queue_->BelongsToCurrentThread());
  SB_DCHECK(written != NULL);

  if (!audio_renderer_ || !video_renderer_) {
    return false;
  }

  *written = true;

  if (input_buffer->sample_type() == kSbMediaTypeAudio) {
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
      video_renderer_->WriteSample(input_buffer);
    }
  }

  return true;
}

bool FilterBasedPlayerWorkerHandler::WriteEndOfStream(SbMediaType sample_type) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  if (!audio_renderer_ || !video_renderer_) {
    return false;
  }

  if (sample_type == kSbMediaTypeAudio) {
    if (audio_renderer_->IsEndOfStreamWritten()) {
      SB_LOG(WARNING) << "Try to write audio EOS after EOS is enqueued";
    } else {
      SB_LOG(INFO) << "Audio EOS enqueued";
      audio_renderer_->WriteEndOfStream();
    }
  } else {
    if (video_renderer_->IsEndOfStreamWritten()) {
      SB_LOG(WARNING) << "Try to write video EOS after EOS is enqueued";
    } else {
      SB_LOG(INFO) << "Video EOS enqueued";
      video_renderer_->WriteEndOfStream();
    }
  }

  return true;
}

bool FilterBasedPlayerWorkerHandler::SetPause(bool pause) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  if (!audio_renderer_) {
    return false;
  }

  paused_ = pause;

  if (pause) {
    audio_renderer_->Pause();
    SB_DLOG(INFO) << "Playback paused.";
  } else {
    audio_renderer_->Play();
    SB_DLOG(INFO) << "Playback started.";
  }

  return true;
}

bool FilterBasedPlayerWorkerHandler::SetPlaybackRate(double playback_rate) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  playback_rate_ = playback_rate;

  if (!audio_renderer_) {
    return false;
  }

  audio_renderer_->SetPlaybackRate(playback_rate_);
  return true;
}

void FilterBasedPlayerWorkerHandler::SetVolume(double volume) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  volume_ = volume;
  if (audio_renderer_) {
    audio_renderer_->SetVolume(volume_);
  }
}

bool FilterBasedPlayerWorkerHandler::SetBounds(
    const PlayerWorker::Bounds& bounds) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

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

void FilterBasedPlayerWorkerHandler::OnError() {
  if (!job_queue_->BelongsToCurrentThread()) {
    job_queue_->Schedule(
        std::bind(&FilterBasedPlayerWorkerHandler::OnError, this));
    return;
  }

  (*player_worker_.*update_player_state_cb_)(kSbPlayerStateError);
}

// TODO: This should be driven by callbacks instead polling.
void FilterBasedPlayerWorkerHandler::Update() {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  if (!audio_renderer_ || !video_renderer_) {
    return;
  }

  if ((*player_worker_.*get_player_state_cb_)() == kSbPlayerStatePrerolling) {
    if (!audio_renderer_->IsSeekingInProgress() &&
        !video_renderer_->IsSeekingInProgress()) {
      (*player_worker_.*update_player_state_cb_)(kSbPlayerStatePresenting);
      if (!paused_) {
        audio_renderer_->Play();
      }
    }
  }

  if ((*player_worker_.*get_player_state_cb_)() == kSbPlayerStatePresenting) {
    if (audio_renderer_->IsEndOfStreamPlayed() &&
        video_renderer_->IsEndOfStreamPlayed()) {
      (*player_worker_.*update_player_state_cb_)(kSbPlayerStateEndOfStream);
    }

    player_worker_->UpdateDroppedVideoFrames(
        video_renderer_->GetDroppedFrames());
    bool is_playing;
    bool is_eos_played;
    (*player_worker_.*update_media_time_cb_)(
        audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played));
  }

  update_job_token_ = job_queue_->Schedule(update_job_, kUpdateInterval);
}

void FilterBasedPlayerWorkerHandler::Stop() {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  job_queue_->RemoveJobByToken(update_job_token_);

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
}

SbDecodeTarget FilterBasedPlayerWorkerHandler::GetCurrentDecodeTarget() {
  ::starboard::ScopedLock lock(video_renderer_existence_mutex_);

  if (audio_renderer_ && video_renderer_) {
    return video_renderer_->GetCurrentDecodeTarget();
  } else {
    return kSbDecodeTargetInvalid;
  }
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
